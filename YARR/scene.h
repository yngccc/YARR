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

#define USE_PIX
#include "pix3.h"

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
	DX12Texture image;
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
		Entity,
		EndOfFile
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

	void getInfo(SceneInfo& info) {
		Token token;
		getToken(token);
		if (token.type == Token::EndOfFile) {
			info.type = SceneInfo::EndOfFile;
			return;
		}
		else if (token.type == Token::Identifier) {
			if (token.str == "Camera") {
				float numbers[6] = {};
				for (float& number : numbers) {
					getToken(token);
					token.toFloat(number);
				}
				info.type = SceneInfo::Camera;
				info.camera.position = DirectX::XMVectorSet(numbers[0], numbers[1], numbers[2], 0);
				info.camera.lookAt = DirectX::XMVector3Normalize(DirectX::XMVectorSet(numbers[3], numbers[4], numbers[5], 0));
				info.camera.up = DirectX::XMVectorSet(0, 1, 0, 0);
				info.camera.pitchAngle = DirectX::XMVectorGetX(DirectX::XMVector3AngleBetweenVectors(info.camera.lookAt, DirectX::XMVectorSet(0, 1, 0, 0)));
				info.camera.pitchAngle = static_cast<float>(M_PI_2) - info.camera.pitchAngle;
			}
			else if (token.str == "Model") {
				info.type = SceneInfo::Model;
				getToken(token);
				if (token.type != Token::String) {
					throw Exception("SceneParser::getInfo error: model name token not a string");
				}
				info.name = token.str;
				getToken(token);
				if (token.type != Token::String) {
					throw Exception("SceneParser::getInfo error: model filePath token not a string");
				}
				info.path = token.str;
			}
			else if (token.str == "Entity") {
				info.type = SceneInfo::Entity;
				getToken(token);
				if (token.type != Token::String) {
					throw Exception("SceneParser::getInfo error: entity name token not a string");
				}
				info.name = token.str;
				float numbers[10] = {};
				for (float& number : numbers) {
					getToken(token);
					token.toFloat(number);
				}
			}
			else {
				throw Exception("SceneParser::getInfo error: unknown identifer token \"" + std::string(token.str) + "\"");
			}
		}
		else {
			throw Exception("SceneParser::getInfo error: first token is not an identifier");
		}
	}
};

struct Scene {
	Camera camera;
	std::unordered_map<std::string, Model> models;
	DX12Buffer tlasBuffer;
	DX12Buffer instanceInfosBuffer;
	DX12Buffer triangleInfosBuffer;
	DX12Buffer materialInfosBuffer;
	int instanceCount = 0;
	int triangleCount = 0;
	int materialCount = 0;
	std::string name;
	std::filesystem::path filePath;

	Scene(const std::string& sceneName) : name(sceneName) {
	}
	Scene(const std::string& sceneName, const std::filesystem::path& sceneFilePath, DX12Context& dx12) : name(sceneName), filePath(sceneFilePath) {
		setCurrentDirToExeDir();
		SceneParser parser(sceneFilePath);
		SceneInfo info;
		while (true) {
			parser.getInfo(info);
			if (info.type == SceneInfo::Camera) {
				camera = info.camera;
			}
			else if (info.type == SceneInfo::Model) {
				models.insert({ std::string(info.name), createModelFromGLTF(info.path, dx12) });
			}
			else if (info.type == SceneInfo::Entity) {
			}
			else if (info.type == SceneInfo::EndOfFile) {
				break;
			}
			else {
				assert(false);
			}
		}
	}
	
	Model createModelFromGLTF(const std::filesystem::path& gltfFilePath, DX12Context& dx12) {
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

				DX12Buffer indicesBuffer = dx12.createBuffer(modelPrimitive.indices.size(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
				DX12Buffer vertexPositionsBuffer = dx12.createBuffer(modelPrimitive.vertices.size() * 12, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
				char* indicesBufferPtr = nullptr;
				dx12Assert(indicesBuffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&indicesBufferPtr)));
				memcpy(indicesBufferPtr, modelPrimitive.indices.data(), modelPrimitive.indices.size());
				indicesBuffer.buffer->Unmap(0, nullptr);
				char* vertexPositionsBufferPtr = nullptr;
				vertexPositionsBuffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexPositionsBufferPtr));
				for (auto& vertex : modelPrimitive.vertices) {
					memcpy(vertexPositionsBufferPtr, vertex.position.data(), 12);
					vertexPositionsBufferPtr += 12;
				}
				vertexPositionsBuffer.buffer->Unmap(0, nullptr);

				D3D12_RAYTRACING_GEOMETRY_DESC meshGeometryDesc = {};
				meshGeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				meshGeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE; // D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
				meshGeometryDesc.Triangles.IndexFormat = (modelPrimitive.indexSize == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
				meshGeometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				meshGeometryDesc.Triangles.IndexCount = static_cast<UINT>(modelPrimitive.indices.size() / modelPrimitive.indexSize);
				meshGeometryDesc.Triangles.VertexCount = static_cast<UINT>(modelPrimitive.vertices.size());
				meshGeometryDesc.Triangles.IndexBuffer = indicesBuffer.buffer->GetGPUVirtualAddress();
				meshGeometryDesc.Triangles.VertexBuffer = { vertexPositionsBuffer.buffer->GetGPUVirtualAddress(), 12 };

				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInput = {};
				blasInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
				blasInput.NumDescs = 1;
				blasInput.pGeometryDescs = &meshGeometryDesc;

				D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo;
				dx12.device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInput, &blasPrebuildInfo);

