#pragma once

#include "dx12.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "thirdparty/include/stb_image.h"
#include "thirdparty/include/stb_image_write.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include "thirdparty/include/tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "thirdparty/include/tiny_obj_loader.h"

#include "thirdparty/include/rtxgi/ddgi/DDGIVolume.h"
#define USE_PIX
#include "thirdparty/include/rtxgi/pix3.h"

struct ModelVertex {
	std::array<float, 3> position;
	std::array<float, 3> normal;
	std::array<float, 2> texCoords;
	std::array<float, 3> tangent;
};

struct ModelPrimitive {
	int materialIndex;
	std::vector<ModelVertex> vertices;
	std::vector<char> indices;
	int indexSize;
	ID3D12Resource* blasBuffer = nullptr;
};

struct ModelMesh {
	std::vector<ModelPrimitive> primitives;
	std::string name;
};

struct ModelNode {
	int meshIndex;
	DirectX::XMMATRIX transform;
	std::vector<int> children;
};

struct ModelMaterial {
	std::array<float, 4> baseColorFactor;
	int baseColorTextureIndex;
	int baseColorTextureSamplerIndex;
	int normalTextureIndex;
	int normalTextureSamplerIndex;
};

struct ModelImage {
	ID3D12Resource* image = nullptr;
};

struct Model {
	std::vector<ModelNode> nodes;
	std::vector<int> rootNodes;
	std::vector<ModelMesh> meshes;
	std::vector<ModelMaterial> materials;
	std::vector<ModelImage> images;
};

struct InstanceInfo {
	DirectX::XMMATRIX transformMat;
	int triangleOffset;
	int triangleCount;
	int materialIndex;
	int padding;
};

struct TriangleInfo {
	std::array<std::array<float, 3>, 3> normals;
	std::array<std::array<float, 2>, 3> texCoords;
	std::array<std::array<float, 3>, 3> tangents;
};

struct MaterialInfo {
	std::array<float, 4> baseColorFactor;
	int baseColorTextureIndex;
	int baseColorTextureSamplerIndex;
	int normalTextureIndex;
	int normalTextureSamplerIndex;
};

struct Camera {
	DirectX::XMVECTOR position = DirectX::XMVectorSet(5, 2, -0.3f, 0);
	DirectX::XMVECTOR lookAt = DirectX::XMVectorAdd(position, DirectX::XMVectorSet(-1, 0, 0, 0));
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
	float pitchAngle = 0;
};

struct SceneInfo {
	enum Type {
		Camera,
		Model,
		Entity
	};

	Type type;
	struct Camera camera;
	std::string_view name;
	std::string_view path;
	std::array<float, 4> rotation;
	std::array<float, 3> scaling;
	std::array<float, 3> translation;
};

struct SceneParser : public Parser {
	using Parser::Parser;

	bool getInfo(SceneInfo* info) {
		Token token;
		if (getToken(&token) && token.type == Token::Identifier) {
			if (token.str == "Camera") {
				float numbers[6] = {};
				for (float& number : numbers) {
					if (!getToken(&token) || !token.toFloat(&number)) {
						return false;
					}
				}
				info->type = SceneInfo::Camera;
				info->camera.position = DirectX::XMVectorSet(numbers[0], numbers[1], numbers[2], 0);
				info->camera.lookAt = DirectX::XMVector3Normalize(DirectX::XMVectorSet(numbers[3], numbers[4], numbers[5], 0));
				info->camera.up = DirectX::XMVectorSet(0, 1, 0, 0);
				info->camera.pitchAngle = DirectX::XMVectorGetX(DirectX::XMVector3AngleBetweenVectors(info->camera.lookAt, DirectX::XMVectorSet(0, 1, 0, 0)));
				info->camera.pitchAngle = static_cast<float>(M_PI_2) - info->camera.pitchAngle;
				return true;
			}
			else if (token.str == "Model") {
				info->type = SceneInfo::Model;
				if (!getToken(&token) || token.type != Token::String) {
					return false;
				}
				info->name = token.str;
				if (!getToken(&token) || token.type != Token::String) {
					return false;
				}
				info->path = token.str;
				return true;
			}
			else if (token.str == "Entity") {
				info->type = SceneInfo::Entity;
				debugPrintf("Entity:");
				if (getToken(&token) && token.type == Token::String) {
					debugPrintf(" %.*s", token.str.length(), token.str.data());
				}
				float numbers[10] = {};
				for (float& number : numbers) {
					if (getToken(&token) && token.toFloat(&number)) {
						debugPrintf(" %f", number);
					}
					else {
						return false;
					}
				}
				debugPrintf("\n");
				return true;
			}
			else {
				return false;
			}
		}
		else {
			return false;
		}
	}
};

