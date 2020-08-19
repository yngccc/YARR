struct SceneConstants {
#ifdef __cplusplus
#else
	float4x4 screenToWorldMat;
	float4 eyePosition;
	int bounceCount;
	int sampleCount;
	int frameCount;
	int lightCount;
#endif
};

struct InstanceInfo {
#ifdef __cplusplus
	DirectX::XMMATRIX transformMat;
	int geometryOffset;
#else
	float4x4 transformMat;
	int geometryOffset;
#endif
};

struct GeometryInfo {
#ifdef __cplusplus
	int triangleOffset;
	int materialIndex;
#else
	int triangleOffset;
	int materialIndex;
#endif
};

struct TriangleInfo {
#ifdef __cplusplus
	float normals[3][3];
	float uvs[3][2];
	float tangents[3][3];
#else
	float3 normals[3];
	float2 texCoords[3];
	float3 tangents[3];
#endif
};

struct MaterialInfo {
#ifdef __cplusplus
	ModelMaterial material;
#else
	float4 baseColorFactor;
	int baseColorTextureIndex;
	int baseColorTectureSamplerIndex;
	int normalTextureIndex;
	int normalTectureSamplerIndex;
	float3 emissiveFactor;
	int emissiveTextureIndex;
	int emissiveTectureSamplerIndex;
	float alphaCutoff;
#endif
};

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1

struct SceneLight {
#ifdef __cplusplus
	int type;
	float position[3] = { 0, 0, 0 };
	float direction[3] = { 0, 1, 0 };
	float color[3] = { 1, 1, 1 };
#else
	int type;
	float3 position;
	float3 direction;
	float3 color;
#endif
};