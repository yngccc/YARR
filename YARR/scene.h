#pragma once

#include "dx12.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image.h"
#include "libs/stb_image_write.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include "libs/tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "libs/tiny_obj_loader.h"

struct ModelVertex {
	std::array<float, 3> position;
	std::array<float, 3> normal;
	std::array<float, 2> texCoords;
};

struct ModelPrimitive {
	int materialIndex;
	std::vector<ModelVertex> vertices;
	std::vector<unsigned short> indices;
	ID3D12Resource* bottomAccelStructBuffer;
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
};

struct ModelImage {
	ID3D12Resource* image;
};

struct Model {
	std::vector<ModelNode> nodes;
	std::vector<int> rootNodes;
	std::vector<ModelMesh> meshes;
	std::vector<ModelMaterial> materials;
	std::vector<ModelImage> images;
	std::string name;
};

struct InstanceInfo {
	DirectX::XMMATRIX transformMat;
	int triangleOffset;
	int triangleCount;
	int materialIndex;
};

struct TriangleInfo {
	std::array<std::array<float, 3>, 3> normals;
	std::array<std::array<float, 2>, 3> texCoords;
};

struct MaterialInfo {
	std::array<float, 4> baseColorFactor;
	int baseColorTextureIndex;
};

struct Scene {
	std::vector<Model> models;
	ID3D12Resource* topAccelStructBuffer;
	ID3D12Resource* instanceInfosBuffer;
	ID3D12Resource* triangleInfosBuffer;
	ID3D12Resource* materialInfosBuffer;
	int instanceCount;
	int triangleCount;
	int materialCount;

	static Scene create() {
		Scene scene = {};
		return scene;
	}
	void createModelFromGLTF(DX12Context* dx12, const char* gltfFileName) {
		tinygltf::TinyGLTF gltfLoader;
		std::string gltfLoadError;
		std::string gltfLoadWarning;
		tinygltf::Model gltfModel;
		bool loadResult = gltfLoader.LoadASCIIFromFile(&gltfModel, &gltfLoadError, &gltfLoadWarning, gltfFileName);
		assert(loadResult);
		assert(gltfModel.scenes.size() == 1);

		Model model = {};
		model.name = gltfModel.scenes[0].name;
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

				assert(gltfPrimitive.mode == TINYGLTF_MODE_TRIANGLES);
				int positionAccessorIndex = gltfPrimitive.attributes.at("POSITION");
				auto& positionAccessor = gltfModel.accessors[positionAccessorIndex];
				auto& positionBufferView = gltfModel.bufferViews[positionAccessor.bufferView];
				auto& positionBuffer = gltfModel.buffers[positionBufferView.buffer];
				unsigned char* positionData = &positionBuffer.data[positionAccessor.byteOffset + positionBufferView.byteOffset];
				assert(positionAccessor.type == TINYGLTF_TYPE_VEC3 && positionAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && positionBufferView.byteStride == 0);

				int normalAccessorIndex = gltfPrimitive.attributes.at("NORMAL");
				auto& normalAccessor = gltfModel.accessors[normalAccessorIndex];
				auto& normalBufferView = gltfModel.bufferViews[normalAccessor.bufferView];
				auto& normalBuffer = gltfModel.buffers[normalBufferView.buffer];
				unsigned char* normalData = &normalBuffer.data[normalAccessor.byteOffset + normalBufferView.byteOffset];
				assert(normalAccessor.type == TINYGLTF_TYPE_VEC3 && normalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && normalBufferView.byteStride == 0);
				assert(normalAccessor.count == positionAccessor.count);

				int uvAccessorIndex = gltfPrimitive.attributes.at("TEXCOORD_0");
				auto& uvAccessor = gltfModel.accessors[uvAccessorIndex];
				auto& uvBufferView = gltfModel.bufferViews[uvAccessor.bufferView];
				auto& uvBuffer = gltfModel.buffers[uvBufferView.buffer];
				unsigned char* uvData = &uvBuffer.data[uvAccessor.byteOffset + uvBufferView.byteOffset];
				assert(uvAccessor.type == TINYGLTF_TYPE_VEC2 && uvAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && uvBufferView.byteStride == 0);
				assert(uvAccessor.count == positionAccessor.count);

				auto& indexAccessor = gltfModel.accessors[gltfPrimitive.indices];
				auto& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
				auto& indexBuffer = gltfModel.buffers[indexBufferView.buffer];
				unsigned char* indexData = &indexBuffer.data[indexAccessor.byteOffset + indexBufferView.byteOffset];
				assert(indexAccessor.count % 3 == 0 && indexAccessor.type == TINYGLTF_TYPE_SCALAR && indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT && indexBufferView.byteStride == 0);

				modelPrimitive.vertices.resize(positionAccessor.count);
				for (size_t i = 0; i < positionAccessor.count; i += 1) {
					memcpy(modelPrimitive.vertices[i].position.data(), positionData + i * 12, 12);
					memcpy(modelPrimitive.vertices[i].normal.data(), normalData + i * 12, 12);
					memcpy(modelPrimitive.vertices[i].texCoords.data(), uvData + i * 8, 8);
				}
				modelPrimitive.indices.resize(indexAccessor.count);
				memcpy(modelPrimitive.indices.data(), indexData, indexAccessor.count * 2);

				ID3D12Resource* indicesBuffer = dx12->createBuffer(modelPrimitive.indices.size() * 2, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
				ID3D12Resource* vertexPositionsBuffer = dx12->createBuffer(modelPrimitive.vertices.size() * 12, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
				char* indicesBufferPtr = nullptr;
				indicesBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indicesBufferPtr));
				memcpy(indicesBufferPtr, modelPrimitive.indices.data(), modelPrimitive.indices.size() * 2);
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
				meshGeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
				meshGeometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
				meshGeometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				meshGeometryDesc.Triangles.IndexCount = static_cast<UINT>(modelPrimitive.indices.size());
				meshGeometryDesc.Triangles.VertexCount = static_cast<UINT>(modelPrimitive.vertices.size());
				meshGeometryDesc.Triangles.IndexBuffer = indicesBuffer->GetGPUVirtualAddress();
				meshGeometryDesc.Triangles.VertexBuffer = { vertexPositionsBuffer->GetGPUVirtualAddress(), 12 };

				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomAccelerationStructInputs = {};
				bottomAccelerationStructInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
				bottomAccelerationStructInputs.NumDescs = 1;
				bottomAccelerationStructInputs.pGeometryDescs = &meshGeometryDesc;

				D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomAccelerationStructPrebuildInfo;
				dx12->device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomAccelerationStructInputs, &bottomAccelerationStructPrebuildInfo);

