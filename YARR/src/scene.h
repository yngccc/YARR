#pragma once

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdparty/include/stb_image.h"
#include "../thirdparty/include/stb_image_write.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include "../thirdparty/include/tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../thirdparty/include/tiny_obj_loader.h"

#include "dx12.h"

struct ModelVertex {
	float position[3];
	float normal[3];
	float uv[2];
	float tangent[3];
};

struct ModelPrimitive {
	std::vector<ModelVertex> vertices;
	std::vector<uint8> indices;
	DX12Buffer vertexBuffer;
	DX12Buffer indexBuffer;
	int indexSize = 2;
	int materialIndex = -1;
};

struct ModelMesh {
	std::string name;
	std::vector<ModelPrimitive> primitives;
	DX12Buffer blasBuffer;
};

struct ModelNode {
	int meshIndex;
	DirectX::XMMATRIX transform;
	std::vector<int> children;
};

struct ModelMaterial {
	float baseColorFactor[4] = {};
	int baseColorTextureIndex = -1;
	int baseColorTextureSamplerIndex = -1;
	int normalTextureIndex = -1;
	int normalTextureSamplerIndex = -1;
	float emissiveFactor[3] = {};
	int emissiveTextureIndex = -1;
	int emissiveTextureSamplerIndex = -1;
	float alphaCutoff = 0.5;
};

struct Model {
	std::vector<ModelNode> nodes;
	std::vector<int> rootNodes;
	std::vector<ModelMesh> meshes;
	std::vector<ModelMaterial> materials;
	std::vector<DX12Texture> textures;
	std::filesystem::path filePath;
};

struct Camera {
	DirectX::XMVECTOR position = DirectX::XMVectorSet(0, 0, 0, 0);
	DirectX::XMVECTOR lookAt = DirectX::XMVectorSet(0, 0, 1, 0);
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);
	float pitchAngle = 0;
	DirectX::XMMATRIX viewMat;
	DirectX::XMMATRIX projMat;
	DirectX::XMMATRIX viewProjMat;
};

