#include "miscs.hlsl"

struct InstanceInfo {
	float4x4 transformMat;
	int triangleOffset;
	int triangleCount;
	int materialIndex;
	int padding;
};

struct TriangleInfo {
	float3 normals[3];
	float2 texCoords[3];
	float3 tangents[3];
};

struct MaterialInfo {
	float4 baseColorFactor;
	int baseColorTextureIndex;
	int baseColorTectureSamplerIndex;
	int normalTextureIndex;
	int normalTectureSamplerIndex;
	float3 emissiveFactor;
	int emissiveTextureIndex;
	int emissiveTectureSamplerIndex;
	float alphaCutoff;
};

RWTexture2D<float3> positionTexture : register(u0);
RWTexture2D<float3> normalTexture : register(u1);
RWTexture2D<float3> colorTexture : register(u2);
RWTexture2D<float3> emissiveTexture : register(u3);
cbuffer constants : register(b0) {
	float4x4 screenToWorldMat;
	float4 eyePosition;
	int bounceCount;
	int sampleCount;
	int frameCount;
};
RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<InstanceInfo> instanceInfos : register(t1);
StructuredBuffer<TriangleInfo> triangleInfos : register(t2);
StructuredBuffer<MaterialInfo> materialInfos : register(t3);
Texture2D textures[] : register(t4);
sampler sampler0 : register(s0);

void getEyeRay(uint2 dimensions, uint2 pixelIndex, inout float3 origin, inout float3 direction) {
	float2 xy = pixelIndex + float2(0.5, 0.5);
	float2 screenPos = (xy / dimensions) * 2.0 - 1.0;
	screenPos.y = -screenPos.y;

	float4 world = mul(float4(screenPos, 0, 1), screenToWorldMat);
	world.xyz /= world.w;

	origin = eyePosition.xyz;
	direction = normalize(world.xyz - origin);
}

struct PrimaryRayPayload {
	float3 position;
	float3 normal;
	float3 color;
	float3 emissive;
};

[shader("raygeneration")]
void primaryRayGen() {
	uint3 dimensions = DispatchRaysDimensions();
	uint3 pixelIndex = DispatchRaysIndex();

	float3 origin;
	float3 direction;
	getEyeRay(dimensions.xy, pixelIndex.xy, origin, direction);

	RayDesc rayDesc;
	rayDesc.Origin = origin;
	rayDesc.Direction = direction;
	rayDesc.TMin = 0;
	rayDesc.TMax = 5000;

	PrimaryRayPayload payload;
	payload.position = float3(0, 0, 0);
	payload.normal = float3(0, 0, 0);
	payload.color = float3(0, 0, 0);
	payload.emissive = float3(0, 0, 0);
	TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);
	positionTexture[pixelIndex.xy] = payload.position;
	normalTexture[pixelIndex.xy] = payload.normal;
	colorTexture[pixelIndex.xy] = payload.color;
	emissiveTexture[pixelIndex.xy] = payload.emissive;
}

[shader("closesthit")]
void primaryRayClosestHit(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
	InstanceInfo instanceInfo = instanceInfos[InstanceIndex()];
	MaterialInfo materialInfo = materialInfos[instanceInfo.materialIndex];
	TriangleInfo triangleInfo = triangleInfos[instanceInfo.triangleOffset + PrimitiveIndex()];
	float3 position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	float3 normal = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.normals);
	float3 color = materialInfo.baseColorFactor.rgb;
	float2 texCoord = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.texCoords);
	if (materialInfo.baseColorTextureIndex >= 0) {
		Texture2D baseColorTexture = textures[materialInfo.baseColorTextureIndex];
		color *= baseColorTexture.SampleLevel(sampler0, texCoord, 0).rgb;
	}
	if (materialInfo.normalTextureIndex >= 0) {
		float3 tangent = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.tangents);
		float3 bitangent = cross(normal, tangent);
		float3x3 tbn = transpose(float3x3(tangent, bitangent, normal));
		Texture2D normalTexture = textures[materialInfo.normalTextureIndex];
		float3 n = normalTexture.SampleLevel(sampler0, texCoord, 0).xyz;
		normal = mul(tbn, n);
	}
	normal = normalize(mul((float3x3)instanceInfo.transformMat, normal));
	payload.position = position;
	payload.normal = normal;
	payload.color = color;
	payload.emissive = materialInfo.emissiveFactor;
}

[shader("anyhit")]
void primaryRayAnyHit(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
	InstanceInfo instanceInfo = instanceInfos[InstanceIndex()];
	MaterialInfo materialInfo = materialInfos[instanceInfo.materialIndex];
	if (materialInfo.baseColorTextureIndex > 0) {
		TriangleInfo triangleInfo = triangleInfos[instanceInfo.triangleOffset + PrimitiveIndex()];
		float2 texCoord = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.texCoords);
		Texture2D baseColorTexture = textures[materialInfo.baseColorTextureIndex];
		float4 color = baseColorTexture.SampleLevel(sampler0, texCoord, 0);
		if (color.w < materialInfo.alphaCutoff) {
			IgnoreHit();
		}
	}
}

[shader("miss")]
void primaryRayMiss(inout PrimaryRayPayload payload) {
	payload.position = float3(0, 0, 0);
	payload.normal = float3(0, 0, 0);
	payload.color = float3(0, 0, 0);
	payload.emissive = float3(0, 0, 0);
}

//float3 L = float3(0, 0, 0);
//for (int i = 0; i < sampleCount; i += 1) {
//	float3 vec = cosineSampleHemisphere(rayPayload.rngState);
//	float3 dir = transformSampleVec(vec, normal);
//
//	RayDesc rayDesc;
//	rayDesc.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
//	//rayDesc.Origin = offsetRayOrigin(rayDesc.Origin, normal);
//	rayDesc.Direction = dir;
//	rayDesc.TMin = 0.0001;
//	rayDesc.TMax = 5000;
//	TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, rayPayload);
//	L += rayPayload.color;
//	rayPayload.bounce = bounce;
//}
//L /= sampleCount;
