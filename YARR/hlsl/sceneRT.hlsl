#include "miscs.hlsl"
#include "../thirdparty/hlsl/ddgi/probeCommon.hlsl"

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
};

RWTexture2D<float4> colorTexture : register(u0);
cbuffer constants : register(b0) {
	float4x4 screenToWorldMat;
	float3 eyePosition;
};
RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<InstanceInfo> instanceInfos : register(t1);
StructuredBuffer<TriangleInfo> triangleInfos : register(t2);
StructuredBuffer<MaterialInfo> materialInfos : register(t3);
Texture2D textures[] : register(t4);
sampler sampler0 : register(s0);

struct RayPayload {
	float4 color;
};

void getEyeRay(uint2 dimensions, uint2 pixelIndex, out float3 origin, out float3 direction) {
	float2 xy = pixelIndex + float2(0.5, 0.5);
	float2 screenPos = (xy / dimensions) * 2.0 - 1.0;
	screenPos.y = -screenPos.y;

	float4 world = mul(float4(screenPos, 0, 1), screenToWorldMat);
	world.xyz /= world.w;

	origin = eyePosition;
	direction = normalize(world.xyz - origin);
}

[shader("raygeneration")]
void rayGen() {
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
	RayPayload rayPayload = { float4(0, 0, 0, 0) };
	TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, rayPayload);
	colorTexture[pixelIndex.xy] = rayPayload.color;

//	// RTXGI
//	float3 irradiance = 0.f;
//#if RTXGI_DDGI_COMPUTE_IRRADIANCE
//	float3 cameraDirection = normalize(worldPosHitT.xyz - cameraOrigin);
//	float3 surfaceBias = DDGIGetSurfaceBias(normal, cameraDirection, DDGIVolume);
//
//	DDGIVolumeResources resources;
//	resources.probeIrradianceSRV = DDGIProbeIrradianceSRV;
//	resources.probeDistanceSRV = DDGIProbeDistanceSRV;
//	resources.trilinearSampler = TrilinearSampler;
//#if RTXGI_DDGI_PROBE_RELOCATION
//	resources.probeOffsets = DDGIProbeOffsets;
//#endif
//#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
//	resources.probeStates = DDGIProbeStates;
//#endif
//	irradiance = DDGIGetVolumeIrradiance(worldPosHitT.xyz, surfaceBias, normal, DDGIVolume, resources);
//#endif
}

float2 barycentricsInterpolate(float2 barycentrics, float2 vertexAttrib[3]) {
	return vertexAttrib[0] + barycentrics.x * (vertexAttrib[1] - vertexAttrib[0]) + barycentrics.y * (vertexAttrib[2] - vertexAttrib[0]);
}

float3 barycentricsInterpolate(float2 barycentrics, float3 vertexAttrib[3]) {
	return vertexAttrib[0] + barycentrics.x * (vertexAttrib[1] - vertexAttrib[0]) + barycentrics.y * (vertexAttrib[2] - vertexAttrib[0]);
}

[shader("closesthit")]
void closestHit(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
	InstanceInfo instanceInfo = instanceInfos[InstanceIndex()];
	TriangleInfo triangleInfo = triangleInfos[instanceInfo.triangleOffset + PrimitiveIndex()];
	MaterialInfo materialInfo = materialInfos[instanceInfo.materialIndex];
	float2 texCoord = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.texCoords);
	Texture2D baseColorTexture = textures[materialInfo.baseColorTextureIndex];
	float4 color = baseColorTexture.SampleLevel(sampler0, texCoord, 0);
	color *= materialInfo.baseColorFactor;
	float3 normal = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.normals);
	if (materialInfo.normalTextureIndex >= 0) {
		float3 tangent = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.tangents);
		float3 bitangent = cross(normal, tangent);
		float3x3 tbn = transpose(float3x3(tangent, bitangent, normal));
		Texture2D normalTexture = textures[materialInfo.normalTextureIndex];
		float3 n = normalTexture.SampleLevel(sampler0, texCoord, 0).xyz;
		normal = mul(tbn, n);
	}
	float n = max(0, dot(normal, normalize(float3(1, 1, 1))));
	rayPayload.color = color * (0.0 + n);
};

[shader("anyhit")]
void anyHit(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
	InstanceInfo instanceInfo = instanceInfos[InstanceIndex()];
	TriangleInfo triangleInfo = triangleInfos[instanceInfo.triangleOffset + PrimitiveIndex()];
	MaterialInfo materialInfo = materialInfos[instanceInfo.materialIndex];
	float2 texCoord = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.texCoords);
	Texture2D baseColorTexture = textures[materialInfo.baseColorTextureIndex];
	float4 color = baseColorTexture.SampleLevel(sampler0, texCoord, 0);
	if (color.w < 1) {
		IgnoreHit();
	}
}

[shader("miss")]
void miss(inout RayPayload rayPayload) {
	rayPayload.color = float4(0, 0, 0, 0);
}