struct SceneInfo {
	enum Type {
		Camera,
		Model,
		Entity,
		DirectionalLight,
		PointLight,
		EndOfFile
	};
	Type type;
	std::string_view name;
	std::string_view path;
	float rotation[4];
	union {
		float scaling[3];
		float cameraPosition[3];
		float lightDirection[3];
		float lightPosition[3];
	};
	union {
		float translation[3];
		float cameraLookAt[3];
		float lightColor[3];
	};
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
				info.type = SceneInfo::Camera;
				for (auto& p : info.cameraPosition) {
					getToken(token);
					token.toFloat(p);
				}
				for (auto& l : info.cameraLookAt) {
					getToken(token);
					token.toFloat(l);
				}
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
			else if (token.str == "DirectionalLight") {
				info.type = SceneInfo::DirectionalLight;
				for (auto& d : info.lightDirection) {
					getToken(token);
					token.toFloat(d);
				}
				for (auto& c : info.lightColor) {
					getToken(token);
					token.toFloat(c);
				}
			}
			else if (token.str == "PointLight") {
				info.type = SceneInfo::PointLight;
				for (auto& d : info.lightPosition) {
					getToken(token);
					token.toFloat(d);
				}
				for (auto& c : info.lightColor) {
					getToken(token);
					token.toFloat(c);
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

#include "../hlsl/sceneStructs.hlsli"

struct Scene {
	Camera camera;
	std::unordered_map<std::string, Model> models;
	std::vector<SceneLight> lights;
	DX12Buffer tlasBuffer;
	DX12Buffer instanceInfosBuffer;
	DX12Buffer geometryInfosBuffer;
	DX12Buffer triangleInfosBuffer;
	DX12Buffer materialInfosBuffer;
	uint64 instanceInfoCount = 0;
	uint64 geometryInfoCount = 0;
	uint64 triangleInfoCount = 0;
	uint64 materialInfoCount = 0;
	std::string name;
	std::filesystem::path filePath;

	Scene(const std::string& sceneName) : name(sceneName) {}
	Scene(const std::string& sceneName, const std::filesystem::path& sceneFilePath, DX12Context& dx12) : name(sceneName), filePath(sceneFilePath) {
		setCurrentDirToExeDir();
		SceneParser parser(sceneFilePath);
		SceneInfo info;
		while (true) {
			parser.getInfo(info);
			if (info.type == SceneInfo::Camera) {
				camera.position = DirectX::XMVectorSet(info.cameraPosition[0], info.cameraPosition[1], info.cameraPosition[2], 0);
				camera.lookAt = DirectX::XMVectorSet(info.cameraLookAt[0], info.cameraLookAt[1], info.cameraLookAt[2], 0);
				camera.up = DirectX::XMVectorSet(0, 1, 0, 0);
				DirectX::XMVECTOR view = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(camera.lookAt, camera.position));
				camera.pitchAngle = DirectX::XMVectorGetX(DirectX::XMVector3AngleBetweenVectors(view, DirectX::XMVectorSet(0, 1, 0, 0)));
				camera.pitchAngle = static_cast<float>(M_PI_2) - camera.pitchAngle;
			}
			else if (info.type == SceneInfo::Model) {
				std::filesystem::path path = info.path;
				std::filesystem::path extension = path.extension();
				if (extension == ".gltf") {
					Model model = createModelFromGLTF(path, dx12);
					models.insert({ std::string(info.name), model });
				}
				else if (extension == ".fbx") {
					assert(false && "not implemented");
				}
				else {
					throw Exception("unknown model file format: " + extension.string() + "\n");
				}
			}
			else if (info.type == SceneInfo::Entity) {
			}
			else if (info.type == SceneInfo::DirectionalLight) {
				SceneLight l;
				l.type = DIRECTIONAL_LIGHT;
				arrayCopy(l.direction, info.lightDirection);
				arrayCopy(l.color, info.lightColor);
				lights.push_back(l);
			}
			else if (info.type == SceneInfo::PointLight) {
				SceneLight l;
				l.type = POINT_LIGHT;
				arrayCopy(l.position, info.lightPosition);
				arrayCopy(l.color, info.lightColor);
				lights.push_back(l);
			}
			else if (info.type == SceneInfo::EndOfFile) {
				break;
			}
			else {
				assert(false && "unknown SceneInfo::Type");
			}
		}
	}
	void writeToFile() {
		std::stringstream strStream;
		strStream
			<< "Camera: ["
			<< DirectX::XMVectorGetX(camera.position) << " " << DirectX::XMVectorGetY(camera.position) << " " << DirectX::XMVectorGetZ(camera.position)
			<< "] ["
			<< DirectX::XMVectorGetX(camera.lookAt) << " " << DirectX::XMVectorGetY(camera.lookAt) << " " << DirectX::XMVectorGetZ(camera.lookAt)
			<< "]\n";
		for (auto& [name, model] : models) {
			strStream << "Model: \"" << name << "\" " << model.filePath << "\n";
		}
		for (auto& light : lights) {
			if (light.type == DIRECTIONAL_LIGHT) {
				strStream << "DirectionalLight: ["
					<< light.direction[0] << " " << light.direction[1] << " " << light.direction[2] << "] ["
					<< light.color[0] << " " << light.color[1] << " " << light.color[2] << "]\n";
			}
			else if (light.type == POINT_LIGHT) {
				strStream << "PointLight: ["
					<< light.position[0] << " " << light.position[1] << " " << light.position[2] << "] ["
					<< light.color[0] << " " << light.color[1] << " " << light.color[2] << "]\n";
			}
		}
		writeFile(filePath, strStream.str());
	}
	void deleteGPUResources() {
		assert(false && "TODO: implement");
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
		model.filePath = gltfFilePath;
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

				auto positionAttribute = gltfPrimitive.attributes.find("POSITION");
				assert(positionAttribute != gltfPrimitive.attributes.end());
				auto& positionAccessor = gltfModel.accessors[positionAttribute->second];
				auto& positionBufferView = gltfModel.bufferViews[positionAccessor.bufferView];
				auto& positionBuffer = gltfModel.buffers[positionBufferView.buffer];
				unsigned char* positionData = &positionBuffer.data[positionAccessor.byteOffset + positionBufferView.byteOffset];
				assert(positionAccessor.type == TINYGLTF_TYPE_VEC3 && positionAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && (positionBufferView.byteStride == 0 || positionBufferView.byteStride == 12));
				assert(positionAccessor.count < USHRT_MAX);

				auto normalAttribute = gltfPrimitive.attributes.find("NORMAL");
				assert(normalAttribute != gltfPrimitive.attributes.end());
				auto& normalAccessor = gltfModel.accessors[normalAttribute->second];
				auto& normalBufferView = gltfModel.bufferViews[normalAccessor.bufferView];
				auto& normalBuffer = gltfModel.buffers[normalBufferView.buffer];
				unsigned char* normalData = &normalBuffer.data[normalAccessor.byteOffset + normalBufferView.byteOffset];
				assert(normalAccessor.type == TINYGLTF_TYPE_VEC3 && normalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && (normalBufferView.byteStride == 0 || normalBufferView.byteStride == 12));
				assert(normalAccessor.count == positionAccessor.count);

				auto uvAttribute = gltfPrimitive.attributes.find("TEXCOORD_0");
				unsigned char* uvData = nullptr;
				if (uvAttribute != gltfPrimitive.attributes.end()) {
					auto& uvAccessor = gltfModel.accessors[uvAttribute->second];
					auto& uvBufferView = gltfModel.bufferViews[uvAccessor.bufferView];
					auto& uvBuffer = gltfModel.buffers[uvBufferView.buffer];
					uvData = &uvBuffer.data[uvAccessor.byteOffset + uvBufferView.byteOffset];
					assert(uvAccessor.type == TINYGLTF_TYPE_VEC2 && uvAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && (uvBufferView.byteStride == 0 || uvBufferView.byteStride == 8));
					assert(uvAccessor.count == positionAccessor.count);
				}
				else {
					assert(gltfModel.materials[gltfPrimitive.material].pbrMetallicRoughness.baseColorTexture.index < 0);
					assert(gltfModel.materials[gltfPrimitive.material].normalTexture.index < 0);
				}

				auto tangentAttrib = gltfPrimitive.attributes.find("TANGENT");
				unsigned char* tangentData = nullptr;
				if (tangentAttrib != gltfPrimitive.attributes.end()) {
					int tangentAccessorIndex = tangentAttrib->second;
					auto& tangentAccessor = gltfModel.accessors[tangentAccessorIndex];
					auto& tangentBufferView = gltfModel.bufferViews[tangentAccessor.bufferView];
					auto& tangentBuffer = gltfModel.buffers[tangentBufferView.buffer];
					tangentData = &tangentBuffer.data[tangentAccessor.byteOffset + tangentBufferView.byteOffset];
					assert(tangentAccessor.type == TINYGLTF_TYPE_VEC4 && tangentAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && (tangentBufferView.byteStride == 0 || tangentBufferView.byteStride == 16));
					assert(tangentAccessor.count == positionAccessor.count);
				}
				else {
					assert(gltfModel.materials[gltfPrimitive.material].normalTexture.index < 0);
				}

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
				for (uint64 i = 0; i < positionAccessor.count; i += 1) {
					memcpy(modelPrimitive.vertices[i].position, positionData + i * 12, 12);
					memcpy(modelPrimitive.vertices[i].normal, normalData + i * 12, 12);
					if (uvData) {
						memcpy(modelPrimitive.vertices[i].uv, uvData + i * 8, 8);
					}
					if (tangentData) {
						memcpy(modelPrimitive.vertices[i].tangent, tangentData + i * 16, 12);
					}
				}
				modelPrimitive.indices.resize(indexAccessor.count * modelPrimitive.indexSize);
				memcpy(modelPrimitive.indices.data(), indexData, indexAccessor.count * modelPrimitive.indexSize);

				modelPrimitive.vertexBuffer = dx12.createBuffer(modelPrimitive.vertices.size() * sizeof(ModelVertex), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
				modelPrimitive.indexBuffer = dx12.createBuffer(modelPrimitive.indices.size(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
				uint8* vertexBufferPtr = nullptr;
				uint8* indexBufferPtr = nullptr;
				d3dAssert(modelPrimitive.vertexBuffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexBufferPtr)));
				d3dAssert(modelPrimitive.indexBuffer.buffer->Map(0, nullptr, reinterpret_cast<void**>(&indexBufferPtr)));
				memcpy(vertexBufferPtr, modelPrimitive.vertices.data(), modelPrimitive.vertices.size() * sizeof(ModelVertex));
				memcpy(indexBufferPtr, modelPrimitive.indices.data(), modelPrimitive.indices.size());
				modelPrimitive.vertexBuffer.buffer->Unmap(0, nullptr);
				modelPrimitive.indexBuffer.buffer->Unmap(0, nullptr);

				modelMesh.primitives.push_back(std::move(modelPrimitive));
			}

			std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> primitiveGeometryDescs;
			primitiveGeometryDescs.reserve(modelMesh.primitives.size());
			for (auto& primitive : modelMesh.primitives) {
				D3D12_RAYTRACING_GEOMETRY_DESC primitiveGeometryDesc = {};
				primitiveGeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				if (gltfModel.materials[primitive.materialIndex].alphaMode == "OPAQUE") {
					primitiveGeometryDesc.Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
				}
				primitiveGeometryDesc.Triangles.IndexFormat = (primitive.indexSize == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
				primitiveGeometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				primitiveGeometryDesc.Triangles.IndexCount = static_cast<UINT>(primitive.indices.size() / primitive.indexSize);
				primitiveGeometryDesc.Triangles.VertexCount = static_cast<UINT>(primitive.vertices.size());
				primitiveGeometryDesc.Triangles.IndexBuffer = primitive.indexBuffer.buffer->GetGPUVirtualAddress();
				primitiveGeometryDesc.Triangles.VertexBuffer = { primitive.vertexBuffer.buffer->GetGPUVirtualAddress(), sizeof(ModelVertex) };
				primitiveGeometryDescs.push_back(primitiveGeometryDesc);
			}

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInput = {};
			blasInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			blasInput.NumDescs = primitiveGeometryDescs.size();
			blasInput.pGeometryDescs = primitiveGeometryDescs.data();

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo;
			dx12.device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInput, &blasPrebuildInfo);

			DX12Buffer blasScratchBuffer = dx12.createBuffer(blasPrebuildInfo.ScratchDataSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
			modelMesh.blasBuffer = dx12.createBuffer(blasPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
			modelMesh.blasBuffer.buffer->SetName(L"bottomAccelerationStructureBuffer");

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
			blasDesc.DestAccelerationStructureData = modelMesh.blasBuffer.buffer->GetGPUVirtualAddress();
			blasDesc.Inputs = blasInput;
			blasDesc.ScratchAccelerationStructureData = blasScratchBuffer.buffer->GetGPUVirtualAddress();

			DX12CommandList& cmdList = dx12.graphicsCommandLists[dx12.currentFrame];
			cmdList.list->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);

			dx12.closeAndExecuteCommandList(dx12.graphicsCommandLists[dx12.currentFrame]);
			dx12.waitAndResetCommandList(dx12.graphicsCommandLists[dx12.currentFrame]);
			blasScratchBuffer.buffer->Release();

			model.meshes.push_back(std::move(modelMesh));
		}
		model.materials.reserve(gltfModel.materials.size());
		for (auto& gltfMaterial : gltfModel.materials) {
			ModelMaterial material;
			material.alphaCutoff = static_cast<float>(gltfMaterial.alphaCutoff);
			for (int i = 0; i < 4; i += 1) {
				material.baseColorFactor[i] = static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[i]);
			}
			for (int i = 0; i < 3; i += 1) {
				material.emissiveFactor[i] = static_cast<float>(gltfMaterial.emissiveFactor[i]);
			}
			if (gltfMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0) {
				assert(gltfMaterial.pbrMetallicRoughness.baseColorTexture.texCoord == 0);
				auto& baseColorTexture = gltfModel.textures[gltfMaterial.pbrMetallicRoughness.baseColorTexture.index];
				assert(baseColorTexture.source >= 0 && baseColorTexture.source < gltfModel.images.size());
				assert(baseColorTexture.sampler >= 0 && baseColorTexture.sampler < gltfModel.samplers.size());
				material.baseColorTextureIndex = baseColorTexture.source;
				material.baseColorTextureSamplerIndex = baseColorTexture.sampler;
			}
			if (gltfMaterial.normalTexture.index >= 0) {
				assert(gltfMaterial.normalTexture.texCoord == 0);
				assert(gltfMaterial.normalTexture.scale == 1.0);
				auto& normalTexture = gltfModel.textures[gltfMaterial.normalTexture.index];
				assert(normalTexture.source >= 0 && normalTexture.source < gltfModel.images.size());
				assert(normalTexture.sampler >= 0 && normalTexture.sampler < gltfModel.samplers.size());
				material.normalTextureIndex = normalTexture.source;
				material.normalTextureSamplerIndex = normalTexture.sampler;
			}
			if (gltfMaterial.emissiveTexture.index >= 0) {
				assert(gltfMaterial.emissiveTexture.texCoord == 0);
				auto& emissiveTexture = gltfModel.textures[gltfMaterial.emissiveTexture.index];
				assert(emissiveTexture.source >= 0 && emissiveTexture.source < gltfModel.images.size());
				assert(emissiveTexture.sampler >= 0 && emissiveTexture.sampler < gltfModel.samplers.size());
				material.emissiveTextureIndex = emissiveTexture.source;
				material.emissiveTextureSamplerIndex = emissiveTexture.sampler;
			}
			model.materials.push_back(material);
		}
		model.textures.reserve(gltfModel.images.size());
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
			DX12Texture texture = dx12.createTexture(gltfImage.width, gltfImage.height, 1, 1, format, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
			DX12TextureCopy textureCopy = {
				texture, gltfImage.image.data(), gltfImage.image.size(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			};
			dx12.copyTextures(&textureCopy, 1);
			std::wstring name(gltfImage.uri.begin(), gltfImage.uri.end());
			texture.texture->SetName(name.c_str());
			model.textures.push_back(texture);
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
		if (geometryInfosBuffer.buffer) {
			geometryInfosBuffer.buffer->Release();
		}
		if (triangleInfosBuffer.buffer) {
			triangleInfosBuffer.buffer->Release();
		}
		if (materialInfosBuffer.buffer) {
			materialInfosBuffer.buffer->Release();
		}

		std::vector<std::vector<std::vector<int>>> primitiveIndices;
		std::vector<GeometryInfo> geometryInfos;
		std::vector<TriangleInfo> triangleInfos;
		std::vector<MaterialInfo> materialInfos;
		int primitiveCount = 0;
		int textureCount = 0;
		for (auto& [modelName, model] : models) {
			std::vector<std::vector<int>> modelPrimitiveIndices;
			for (auto& mesh : model.meshes) {
				std::vector<int> meshPrimitiveIndices;
				for (auto& primitive : mesh.primitives) {
					meshPrimitiveIndices.push_back(primitiveCount);
					primitiveCount += 1;
					GeometryInfo geometryInfo;
					geometryInfo.triangleOffset = static_cast<int>(triangleInfos.size());
					geometryInfo.materialIndex = primitive.materialIndex >= 0 ? static_cast<int>(materialInfos.size()) + primitive.materialIndex : -1;
					geometryInfos.push_back(geometryInfo);
					for (uint64 indexIndex = 0; indexIndex < primitive.indices.size() / primitive.indexSize; indexIndex += 3) {
						TriangleInfo triangleInfo = {};
						for (uint64 triangleIndex = 0; triangleIndex < 3; triangleIndex += 1) {
							uint8* indexPtr = &primitive.indices[(indexIndex + triangleIndex) * primitive.indexSize];
							uint32 index = 0;
							if (primitive.indexSize == 2) {
								index = *reinterpret_cast<uint16*>(indexPtr);
							}
							else {
								index = *reinterpret_cast<uint32*>(indexPtr);
							}
							ModelVertex& vertex = primitive.vertices[index];
							arrayCopy(triangleInfo.normals[triangleIndex], vertex.normal);
							arrayCopy(triangleInfo.uvs[triangleIndex], vertex.uv);
							arrayCopy(triangleInfo.tangents[triangleIndex], vertex.tangent);
						}
						triangleInfos.push_back(triangleInfo);
					}
				}
				modelPrimitiveIndices.push_back(std::move(meshPrimitiveIndices));
			}
			primitiveIndices.push_back(std::move(modelPrimitiveIndices));
			for (auto& material : model.materials) {
				MaterialInfo materialInfo = { material };
				if (material.baseColorTextureIndex >= 0) {
					materialInfo.material.baseColorTextureIndex = textureCount + material.baseColorTextureIndex;
					materialInfo.material.baseColorTextureSamplerIndex = material.baseColorTextureSamplerIndex;
				}
				if (material.normalTextureIndex >= 0) {
					materialInfo.material.normalTextureIndex = textureCount + material.normalTextureIndex;
					materialInfo.material.normalTextureSamplerIndex = material.normalTextureSamplerIndex;
				}
				if (material.emissiveTextureIndex >= 0) {
					materialInfo.material.emissiveTextureIndex = textureCount + material.emissiveTextureIndex;
					materialInfo.material.emissiveTextureSamplerIndex = material.emissiveTextureSamplerIndex;
				}
				materialInfos.push_back(materialInfo);
			}
			textureCount += static_cast<int>(model.textures.size());
		}
		std::vector<InstanceInfo> instanceInfos;
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> tlasInstanceDescs;
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
					D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
					DirectX::XMStoreFloat3x4(reinterpret_cast<DirectX::XMFLOAT3X4*>(instanceDesc.Transform), node.second);
					instanceDesc.InstanceMask = 0xff;
					instanceDesc.AccelerationStructure = mesh.blasBuffer.buffer->GetGPUVirtualAddress();
					tlasInstanceDescs.push_back(instanceDesc);
					InstanceInfo instanceInfo = {};
					instanceInfo.transformMat = node.second;
					instanceInfo.geometryOffset = primitiveIndices[modelIndex][modelNode->meshIndex][0];
					instanceInfos.push_back(instanceInfo);
				}
				for (int childIndex : modelNode->children) {
					DirectX::XMMATRIX childTransform = XMMatrixMultiply(node.second, model.nodes[childIndex].transform);
					nodeStack.push(std::make_pair(childIndex, childTransform));
				}
			}
			modelIndex += 1;
		}

		instanceInfoCount = instanceInfos.size();
		instanceInfosBuffer = dx12.createBuffer(instanceInfos.size() * sizeof(instanceInfos[0]), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		instanceInfosBuffer.buffer->SetName(L"instanceInfosBuffer");
		void* instanceInfosBufferPtr = nullptr;
		instanceInfosBuffer.buffer->Map(0, nullptr, &instanceInfosBufferPtr);
		memcpy(instanceInfosBufferPtr, instanceInfos.data(), instanceInfos.size() * sizeof(instanceInfos[0]));
		instanceInfosBuffer.buffer->Unmap(0, nullptr);

		geometryInfoCount = geometryInfos.size();
		geometryInfosBuffer = dx12.createBuffer(geometryInfos.size() * sizeof(geometryInfos[0]), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		geometryInfosBuffer.buffer->SetName(L"geometryInfosBuffer");
		void* geometryInfosBufferPtr = nullptr;
		geometryInfosBuffer.buffer->Map(0, nullptr, &geometryInfosBufferPtr);
		memcpy(geometryInfosBufferPtr, geometryInfos.data(), geometryInfos.size() * sizeof(geometryInfos[0]));
		geometryInfosBuffer.buffer->Unmap(0, nullptr);

		triangleInfoCount = triangleInfos.size();
		triangleInfosBuffer = dx12.createBuffer(triangleInfos.size() * sizeof(triangleInfos[0]), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		triangleInfosBuffer.buffer->SetName(L"triangleInfosBuffer");
		void* triangleInfosBufferPtr = nullptr;
		triangleInfosBuffer.buffer->Map(0, nullptr, &triangleInfosBufferPtr);
		memcpy(triangleInfosBufferPtr, triangleInfos.data(), triangleInfos.size() * sizeof(triangleInfos[0]));
		triangleInfosBuffer.buffer->Unmap(0, nullptr);

		materialInfoCount = materialInfos.size();
		materialInfosBuffer = dx12.createBuffer(materialInfos.size() * sizeof(materialInfos[0]), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		materialInfosBuffer.buffer->SetName(L"materialInfosBuffer");
		void* materialInfosBufferPtr = nullptr;
		materialInfosBuffer.buffer->Map(0, nullptr, &materialInfosBufferPtr);
		memcpy(materialInfosBufferPtr, materialInfos.data(), materialInfos.size() * sizeof(materialInfos[0]));
		materialInfosBuffer.buffer->Unmap(0, nullptr);

		DX12Buffer tlasInstanceDescsBuffer = dx12.createBuffer(tlasInstanceDescs.size() * sizeof(tlasInstanceDescs[0]), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* instanceDescsBuffer = nullptr;
		tlasInstanceDescsBuffer.buffer->Map(0, nullptr, &instanceDescsBuffer);
		memcpy(instanceDescsBuffer, tlasInstanceDescs.data(), tlasInstanceDescs.size() * sizeof(tlasInstanceDescs[0]));
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

		DX12CommandList& cmdList = dx12.graphicsCommandLists[dx12.currentFrame];
		cmdList.list->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);
		dx12.closeAndExecuteCommandList(dx12.graphicsCommandLists[dx12.currentFrame]);
		dx12.waitAndResetCommandList(dx12.graphicsCommandLists[dx12.currentFrame]);

		tlasInstanceDescsBuffer.buffer->Release();
	}
};