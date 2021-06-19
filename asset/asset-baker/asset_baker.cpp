#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>

#include "json.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_resize.h"

#include "asset_loader.h"
#include "texture_asset.h"
#include "mesh_asset.h"
#include "vk_mesh.h"

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/quaternion.hpp"


namespace fs = std::filesystem;
using namespace assets;

struct ConverterState {
	fs::path asset_path;
	fs::path export_path;

	fs::path convertToExportRelative(fs::path path) const;
};

bool convertImage(const fs::path& input, const fs::path& output)
{
	int texWidth, texHeight, texChannels;

	auto pngStart{ std::chrono::high_resolution_clock::now() };

	stbi_uc* pixels{ stbi_load(input.u8string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha) };

	auto pngEnd{ std::chrono::high_resolution_clock::now() };

	auto diff = pngEnd - pngStart;

	std::cout << "png took " << std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() / 1000000.0 << "ms" << std::endl;

	if (!pixels) {
		std::cout << "Failed to load texture file " << input << "\n";
		return false;
	}

	int texture_size{ texWidth * texHeight * 4 };

	TextureInfo texinfo;
	texinfo.originalSize = texture_size;


	std::string s{ input.filename().generic_string() };
	bool colorTexture{ s.size() >= 9 && (s.substr(s.size() - 9, 5) == "_diff") };

	texinfo.textureFormat = colorTexture ? TextureFormat::SRGBA8 : TextureFormat::RGBA8;
	texinfo.originalFile = input.string();

	auto mipStart{ std::chrono::high_resolution_clock::now() };

	texinfo.width = texWidth;
	texinfo.height = texHeight;

	std::vector<unsigned char> allBuffer;


	size_t allBufferSize{ (size_t)texture_size };
	uint32_t width{ texinfo.width };
	uint32_t height{ texinfo.height };
	uint32_t miplevels{ 1 };

	// make mipmaps
	while (width > 1 || height > 1) {
		width >>= 1;
		height >>= 1;
		allBufferSize += (size_t)width * height * 4;
		++miplevels;
	}

	texinfo.miplevels = miplevels;
	allBuffer.resize(allBufferSize);
	width = texinfo.width;
	height = texinfo.height;
	size_t mipSize{ (size_t)width * height * 4 };
	unsigned char* ptr{ allBuffer.data() };

	memcpy(ptr, pixels, mipSize);

	while (mipSize > 0) {
		stbir_resize_uint8(ptr, width, height, 0, ptr + mipSize, width / 2, height / 2, 0, 4);
		ptr += mipSize;
		width >>= 1;
		height >>= 1;
		mipSize = (size_t)width * height * 4;
	}

	auto mipEnd{ std::chrono::high_resolution_clock::now() };
	auto mipDiff{ mipEnd - mipStart };

	std::cout << "creating mipmaps took " << std::chrono::duration_cast<std::chrono::nanoseconds>(mipDiff).count() / 1000000.0 << "ms" << std::endl;

	texinfo.originalSize = allBuffer.size();
	assets::AssetFile newImage{ assets::packTexture(&texinfo, allBuffer.data()) };

	nlohmann::json textureMetadata;
	textureMetadata["format"] = "RGBA8";
	textureMetadata["original_size"] = texinfo.originalSize;
	textureMetadata["original_file"] = texinfo.originalFile;
	textureMetadata["miplevels"] = texinfo.miplevels;
	textureMetadata["width"] = texinfo.width;
	textureMetadata["height"] = texinfo.height;

	stbi_image_free(pixels);

	// will write compression_mode field of textureMetadata, and write that to newImage before saving
	saveBinaryFile(output.string().c_str(), textureMetadata, newImage);

	return true;
}