				DX12Buffer blasScratchBuffer = dx12.createBuffer(blasPrebuildInfo.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
				DX12Buffer blasBuffer = dx12.createBuffer(blasPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
				blasBuffer.buffer->SetName(L"bottomAccelerationStructureBuffer");

				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
				blasDesc.DestAccelerationStructureData = blasBuffer.buffer->GetGPUVirtualAddress();
				blasDesc.Inputs = blasInput;
				blasDesc.ScratchAccelerationStructureData = blasScratchBuffer.buffer->GetGPUVirtualAddress();

				dx12.waitAndResetGraphicsCommandList();
				DX12CommandList& cmdList = dx12.graphicsCommandLists[dx12.currentFrame];
				cmdList.list->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);
				dx12.closeAndExecuteGraphicsCommandList();
				dx12.waitAndResetGraphicsCommandList();

				modelPrimitive.blasBuffer = blasBuffer.buffer;

				blasScratchBuffer.buffer->Release();
				indicesBuffer.buffer->Release();
				vertexPositionsBuffer.buffer->Release();

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
			DX12Texture image = dx12.createTexture(gltfImage.width, gltfImage.height, 1, 1, format, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
			DX12TextureCopy textureCopy = {
				image, reinterpret_cast<char*>(gltfImage.image.data()), gltfImage.image.size(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			};
			dx12.copyTextures({ textureCopy });
			std::wstring name(gltfImage.uri.begin(), gltfImage.uri.end());
			image.texture->SetName(name.c_str());
			model.images.push_back(ModelImage{ image });
		}
		return model;
	}
	void rebuildTLAS(DX12Context& dx12) {
		if (models.empty()) {
			return;
		}
		if (tlasBuffer.buffer) {
			tlasBuffer.buffer->Release();
		}
		if (instanceInfosBuffer.buffer) {
			instanceInfosBuffer.buffer->Release();
		}
		if (triangleInfosBuffer.buffer) {
			triangleInfosBuffer.buffer->Release();
		}
		if (materialInfosBuffer.buffer) {
			materialInfosBuffer.buffer->Release();
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
		instanceInfosBuffer = dx12.createBuffer(instanceInfos.size() * sizeof(InstanceInfo), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		triangleInfosBuffer = dx12.createBuffer(triangleInfos.size() * sizeof(TriangleInfo), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		materialInfosBuffer = dx12.createBuffer(materialInfos.size() * sizeof(MaterialInfo), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		instanceInfosBuffer.buffer->SetName(L"instanceInfosBuffer");
		triangleInfosBuffer.buffer->SetName(L"triangleInfosBuffer");
		materialInfosBuffer.buffer->SetName(L"materialInfosBuffer");
		void* instanceInfosBufferPtr = nullptr;
		void* triangleInfosBufferPtr = nullptr;
		void* materialInfosBufferPtr = nullptr;
		instanceInfosBuffer.buffer->Map(0, nullptr, &instanceInfosBufferPtr);
		triangleInfosBuffer.buffer->Map(0, nullptr, &triangleInfosBufferPtr);
		materialInfosBuffer.buffer->Map(0, nullptr, &materialInfosBufferPtr);
		memcpy(instanceInfosBufferPtr, instanceInfos.data(), instanceInfos.size() * sizeof(InstanceInfo));
		memcpy(triangleInfosBufferPtr, triangleInfos.data(), triangleInfos.size() * sizeof(TriangleInfo));
		memcpy(materialInfosBufferPtr, materialInfos.data(), materialInfos.size() * sizeof(MaterialInfo));
		instanceInfosBuffer.buffer->Unmap(0, nullptr);
		triangleInfosBuffer.buffer->Unmap(0, nullptr);
		materialInfosBuffer.buffer->Unmap(0, nullptr);

		DX12Buffer tlasInstanceDescsBuffer = dx12.createBuffer(tlasInstanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* instanceDescsBuffer = nullptr;
		tlasInstanceDescsBuffer.buffer->Map(0, nullptr, &instanceDescsBuffer);
		memcpy(instanceDescsBuffer, tlasInstanceDescs.data(), tlasInstanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
		tlasInstanceDescsBuffer.buffer->Unmap(0, nullptr);

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
		tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
		tlasInputs.NumDescs = static_cast<UINT>(tlasInstanceDescs.size());
		tlasInputs.InstanceDescs = tlasInstanceDescsBuffer.buffer->GetGPUVirtualAddress();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
		dx12.device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);
		DX12Buffer tlasScratchBuffer = dx12.createBuffer(tlasPrebuildInfo.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
		tlasBuffer = dx12.createBuffer(tlasPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
		tlasBuffer.buffer->SetName(L"topAccelerationStructureBuffer");

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {};
		tlasDesc.DestAccelerationStructureData = tlasBuffer.buffer->GetGPUVirtualAddress();
		tlasDesc.Inputs = tlasInputs;
		tlasDesc.ScratchAccelerationStructureData = tlasScratchBuffer.buffer->GetGPUVirtualAddress();

		dx12.waitAndResetGraphicsCommandList();
		ID3D12GraphicsCommandList4* cmdList = dx12.graphicsCommandLists[dx12.currentFrame].list;
		cmdList->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);
		dx12.closeAndExecuteGraphicsCommandList();
		dx12.waitAndResetGraphicsCommandList();

		tlasInstanceDescsBuffer.buffer->Release();
	}
};