struct Scene {
	Camera camera;
	std::unordered_map<std::string, Model> models;
	ID3D12Resource* tlasBuffer = nullptr;
	ID3D12Resource* instanceInfosBuffer = nullptr;
	ID3D12Resource* triangleInfosBuffer = nullptr;
	ID3D12Resource* materialInfosBuffer = nullptr;
	int instanceCount = 0;
	int triangleCount = 0;
	int materialCount = 0;
	std::filesystem::path filePath;

	Scene() {};
	Scene(const std::filesystem::path& sceneFilePath, DX12Context* dx12) : filePath(sceneFilePath) {
		setCurrentDirToExeDir();
		SceneParser parser(sceneFilePath);
		SceneInfo info;
		while (parser.getInfo(&info)) {
			if (info.type == SceneInfo::Camera) {
				camera = info.camera;
			}
			else if (info.type == SceneInfo::Model) {
				Model model = createModelFromGLTF(info.path, dx12);
				models.emplace(info.name, model);
			}
			else if (info.type == SceneInfo::Entity) {
			}
		}
	}
	Model createModelFromGLTF(const std::filesystem::path& gltfFilePath, DX12Context* dx12) {
		tinygltf::TinyGLTF gltfLoader;
		std::string gltfLoadError;
		std::string gltfLoadWarning;
		tinygltf::Model gltfModel;
		bool loadSuccess = gltfLoader.LoadASCIIFromFile(&gltfModel, &gltfLoadError, &gltfLoadWarning, gltfFilePath.string());
		if (!loadSuccess) {
			throw Exception(std::move(gltfLoadError));
		}
		assert(gltfModel.scenes.size() == 1);

		Model model = {};
		model.nodes.reserve(gltfModel.nodes.size());
		for (auto& gltfNode : gltfModel.nodes) {
			DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity();
			if (gltfNode.matrix.size() > 0) {
				assert(gltfNode.matrix.size() == 16);
				DirectX::XMFLOAT4X4 mat;
				for (int row = 0; row < 4; row += 1) {
					for (int column = 0; column < 4; column += 1) {
						mat(row, column) = static_cast<float>(gltfNode.matrix[column * 4 + row]);
					}
				}
				transform = DirectX::XMLoadFloat4x4(&mat);
			}
			else {
				if (gltfNode.rotation.size() > 0) {
					DirectX::XMVECTOR q = DirectX::XMVectorSet(static_cast<float>(gltfNode.rotation[0]), static_cast<float>(gltfNode.rotation[1]), static_cast<float>(gltfNode.rotation[2]), static_cast<float>(gltfNode.rotation[3]));
					transform = transform * DirectX::XMMatrixRotationQuaternion(q);
				}
				if (gltfNode.scale.size() > 0) {
					transform = transform * DirectX::XMMatrixScaling(static_cast<float>(gltfNode.scale[0]), static_cast<float>(gltfNode.scale[1]), static_cast<float>(gltfNode.scale[2]));
				}
				if (gltfNode.translation.size() > 0) {
					transform = transform * DirectX::XMMatrixTranslation(static_cast<float>(gltfNode.translation[0]), static_cast<float>(gltfNode.translation[1]), static_cast<float>(gltfNode.translation[2]));
				}
			}
			model.nodes.push_back(ModelNode{ gltfNode.mesh, transform, gltfNode.children });
		}
		model.rootNodes = gltfModel.scenes[0].nodes;
		model.meshes.reserve(gltfModel.meshes.size());
		for (auto& gltfMesh : gltfModel.meshes) {
			ModelMesh modelMesh;
			modelMesh.name = gltfMesh.name;
			modelMesh.primitives.reserve(gltfMesh.primitives.size());
			for (auto& gltfPrimitive : gltfMesh.primitives) {
				ModelPrimitive modelPrimitive;
				modelPrimitive.materialIndex = gltfPrimitive.material;
				assert(gltfPrimitive.material >= 0 && gltfPrimitive.material < gltfModel.materials.size());

				int positionAccessorIndex = gltfPrimitive.attributes.at("POSITION");
				auto& positionAccessor = gltfModel.accessors[positionAccessorIndex];
				auto& positionBufferView = gltfModel.bufferViews[positionAccessor.bufferView];
				auto& positionBuffer = gltfModel.buffers[positionBufferView.buffer];
				unsigned char* positionData = &positionBuffer.data[positionAccessor.byteOffset + positionBufferView.byteOffset];
				assert(positionAccessor.type == TINYGLTF_TYPE_VEC3 && positionAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && (positionBufferView.byteStride == 0 || positionBufferView.byteStride == 12));
				assert(positionAccessor.count < USHRT_MAX);

				int normalAccessorIndex = gltfPrimitive.attributes.at("NORMAL");
				auto& normalAccessor = gltfModel.accessors[normalAccessorIndex];
				auto& normalBufferView = gltfModel.bufferViews[normalAccessor.bufferView];
				auto& normalBuffer = gltfModel.buffers[normalBufferView.buffer];
				unsigned char* normalData = &normalBuffer.data[normalAccessor.byteOffset + normalBufferView.byteOffset];
				assert(normalAccessor.type == TINYGLTF_TYPE_VEC3 && normalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && (normalBufferView.byteStride == 0 || normalBufferView.byteStride == 12));
				assert(normalAccessor.count == positionAccessor.count);

				int uvAccessorIndex = gltfPrimitive.attributes.at("TEXCOORD_0");
				auto& uvAccessor = gltfModel.accessors[uvAccessorIndex];
				auto& uvBufferView = gltfModel.bufferViews[uvAccessor.bufferView];
				auto& uvBuffer = gltfModel.buffers[uvBufferView.buffer];
				unsigned char* uvData = &uvBuffer.data[uvAccessor.byteOffset + uvBufferView.byteOffset];
				assert(uvAccessor.type == TINYGLTF_TYPE_VEC2 && uvAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && (uvBufferView.byteStride == 0 || uvBufferView.byteStride == 8));
				assert(uvAccessor.count == positionAccessor.count);

				auto& indexAccessor = gltfModel.accessors[gltfPrimitive.indices];
				auto& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
				auto& indexBuffer = gltfModel.buffers[indexBufferView.buffer];
				unsigned char* indexData = &indexBuffer.data[indexAccessor.byteOffset + indexBufferView.byteOffset];
				assert(indexAccessor.count % 3 == 0 && indexAccessor.type == TINYGLTF_TYPE_SCALAR);
				if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					assert(indexBufferView.byteStride == 0 || indexBufferView.byteStride == 2);
					modelPrimitive.indexSize = 2;
				}
				else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					assert(indexBufferView.byteStride == 0 || indexBufferView.byteStride == 4);
					modelPrimitive.indexSize = 4;
				}
				else {
					assert(false);
				}
				assert(gltfPrimitive.mode == TINYGLTF_MODE_TRIANGLES);

				modelPrimitive.vertices.resize(positionAccessor.count);
				for (size_t i = 0; i < positionAccessor.count; i += 1) {
					memcpy(modelPrimitive.vertices[i].position.data(), positionData + i * 12, 12);
					memcpy(modelPrimitive.vertices[i].normal.data(), normalData + i * 12, 12);
					memcpy(modelPrimitive.vertices[i].texCoords.data(), uvData + i * 8, 8);
				}
				auto tangentAttrib = gltfPrimitive.attributes.find("TANGENT");
				if (tangentAttrib != gltfPrimitive.attributes.end()) {
					int tangentAccessorIndex = tangentAttrib->second;
					auto& tangentAccessor = gltfModel.accessors[tangentAccessorIndex];
					auto& tangentBufferView = gltfModel.bufferViews[tangentAccessor.bufferView];
					auto& tangentBuffer = gltfModel.buffers[tangentBufferView.buffer];
					unsigned char* tangentData = &tangentBuffer.data[tangentAccessor.byteOffset + tangentBufferView.byteOffset];
					assert(tangentAccessor.type == TINYGLTF_TYPE_VEC4 && tangentAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && (tangentBufferView.byteStride == 0 || tangentBufferView.byteStride == 16));
					assert(tangentAccessor.count == positionAccessor.count);
					for (size_t i = 0; i < positionAccessor.count; i += 1) {
						memcpy(modelPrimitive.vertices[i].tangent.data(), tangentData + i * 16, 12);
					}
				}
				else {
					auto& gltfMaterial = gltfModel.materials[gltfPrimitive.material];
					assert(gltfMaterial.normalTexture.index < 0);
				}
				modelPrimitive.indices.resize(indexAccessor.count * modelPrimitive.indexSize);
				memcpy(modelPrimitive.indices.data(), indexData, indexAccessor.count * modelPrimitive.indexSize);

				ID3D12Resource* indicesBuffer = dx12->createBuffer(modelPrimitive.indices.size(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
				ID3D12Resource* vertexPositionsBuffer = dx12->createBuffer(modelPrimitive.vertices.size() * 12, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
				char* indicesBufferPtr = nullptr;
				dx12Assert(indicesBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indicesBufferPtr)));
				memcpy(indicesBufferPtr, modelPrimitive.indices.data(), modelPrimitive.indices.size());
				indicesBuffer->Unmap(0, nullptr);
				char* vertexPositionsBufferPtr = nullptr;
				vertexPositionsBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexPositionsBufferPtr));
				for (auto& vertex : modelPrimitive.vertices) {
					memcpy(vertexPositionsBufferPtr, vertex.position.data(), 12);
					vertexPositionsBufferPtr += 12;
				}
				vertexPositionsBuffer->Unmap(0, nullptr);