void unpackBufferGLTF(tinygltf::Model& model, tinygltf::Accessor& accesor, std::vector<uint8_t>& outputBuffer)
{
	int bufferID = accesor.bufferView;
	size_t elementSize = tinygltf::GetComponentSizeInBytes(accesor.componentType);

	tinygltf::BufferView& bufferView = model.bufferViews[bufferID];

	tinygltf::Buffer& bufferData = (model.buffers[bufferView.buffer]);


	uint8_t* dataptr = bufferData.data.data() + accesor.byteOffset + bufferView.byteOffset;

	int components = tinygltf::GetNumComponentsInType(accesor.type);

	elementSize *= components;

	size_t stride = bufferView.byteStride;
	if (stride == 0)
	{
		stride = elementSize;

	}

	outputBuffer.resize(accesor.count * elementSize);

	for (int i = 0; i < accesor.count; i++) {
		uint8_t* dataindex = dataptr + stride * i;
		uint8_t* targetptr = outputBuffer.data() + elementSize * i;

		memcpy(targetptr, dataindex, elementSize);
	}
}

// implementation for extractVerticesGLTF. This allows specialization to work properly for skinning
template <typename T>
void extractVerticesInternalGLTF(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<T>& _vertices)
{
	tinygltf::Accessor& pos_accesor = model.accessors[primitive.attributes["POSITION"]];
	_vertices.resize(pos_accesor.count);
	std::vector<uint8_t> pos_data;
	unpackBufferGLTF(model, pos_accesor, pos_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (pos_accesor.type == TINYGLTF_TYPE_VEC3)
		{
			if (pos_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)pos_data.data();

				//vec3f 
				_vertices[i].position[0] = *(dtf + (i * 3) + 0);
				_vertices[i].position[1] = *(dtf + (i * 3) + 1);
				_vertices[i].position[2] = *(dtf + (i * 3) + 2);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	tinygltf::Accessor& normal_accesor = model.accessors[primitive.attributes["NORMAL"]];
	std::vector<uint8_t> normal_data;
	unpackBufferGLTF(model, normal_accesor, normal_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (normal_accesor.type == TINYGLTF_TYPE_VEC3)
		{
			if (normal_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)normal_data.data();

				//vec3f 
				_vertices[i].normal[0] = *(dtf + (i * 3) + 0);
				_vertices[i].normal[1] = *(dtf + (i * 3) + 1);
				_vertices[i].normal[2] = *(dtf + (i * 3) + 2);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	tinygltf::Accessor& tangent_accesor = model.accessors[primitive.attributes["TANGENT"]];
	std::vector<uint8_t> tangent_data;
	unpackBufferGLTF(model, tangent_accesor, tangent_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (tangent_accesor.type == TINYGLTF_TYPE_VEC4)
		{
			if (tangent_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)tangent_data.data();

				//vec3f 
				_vertices[i].tangent[0] = *(dtf + (i * 3) + 0);
				_vertices[i].tangent[1] = *(dtf + (i * 3) + 1);
				_vertices[i].tangent[2] = *(dtf + (i * 3) + 2);
				_vertices[i].tangent[3] = *(dtf + (i * 3) + 3);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	tinygltf::Accessor& uv_accesor = model.accessors[primitive.attributes["TEXCOORD_0"]];
	std::vector<uint8_t> uv_data;
	unpackBufferGLTF(model, uv_accesor, uv_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (uv_accesor.type == TINYGLTF_TYPE_VEC2)
		{
			if (uv_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)uv_data.data();

				//vec3f 
				_vertices[i].uv[0] = *(dtf + (i * 2) + 0);
				_vertices[i].uv[1] = *(dtf + (i * 2) + 1);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	return;
}
// Vertex with tangent attribute
template <typename T>
void extractVerticesGLTF(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<T>& _vertices)
{
	extractVerticesInternalGLTF(primitive, model, _vertices);
}

// specialization for skinned vertices
template <>
void extractVerticesGLTF(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<assets::Vertex_f32_PNTVIW>& _vertices)
{
	extractVerticesInternalGLTF(primitive, model, _vertices);

	tinygltf::Accessor& joints_accesor = model.accessors[primitive.attributes["JOINTS_0"]];
	std::vector<uint8_t> joints_data;
	unpackBufferGLTF(model, joints_accesor, joints_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (joints_accesor.type == TINYGLTF_TYPE_VEC4)
		{
			if (joints_accesor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
			{
				float* dtf = (float*)joints_data.data();

				//vec3f 
				_vertices[i].jointIndices[0] = *(dtf + (i * 3) + 0);
				_vertices[i].jointIndices[1] = *(dtf + (i * 3) + 1);
				_vertices[i].jointIndices[2] = *(dtf + (i * 3) + 2);
				_vertices[i].jointIndices[3] = *(dtf + (i * 3) + 3);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}

	tinygltf::Accessor& weights_accesor = model.accessors[primitive.attributes["WEIGHTS_0"]];
	std::vector<uint8_t> weights_data;
	unpackBufferGLTF(model, weights_accesor, weights_data);
	for (int i = 0; i < _vertices.size(); i++) {
		if (weights_accesor.type == TINYGLTF_TYPE_VEC4)
		{
			if (weights_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
			{
				float* dtf = (float*)weights_data.data();

				//vec3f 
				_vertices[i].jointWeights[0] = *(dtf + (i * 3) + 0);
				_vertices[i].jointWeights[1] = *(dtf + (i * 3) + 1);
				_vertices[i].jointWeights[2] = *(dtf + (i * 3) + 2);
				_vertices[i].jointWeights[3] = *(dtf + (i * 3) + 3);
			} else {
				std::cout << "ERROR: Component type mismatch\n";
				assert(false);
			}
		} else {
			std::cout << "ERROR: Accessor type mismatch\n";
			assert(false);
		}
	}
}

void extractIndicesGLTF(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<uint16_t>& _primindices)
{
	int indexaccesor = primitive.indices;

	int indexbuffer = model.accessors[indexaccesor].bufferView;
	int componentType = model.accessors[indexaccesor].componentType;
	size_t indexsize = tinygltf::GetComponentSizeInBytes(componentType);

	tinygltf::BufferView& indexview = model.bufferViews[indexbuffer];
	int bufferidx = indexview.buffer;

	tinygltf::Buffer& buffindex = (model.buffers[bufferidx]);

	uint8_t* dataptr = buffindex.data.data() + indexview.byteOffset;

	std::vector<uint8_t> unpackedIndices;
	unpackBufferGLTF(model, model.accessors[indexaccesor], unpackedIndices);

	for (int i = 0; i < model.accessors[indexaccesor].count; i++) {

		uint32_t index;
		switch (componentType) {
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			uint16_t* bfr = (uint16_t*)unpackedIndices.data();
			index = *(bfr + i);
		}
		break;
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		{
			int16_t* bfr = (int16_t*)unpackedIndices.data();
			index = *(bfr + i);
		}
		break;
		default:
			assert(false);
		}

		_primindices.push_back(index);
	}

	for (int i = 0; i < _primindices.size() / 3; i++)
	{
		//flip the triangle

		std::swap(_primindices[i * 3 + 1], _primindices[i * 3 + 2]);
	}
}

std::string calculateMeshNameGLTF(tinygltf::Model& model, int meshIndex, int primitiveIndex)
{
	char buffer0[50];
	char buffer1[50];
	itoa(meshIndex, buffer0, 10);
	itoa(primitiveIndex, buffer1, 10);

	std::string meshname = "MESH_" + std::string{ &buffer0[0] } + "_" + model.meshes[meshIndex].name;

	bool multiprim = model.meshes[meshIndex].primitives.size() > 1;

	if (multiprim) {
		meshname += "_PRIM_" + std::string{ &buffer1[0] };
	}

	return meshname;
}

template <typename VFormat>
bool extractMeshesGLTF(tinygltf::Model& model, const fs::path& input, const fs::path& outputFolder, const ConverterState& convState, assets::VertexFormat vertexFormatEnum)
{
	tinygltf::Model* glmod = &model;
	for (auto meshindex = 0; meshindex < model.meshes.size(); meshindex++) {

		auto& glmesh = model.meshes[meshindex];

		//using Format = assets::Vertex_f32_PNTV;
		//auto vertexFormatEnum = assets::VertexFormat::PNTV_F32;

		std::vector<VFormat> _vertices;
		std::vector<uint16_t> _indices;

		for (auto primindex = 0; primindex < glmesh.primitives.size(); primindex++) {

			_vertices.clear();
			_indices.clear();

			std::string meshname = calculateMeshNameGLTF(model, meshindex, primindex);

			auto& primitive = glmesh.primitives[primindex];

			extractIndicesGLTF(primitive, model, _indices);
			extractVerticesGLTF(primitive, model, _vertices);


			MeshInfo meshinfo;
			meshinfo.vertexFormat = vertexFormatEnum;
			meshinfo.vertexBufferSize = _vertices.size() * sizeof(VFormat);
			meshinfo.indexBufferSize = _indices.size() * sizeof(uint16_t);
			meshinfo.indexSize = sizeof(uint16_t);
			meshinfo.originalFile = input.string();

			meshinfo.bounds = assets::calculateBounds(_vertices.data(), _vertices.size());

			assets::AssetFile newFile{ assets::packMesh(&meshinfo, (char*)_vertices.data(), (char*)_indices.data()) };

			nlohmann::json metadata;

			if (meshinfo.vertexFormat == VertexFormat::PNTV_F32) {
				metadata["vertex_format"] = "PNTV_F32";
			} else if (meshinfo.vertexFormat == VertexFormat::PNTVIW_F32) {
				metadata["vertex_format"] = "PNTVIW_F32";
			} else if (meshinfo.vertexFormat == VertexFormat::P32N8C8V16) {
				metadata["vertex_format"] = "P32N8C8V16";
			} else if (meshinfo.vertexFormat == VertexFormat::PNCV_F32) {
				metadata["vertex_format"] = "PNCV_F32";
			}

			metadata["vertex_buffer_size"] = meshinfo.vertexBufferSize;
			metadata["index_buffer_size"] = meshinfo.indexBufferSize;
			metadata["index_size"] = meshinfo.indexSize;
			metadata["original_file"] = meshinfo.originalFile;

			std::vector<float> boundsData;
			boundsData.resize(7);

			boundsData[0] = meshinfo.bounds.origin[0];
			boundsData[1] = meshinfo.bounds.origin[1];
			boundsData[2] = meshinfo.bounds.origin[2];

			boundsData[3] = meshinfo.bounds.radius;

			boundsData[4] = meshinfo.bounds.extents[0];
			boundsData[5] = meshinfo.bounds.extents[1];
			boundsData[6] = meshinfo.bounds.extents[2];

			metadata["bounds"] = boundsData;

			fs::path meshpath = outputFolder / (meshname + ".mesh");

			//save to disk
			saveBinaryFile(meshpath.string().c_str(), metadata, newFile);
		}
	}
	return true;
}

//Node* findNode(Node* parent, uint32_t index) {
//	Node* nodeFound = nullptr;
//	if (parent->index == index) {
//		return parent;
//	}
//	for (auto& child : parent->children) {
//		nodeFound = findNode(child, index);
//		if (nodeFound) {
//			break;
//		}
//	}
//	return nodeFound;
//}
//
//Node* nodeFromIndex(uint32_t index) {
//	Node* nodeFound = nullptr;
//	for (Node& node : nodes) {
//		nodeFound = findNode(node, index);
//		if (nodeFound) {
//			break;
//		}
//	}
//	return nodeFound;
//}
//
//// from https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.cpp
//void loadSkins(tinygltf::Model& gltfModel)
//{
//	for (tinygltf::Skin& source : gltfModel.skins) {
//		Skin* newSkin{ new Skin{} };
//		newSkin->name = source.name;
//
//		// Find skeleton root node
//		if (source.skeleton > -1) {
//			newSkin->skeletonRoot = nodeFromIndex(source.skeleton);
//		}
//
//		// Find joint nodes
//		for (int jointIndex : source.joints) {
//			Node* node = nodeFromIndex(jointIndex);
//			if (node) {
//				newSkin->joints.push_back(nodeFromIndex(jointIndex));
//			}
//		}
//
//		// Get inverse bind matrices from buffer
//		if (source.inverseBindMatrices > -1) {
//			const tinygltf::Accessor& accessor = gltfModel.accessors[source.inverseBindMatrices];
//			const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
//			const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];
//			newSkin->inverseBindMatrices.resize(accessor.count);
//			memcpy(newSkin->inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
//		}
//
//		skins.push_back(newSkin);
//	}
//}

/*
std::string calculateMaterialNameGLTF(tinygltf::Model& model, int materialIndex)
{
	char buffer[50];

	itoa(materialIndex, buffer, 10);
	std::string matname = "MAT_" + std::string{ &buffer[0] } + "_" + model.materials[materialIndex].name;
	return matname;
}

void extractMaterialsGLTF(tinygltf::Model& model, const fs::path& input, const fs::path& outputFolder, const ConverterState& convState)
{

	int nm = 0;
	for (auto& glmat : model.materials) {
		std::string matname = calculateMaterialNameGLTF(model, nm);

		nm++;
		auto& pbr = glmat.pbrMetallicRoughness;


		assets::MaterialInfo newMaterial;
		newMaterial.baseEffect = "defaultPBR";

		{
			if (pbr.baseColorTexture.index < 0)
			{
				pbr.baseColorTexture.index = 0;
			}
			auto baseColor = model.textures[pbr.baseColorTexture.index];
			auto baseImage = model.images[baseColor.source];

			fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

			baseColorPath.replace_extension(".tx");

			baseColorPath = convState.convertToExportRelative(baseColorPath);

			newMaterial.textures["baseColor"] = baseColorPath.string();
		}
		if (pbr.metallicRoughnessTexture.index >= 0)
		{
			auto image = model.textures[pbr.metallicRoughnessTexture.index];
			auto baseImage = model.images[image.source];

			fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

			baseColorPath.replace_extension(".tx");

			baseColorPath = convState.convertToExportRelative(baseColorPath);

			newMaterial.textures["metallicRoughness"] = baseColorPath.string();
		}

		if (glmat.normalTexture.index >= 0)
		{
			auto image = model.textures[glmat.normalTexture.index];
			auto baseImage = model.images[image.source];

			fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

			baseColorPath.replace_extension(".tx");

			baseColorPath = convState.convertToExportRelative(baseColorPath);

			newMaterial.textures["normals"] = baseColorPath.string();
		}

		if (glmat.occlusionTexture.index >= 0)
		{
			auto image = model.textures[glmat.occlusionTexture.index];
			auto baseImage = model.images[image.source];

			fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

			baseColorPath.replace_extension(".tx");

			baseColorPath = convState.convertToExportRelative(baseColorPath);

			newMaterial.textures["occlusion"] = baseColorPath.string();
		}

		if (glmat.emissiveTexture.index >= 0)
		{
			auto image = model.textures[glmat.emissiveTexture.index];
			auto baseImage = model.images[image.source];

			fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

			baseColorPath.replace_extension(".tx");

			baseColorPath = convState.convertToExportRelative(baseColorPath);

			newMaterial.textures["emissive"] = baseColorPath.string();
		}


		fs::path materialPath = outputFolder / (matname + ".mat");

		if (glmat.alphaMode.compare("BLEND") == 0) {
			newMaterial.transparency = TransparencyMode::Transparent;
		} else {
			newMaterial.transparency = TransparencyMode::Opaque;
		}

		assets::AssetFile newFile = assets::pack_material(&newMaterial);

		//save to disk
		saveBinaryFile(materialPath.string().c_str(), newFile);
	}
}
*/

/*
void extractNodesGLTF(tinygltf::Model& model, const fs::path& input, const fs::path& outputFolder, const ConverterState& convState)
{
	assets::PrefabInfo prefab;

	std::vector<uint64_t> meshnodes;
	for (int i = 0; i < model.nodes.size(); i++)
	{
		auto& node = model.nodes[i];

		std::string nodename = node.name;

		prefab.node_names[i] = nodename;

		std::array<float, 16> matrix;

		//node has a matrix
		if (node.matrix.size() > 0)
		{
			for (int n = 0; n < 16; n++) {
				matrix[n] = node.matrix[n];
			}

			//glm::mat4 flip = glm::mat4{ 1.0 };
			//flip[1][1] = -1;

			glm::mat4 mat;

			memcpy(&mat, &matrix, sizeof(glm::mat4));

			mat = mat;// * flip;

			memcpy(matrix.data(), &mat, sizeof(glm::mat4));
		}
		//separate transform
		else
		{

			glm::mat4 translation{ 1.f };
			if (node.translation.size() > 0)
			{
				translation = glm::translate(glm::vec3{ node.translation[0],node.translation[1] ,node.translation[2] });
			}

			glm::mat4 rotation{ 1.f };

			if (node.rotation.size() > 0)
			{
				glm::quat rot(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
				rotation = glm::mat4{ rot };
			}

			glm::mat4 scale{ 1.f };
			if (node.scale.size() > 0)
			{
				scale = glm::scale(glm::vec3{ node.scale[0],node.scale[1] ,node.scale[2] });
			}
			//glm::mat4 flip = glm::mat4{ 1.0 };
			//flip[1][1] = -1;

			glm::mat4 transformMatrix = (translation * rotation * scale);// * flip;

			memcpy(matrix.data(), &transformMatrix, sizeof(glm::mat4));
		}

		prefab.node_matrices[i] = prefab.matrices.size();
		prefab.matrices.push_back(matrix);

		if (node.mesh >= 0)
		{
			auto mesh = model.meshes[node.mesh];

			if (mesh.primitives.size() > 1) {
				meshnodes.push_back(i);
			} else {
				auto primitive = mesh.primitives[0];
				std::string meshname = calculate_gltf_mesh_name(model, node.mesh, 0);

				fs::path meshpath = outputFolder / (meshname + ".mesh");

				int material = primitive.material;

				std::string matname = calculateMaterialNameGLTF(model, material);

				fs::path materialpath = outputFolder / (matname + ".mat");

				assets::PrefabInfo::NodeMesh nmesh;
				nmesh.mesh_path = convState.convertToExportRelative(meshpath).string();
				nmesh.material_path = convState.convertToExportRelative(materialpath).string();

				prefab.node_meshes[i] = nmesh;
			}
		}
	}

	//calculate parent hierarchies
	//gltf stores children, but we want parent
	for (int i = 0; i < model.nodes.size(); i++)
	{
		for (auto c : model.nodes[i].children)
		{
			prefab.node_parents[c] = i;
		}
	}

	//for every gltf node that is a root node (no parents), apply the coordinate fixup

	glm::mat4 flip = glm::mat4{ 1.0 };
	flip[1][1] = -1;


	glm::mat4 rotation = glm::mat4{ 1.0 };
	//flip[1][1] = -1;
	rotation = glm::rotate(glm::radians(-180.f), glm::vec3{ 1,0,0 });


	//flip[2][2] = -1;
	for (int i = 0; i < model.nodes.size(); i++)
	{

		auto it = prefab.node_parents.find(i);
		if (it == prefab.node_parents.end())
		{
			auto matrix = prefab.matrices[prefab.node_matrices[i]];
			//no parent, root node
			glm::mat4 mat;

			memcpy(&mat, &matrix, sizeof(glm::mat4));

			mat = rotation * (flip * mat);

			memcpy(&matrix, &mat, sizeof(glm::mat4));

			prefab.matrices[prefab.node_matrices[i]] = matrix;

		}
	}


	int nodeindex = model.nodes.size();
	//iterate nodes with mesh, convert each submesh into a node
	for (int i = 0; i < meshnodes.size(); i++)
	{
		auto& node = model.nodes[i];

		if (node.mesh < 0) break;

		auto mesh = model.meshes[node.mesh];


		for (int primindex = 0; primindex < mesh.primitives.size(); primindex++)
		{
			auto primitive = mesh.primitives[primindex];
			int newnode = nodeindex++;

			char buffer[50];

			itoa(primindex, buffer, 10);

			prefab.node_names[newnode] = prefab.node_names[i] + "_PRIM_" + &buffer[0];

			int material = primitive.material;
			auto mat = model.materials[material];
			std::string matname = calculateMaterialNameGLTF(model, material);
			std::string meshname = calculateMeshNameGLTF(model, node.mesh, primindex);

			fs::path materialpath = outputFolder / (matname + ".mat");
			fs::path meshpath = outputFolder / (meshname + ".mesh");

			assets::PrefabInfo::NodeMesh nmesh;
			nmesh.mesh_path = convState.convertToExportRelative(meshpath).string();
			nmesh.material_path = convState.convertToExportRelative(materialpath).string();

			prefab.node_meshes[newnode] = nmesh;
		}

	}


	assets::AssetFile newFile = assets::pack_prefab(prefab);

	fs::path scenefilepath = (outputFolder.parent_path()) / input.stem();

	scenefilepath.replace_extension(".pfb");

	//save to disk
	saveBinaryFile(scenefilepath.string().c_str(), newFile);
}
*/
int main(int argc, char* argv[])
{
	if (argc < 2) {
		std::cout << "You need to put the path to the info file";
		return -1;
	} else {

		fs::path path{ argv[1] };

		fs::path directory = path;

		fs::path exported_dir = path.parent_path() / "assets_export";

		std::cout << "loaded asset directory at " << directory << std::endl;

		ConverterState convstate;
		convstate.asset_path = path;
		convstate.export_path = exported_dir;

		for (auto& p : fs::recursive_directory_iterator(directory))
		{
			std::cout << "File: " << p << std::endl;

			auto relative = p.path().lexically_proximate(directory);

			auto export_path = exported_dir / relative;

			if (!fs::is_directory(export_path.parent_path()))
			{
				fs::create_directory(export_path.parent_path());
			}

			if (p.path().extension() == ".png" || p.path().extension() == ".jpg" || p.path().extension() == ".TGA")
			{
				std::cout << "found a texture" << std::endl;

				auto newpath = p.path();

				export_path.replace_extension(".tx");

				convertImage(p.path(), export_path);
			}

			if (p.path().extension() == ".gltf")
			{
				std::cout << "found a mesh (gltf)\n";

				using namespace tinygltf;
				Model model;
				TinyGLTF loader;
				std::string err;
				std::string warn;

				bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, p.path().string().c_str());

				if (!warn.empty()) {
					printf("Warn: %s\n", warn.c_str());
				}

				if (!err.empty()) {
					printf("Err: %s\n", err.c_str());
				}

				if (!ret) {
					printf("Failed to parse glTF\n");
					return -1;
				} else {
					auto folder = export_path.parent_path() / (p.path().stem().string() + "_GLTF");
					fs::create_directory(folder);

					// If the mesh is skinned, we must use vertex format which includes skinning data
					std::cout << "skins: " << model.skins.size() << '\n';
					if (model.skins.size() == 0) {
						extractMeshesGLTF<assets::Vertex_f32_PNTV>(model, p.path(), folder, convstate, assets::VertexFormat::PNTV_F32);
					} else {
						extractMeshesGLTF<assets::Vertex_f32_PNTVIW>(model, p.path(), folder, convstate, assets::VertexFormat::PNTVIW_F32);
					}


					//extractMaterialsGLTF(model, p.path(), folder, convstate);

					//extractNodesGLTF(model, p.path(), folder, convstate);
				}
			}
		}
	}

	return 0;
}

fs::path ConverterState::convertToExportRelative(fs::path path) const
{
	return path.lexically_proximate(export_path);
}