				ID3D12Resource* scratchAccelerationStructureBuffer = dx12->createBuffer(bottomAccelerationStructPrebuildInfo.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
				ID3D12Resource* bottomAccelerationStructureBuffer = dx12->createBuffer(bottomAccelerationStructPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
				bottomAccelerationStructureBuffer->SetName(L"bottomAccelerationStructureBuffer");

				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomAccelerationStructDesc = {};
				bottomAccelerationStructDesc.DestAccelerationStructureData = bottomAccelerationStructureBuffer->GetGPUVirtualAddress();
				bottomAccelerationStructDesc.Inputs = bottomAccelerationStructInputs;
				bottomAccelerationStructDesc.ScratchAccelerationStructureData = scratchAccelerationStructureBuffer->GetGPUVirtualAddress();

				dx12->setCurrentCommandList();
				ID3D12GraphicsCommandList4* cmdList = dx12->graphicsCommandLists[dx12->currentGraphicsCommandListIndex].list;
				cmdList->BuildRaytracingAccelerationStructure(&bottomAccelerationStructDesc, 0, nullptr);
				dx12->executeCurrentCommandList();
				dx12->drainGraphicsCommandQueue();

				modelPrimitive.bottomAccelStructBuffer = bottomAccelerationStructureBuffer;
				scratchAccelerationStructureBuffer->Release();

				// NOTE: will break nsight acceleration structure tracking if released
				indicesBuffer->Release();
				vertexPositionsBuffer->Release();

				modelMesh.primitives.push_back(std::move(modelPrimitive));
			}
			model.meshes.push_back(std::move(modelMesh));
		}
		model.materials.reserve(gltfModel.materials.size());
		for (auto& gltfMaterial : gltfModel.materials) {
			ModelMaterial material = {};
			memcpy(material.baseColorFactor.data(), gltfMaterial.pbrMetallicRoughness.baseColorFactor.data(), 16);
			material.baseColorTextureIndex = gltfModel.textures[gltfMaterial.pbrMetallicRoughness.baseColorTexture.index].source;
			assert(material.baseColorTextureIndex >= 0 && material.baseColorTextureIndex < gltfModel.materials.size());
			assert(gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord == 0);
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
				format = DXGI_FORMAT_R8G8B8A8_UNORM;
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

		models.push_back(std::move(model));
	}
	void rebuildTopAccelStruct(DX12Context* dx12) {
		if (models.size() == 0) {
			return;
		}
		if (topAccelStructBuffer) {
			topAccelStructBuffer->Release();
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
			int imageOffset;
		};
		std::vector<std::vector<std::vector<PrimitiveInfo>>> primitiveInfos;
		std::vector<TriangleInfo> triangleInfos;
		std::vector<MaterialInfo> materialInfos;
		int imageCount = 0;
		for (auto& model : models) {
			std::vector<std::vector<PrimitiveInfo>> modelPrimitiveInfos;
			for (auto& mesh : model.meshes) {
				std::vector<PrimitiveInfo> meshPrimitiveInfos;
				for (auto& primitive : mesh.primitives) {
					int triangleCount = static_cast<int>(primitive.indices.size()) / 3;
					PrimitiveInfo primitiveInfo = {
						static_cast<int>(triangleInfos.size()), triangleCount,
						static_cast<int>(materialInfos.size()), imageCount
					};
					meshPrimitiveInfos.push_back(primitiveInfo);
					ModelMaterial& modelMaterial = model.materials[primitive.materialIndex];
					materialInfos.push_back(MaterialInfo{ modelMaterial.baseColorFactor, imageCount + modelMaterial.baseColorTextureIndex });
					for (int indexIndex = 0; indexIndex < primitive.indices.size(); indexIndex += 3) {
						TriangleInfo triangleInfo = {};
						for (int triangleIndex = 0; triangleIndex < 3; triangleIndex += 1) {
							ModelVertex& vertex = primitive.vertices[primitive.indices[indexIndex + triangleIndex]];
							triangleInfo.normals[triangleIndex] = vertex.normal;
							triangleInfo.texCoords[triangleIndex] = vertex.texCoords;
						}
						triangleInfos.push_back(triangleInfo);
					}
				}
				modelPrimitiveInfos.push_back(std::move(meshPrimitiveInfos));
			}
			primitiveInfos.push_back(std::move(modelPrimitiveInfos));
			imageCount += static_cast<int>(model.images.size());
		}
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> topAccelStructInstanceDescs;
		std::vector<InstanceInfo> instanceInfos;
		for (int modelIndex = 0; modelIndex < models.size(); modelIndex += 1) {
			Model& model = models[modelIndex];
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
						instanceDesc.AccelerationStructure = primitive.bottomAccelStructBuffer->GetGPUVirtualAddress();
						topAccelStructInstanceDescs.push_back(instanceDesc);
						InstanceInfo instanceInfo = {};
						instanceInfo.transformMat = node.second;
						PrimitiveInfo& primitiveInfo = primitiveInfos[modelIndex][modelNode->meshIndex][primitiveIndex];
						instanceInfo.triangleOffset = primitiveInfo.triangleCount;
						instanceInfo.triangleCount = primitiveInfo.triangleOffset;
						instanceInfo.materialIndex = primitiveInfo.materialOffset + primitive.materialIndex;
						instanceInfos.push_back(instanceInfo);
					}
				}
				for (int childIndex : modelNode->children) {
					DirectX::XMMATRIX childTransform = XMMatrixMultiply(node.second, model.nodes[childIndex].transform);
					nodeStack.push(std::make_pair(childIndex, childTransform));
				}
			}
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

		ID3D12Resource* topAccelerationStructInstanceDescsBuffer = dx12->createBuffer(topAccelStructInstanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* instanceDescsBuffer = nullptr;
		topAccelerationStructInstanceDescsBuffer->Map(0, nullptr, &instanceDescsBuffer);
		memcpy(instanceDescsBuffer, topAccelStructInstanceDescs.data(), topAccelStructInstanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
		topAccelerationStructInstanceDescsBuffer->Unmap(0, nullptr);

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topAccelerationStructInputs = {};
		topAccelerationStructInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topAccelerationStructInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
		topAccelerationStructInputs.NumDescs = static_cast<UINT>(topAccelStructInstanceDescs.size());
		topAccelerationStructInputs.InstanceDescs = topAccelerationStructInstanceDescsBuffer->GetGPUVirtualAddress();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topAccelerationStructPrebuildInfo;
		dx12->device->GetRaytracingAccelerationStructurePrebuildInfo(&topAccelerationStructInputs, &topAccelerationStructPrebuildInfo);
		ID3D12Resource* scratchAccelerationBuffer = dx12->createBuffer(topAccelerationStructPrebuildInfo.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
		topAccelStructBuffer = dx12->createBuffer(topAccelerationStructPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
		topAccelStructBuffer->SetName(L"topAccelerationStructureBuffer");

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topAccelerationStructDesc = {};
		topAccelerationStructDesc.DestAccelerationStructureData = topAccelStructBuffer->GetGPUVirtualAddress();
		topAccelerationStructDesc.Inputs = topAccelerationStructInputs;
		topAccelerationStructDesc.ScratchAccelerationStructureData = scratchAccelerationBuffer->GetGPUVirtualAddress();

		dx12->setCurrentCommandList();
		ID3D12GraphicsCommandList4* cmdList = dx12->graphicsCommandLists[dx12->currentGraphicsCommandListIndex].list;
		cmdList->BuildRaytracingAccelerationStructure(&topAccelerationStructDesc, 0, nullptr);
		dx12->executeCurrentCommandList();
		dx12->drainGraphicsCommandQueue();

		topAccelerationStructInstanceDescsBuffer->Release();
	}
};