				D3D12_RAYTRACING_GEOMETRY_DESC meshGeometryDesc = {};
				meshGeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				meshGeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE; // D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
				meshGeometryDesc.Triangles.IndexFormat = (modelPrimitive.indexSize == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
				meshGeometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				meshGeometryDesc.Triangles.IndexCount = static_cast<UINT>(modelPrimitive.indices.size() / modelPrimitive.indexSize);
				meshGeometryDesc.Triangles.VertexCount = static_cast<UINT>(modelPrimitive.vertices.size());
				meshGeometryDesc.Triangles.IndexBuffer = indicesBuffer->GetGPUVirtualAddress();
				meshGeometryDesc.Triangles.VertexBuffer = { vertexPositionsBuffer->GetGPUVirtualAddress(), 12 };

				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInput = {};
				blasInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
				blasInput.NumDescs = 1;
				blasInput.pGeometryDescs = &meshGeometryDesc;

				D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo;
				dx12->device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInput, &blasPrebuildInfo);

				ID3D12Resource* blasScratchBuffer = dx12->createBuffer(blasPrebuildInfo.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
				ID3D12Resource* blasBuffer = dx12->createBuffer(blasPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
				blasBuffer->SetName(L"bottomAccelerationStructureBuffer");

				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
				blasDesc.DestAccelerationStructureData = blasBuffer->GetGPUVirtualAddress();
				blasDesc.Inputs = blasInput;
				blasDesc.ScratchAccelerationStructureData = blasScratchBuffer->GetGPUVirtualAddress();

				dx12->setCurrentCommandList();
				ID3D12GraphicsCommandList4* cmdList = dx12->graphicsCommandLists[dx12->currentGraphicsCommandListIndex].list;
				cmdList->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);
				dx12->executeCurrentCommandList();
				dx12->drainGraphicsCommandQueue();

				modelPrimitive.blasBuffer = blasBuffer;
				blasScratchBuffer->Release();

				indicesBuffer->Release();
				vertexPositionsBuffer->Release();

				modelMesh.primitives.push_back(std::move(modelPrimitive));
			}
			model.meshes.push_back(std::move(modelMesh));
		}
		model.materials.reserve(gltfModel.materials.size());
		for (auto& gltfMaterial : gltfModel.materials) {
			ModelMaterial material = { {}, -1, -1, -1, -1 };
			for (int i = 0; i < 4; i += 1) {
				material.baseColorFactor[i] = static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[i]);
			}
			assert(gltfMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0 && gltfMaterial.pbrMetallicRoughness.baseColorTexture.index < gltfModel.textures.size());
			assert(gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord == 0);
			auto& baseColorTexture = gltfModel.textures[gltfMaterial.pbrMetallicRoughness.baseColorTexture.index];
			assert(baseColorTexture.source >= 0 && baseColorTexture.source < gltfModel.images.size());
			assert(baseColorTexture.sampler >= 0 && baseColorTexture.sampler < gltfModel.samplers.size());
			material.baseColorTextureIndex = baseColorTexture.source;
			material.baseColorTextureSamplerIndex = baseColorTexture.sampler;
			if (gltfMaterial.normalTexture.index >= 0) {
				assert(gltfMaterial.normalTexture.texCoord == 0);
				assert(gltfMaterial.normalTexture.scale == 1.0);
				auto& normalTexture = gltfModel.textures[gltfMaterial.normalTexture.index];
				assert(normalTexture.source >= 0 && normalTexture.source < gltfModel.images.size());
				assert(normalTexture.sampler >= 0 && normalTexture.sampler < gltfModel.samplers.size());
				material.normalTextureIndex = normalTexture.source;
				material.normalTextureSamplerIndex = normalTexture.sampler;
			}
			model.materials.push_back(material);
		}
		model.images.reserve(gltfModel.images.size());
		for (auto& gltfImage : gltfModel.images) {
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
			if (gltfImage.component == 1 && gltfImage.bits == 8 && gltfImage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				format = DXGI_FORMAT_R8_UNORM;
			}
			else if (gltfImage.component == 2 && gltfImage.bits == 8 && gltfImage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				format = DXGI_FORMAT_R8G8_UNORM;
			}
			else if (gltfImage.component == 4 && gltfImage.bits == 8 && gltfImage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			}
			assert(format != DXGI_FORMAT_UNKNOWN);
			ID3D12Resource* image = dx12->createTexture(gltfImage.width, gltfImage.height, 1, 1, format, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
			DX12TextureCopy textureCopy = {
				image, reinterpret_cast<char*>(gltfImage.image.data()), static_cast<int>(gltfImage.image.size()),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			};
			dx12->copyTextures(&textureCopy, 1);
			std::wstring name(gltfImage.uri.begin(), gltfImage.uri.end());
			image->SetName(name.c_str());
			model.images.push_back(ModelImage{ image });
		}
		return model;
	}
	void rebuildTLAS(DX12Context* dx12) {
		if (models.empty()) {
			return;
		}
		if (tlasBuffer) {
			tlasBuffer->Release();
		}
		if (instanceInfosBuffer) {
			instanceInfosBuffer->Release();
		}
		if (triangleInfosBuffer) {
			triangleInfosBuffer->Release();
		}
		if (materialInfosBuffer) {
			materialInfosBuffer->Release();
		}

		struct PrimitiveInfo {
			int triangleOffset;
			int triangleCount;
			int materialOffset;
		};
		std::vector<std::vector<std::vector<PrimitiveInfo>>> primitiveInfos;
		std::vector<TriangleInfo> triangleInfos;
		std::vector<MaterialInfo> materialInfos;
		int imageCount = 0;
		for (auto& [modelName, model] : models) {
			std::vector<std::vector<PrimitiveInfo>> modelPrimitiveInfos;
			for (auto& mesh : model.meshes) {
				std::vector<PrimitiveInfo> meshPrimitiveInfos;
				for (auto& primitive : mesh.primitives) {
					int triangleCount = static_cast<int>(primitive.indices.size() / primitive.indexSize) / 3;
					PrimitiveInfo primitiveInfo = { static_cast<int>(triangleInfos.size()), triangleCount, static_cast<int>(materialInfos.size()) };
					meshPrimitiveInfos.push_back(primitiveInfo);
					for (int indexIndex = 0; indexIndex < primitive.indices.size() / primitive.indexSize; indexIndex += 3) {
						TriangleInfo triangleInfo = {};
						for (int triangleIndex = 0; triangleIndex < 3; triangleIndex += 1) {
							char* indexPtr = &primitive.indices[(indexIndex + triangleIndex) * primitive.indexSize];
							unsigned int index = 0;
							if (primitive.indexSize == 2) {
								index = *reinterpret_cast<unsigned short*>(indexPtr);
							}
							else {
								index = *reinterpret_cast<unsigned int*>(indexPtr);
							}
							ModelVertex& vertex = primitive.vertices[index];
							triangleInfo.normals[triangleIndex] = vertex.normal;
							triangleInfo.texCoords[triangleIndex] = vertex.texCoords;
							triangleInfo.tangents[triangleIndex] = vertex.tangent;
						}
						triangleInfos.push_back(triangleInfo);
					}
				}
				modelPrimitiveInfos.push_back(std::move(meshPrimitiveInfos));
			}
			primitiveInfos.push_back(std::move(modelPrimitiveInfos));
			for (auto& material : model.materials) {
				MaterialInfo materialInfo = { material.baseColorFactor, imageCount + material.baseColorTextureIndex, material.baseColorTextureSamplerIndex, -1, -1 };
				if (material.normalTextureIndex >= 0) {
					materialInfo.normalTextureIndex = imageCount + material.normalTextureIndex;
					materialInfo.normalTextureSamplerIndex = material.normalTextureSamplerIndex;
				}
				materialInfos.push_back(materialInfo);
			}
			imageCount += static_cast<int>(model.images.size());
		}
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> tlasInstanceDescs;
		std::vector<InstanceInfo> instanceInfos;
		int modelIndex = 0;
		for (auto& [modelName, model] : models) {
			std::stack<std::pair<int, DirectX::XMMATRIX>> nodeStack;
			for (auto& nodeIndex : model.rootNodes) {
				nodeStack.push(std::make_pair(nodeIndex, model.nodes[nodeIndex].transform));
			}
			while (!nodeStack.empty()) {
				std::pair<int, DirectX::XMMATRIX> node = nodeStack.top();
				nodeStack.pop();
				ModelNode* modelNode = &model.nodes[node.first];
				if (modelNode->meshIndex >= 0) {
					ModelMesh& mesh = model.meshes[modelNode->meshIndex];
					for (int primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); primitiveIndex += 1) {
						ModelPrimitive& primitive = mesh.primitives[primitiveIndex];
						D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
						DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4*>(instanceDesc.Transform), node.second);
						instanceDesc.InstanceMask = 0xff;
						instanceDesc.AccelerationStructure = primitive.blasBuffer->GetGPUVirtualAddress();
						tlasInstanceDescs.push_back(instanceDesc);
						InstanceInfo instanceInfo = {};
						instanceInfo.transformMat = node.second;
						PrimitiveInfo& primitiveInfo = primitiveInfos[modelIndex][modelNode->meshIndex][primitiveIndex];
						instanceInfo.triangleOffset = primitiveInfo.triangleOffset;
						instanceInfo.triangleCount = primitiveInfo.triangleCount;
						instanceInfo.materialIndex = primitiveInfo.materialOffset + primitive.materialIndex;
						instanceInfos.push_back(instanceInfo);
					}
				}
				for (int childIndex : modelNode->children) {
					DirectX::XMMATRIX childTransform = XMMatrixMultiply(node.second, model.nodes[childIndex].transform);
					nodeStack.push(std::make_pair(childIndex, childTransform));
				}
			}
			modelIndex += 1;
		}

		instanceCount = static_cast<int>(instanceInfos.size());
		triangleCount = static_cast<int>(triangleInfos.size());
		materialCount = static_cast<int>(materialInfos.size());
		instanceInfosBuffer = dx12->createBuffer(instanceInfos.size() * sizeof(InstanceInfo), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		triangleInfosBuffer = dx12->createBuffer(triangleInfos.size() * sizeof(TriangleInfo), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		materialInfosBuffer = dx12->createBuffer(materialInfos.size() * sizeof(MaterialInfo), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		instanceInfosBuffer->SetName(L"instanceInfosBuffer");
		triangleInfosBuffer->SetName(L"triangleInfosBuffer");
		materialInfosBuffer->SetName(L"materialInfosBuffer");
		void* instanceInfosBufferPtr = nullptr;
		void* triangleInfosBufferPtr = nullptr;
		void* materialInfosBufferPtr = nullptr;
		instanceInfosBuffer->Map(0, nullptr, &instanceInfosBufferPtr);
		triangleInfosBuffer->Map(0, nullptr, &triangleInfosBufferPtr);
		materialInfosBuffer->Map(0, nullptr, &materialInfosBufferPtr);
		memcpy(instanceInfosBufferPtr, instanceInfos.data(), instanceInfos.size() * sizeof(InstanceInfo));
		memcpy(triangleInfosBufferPtr, triangleInfos.data(), triangleInfos.size() * sizeof(TriangleInfo));
		memcpy(materialInfosBufferPtr, materialInfos.data(), materialInfos.size() * sizeof(MaterialInfo));
		instanceInfosBuffer->Unmap(0, nullptr);
		triangleInfosBuffer->Unmap(0, nullptr);
		materialInfosBuffer->Unmap(0, nullptr);

		ID3D12Resource* tlasInstanceDescsBuffer = dx12->createBuffer(tlasInstanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* instanceDescsBuffer = nullptr;
		tlasInstanceDescsBuffer->Map(0, nullptr, &instanceDescsBuffer);
		memcpy(instanceDescsBuffer, tlasInstanceDescs.data(), tlasInstanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
		tlasInstanceDescsBuffer->Unmap(0, nullptr);

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
		tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
		tlasInputs.NumDescs = static_cast<UINT>(tlasInstanceDescs.size());
		tlasInputs.InstanceDescs = tlasInstanceDescsBuffer->GetGPUVirtualAddress();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
		dx12->device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);
		ID3D12Resource* tlasScratchBuffer = dx12->createBuffer(tlasPrebuildInfo.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
		tlasBuffer = dx12->createBuffer(tlasPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
		tlasBuffer->SetName(L"topAccelerationStructureBuffer");

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {};
		tlasDesc.DestAccelerationStructureData = tlasBuffer->GetGPUVirtualAddress();
		tlasDesc.Inputs = tlasInputs;
		tlasDesc.ScratchAccelerationStructureData = tlasScratchBuffer->GetGPUVirtualAddress();

		dx12->setCurrentCommandList();
		ID3D12GraphicsCommandList4* cmdList = dx12->graphicsCommandLists[dx12->currentGraphicsCommandListIndex].list;
		cmdList->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);
		dx12->executeCurrentCommandList();
		dx12->drainGraphicsCommandQueue();

		tlasInstanceDescsBuffer->Release();
	}
};