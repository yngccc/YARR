#include "miscs.hlsl"
#include "../thirdparty/hlsl/ddgi/probeCommon.hlsl"
#include "../thirdparty/hlsl/ddgi/Irradiance.hlsl"

ConstantBuffer<DDGIVolumeDescGPU>        DDGIVolume                  : register(b1, space1);

RWTexture2D<float4>                      DDGIProbeRTRadiance         : register(u0, space1);
//RWTexture2D<float4>                    DDGIProbeIrradianceUAV      : register(u1, space1);    // not used by app (SDK only)
//RWTexture2D<float4>                    DDGIProbeDistanceUAV        : register(u2, space1);    // not used by app (SDK only)
RWTexture2D<float4>                      DDGIProbeOffsets            : register(u3, space1);
RWTexture2D<uint>                        DDGIProbeStates             : register(u4, space1);
// -----------------------------------

Texture2D<float4>                        DDGIProbeIrradianceSRV      : register(t0);
Texture2D<float4>                        DDGIProbeDistanceSRV        : register(t1);
RaytracingAccelerationStructure          SceneBVH : register(t2);
//ByteAddressBuffer                      Indices                     : register(t3);        // not used, part of local root signature
//ByteAddressBuffer                      Vertices                    : register(t4);        // not used, part of local root signature
Texture2D<float4>                        BlueNoiseRGB                : register(t5);

SamplerState                            TrilinearSampler             : register(s0);
SamplerState                            PointSampler                 : register(s1);

cbuffer NoiseRootConstants : register(b4) {
	uint  ResolutionX;
	uint  FrameNumber;
	float Exposure;
	uint  UseRTAO;
	uint  ViewAO;
	float AORadius;
	float AOPower;
	float AOBias;
};

cbuffer VisTLASUpdateRootConstants : register (b5) {
	uint2 BLASGPUAddress;    // 64-bit gpu address
	float VizProbeRadius;
	float VisTLASPad;
};

cbuffer RTRootConstants : register(b6) {
	float NormalBias;
	float ViewBias;
	uint  NumBounces;
	float RTPad;
};

struct PayloadData {
	float3    baseColor;
	float3    worldPosition;
	float3    normal;
	float     hitT;
	uint      hitKind;
	uint      instanceIndex;
};

//struct InstanceInfo {
//	float4x4 transformMat;
//	int triangleOffset;
//	int triangleCount;
//	int materialIndex;
//	int padding;
//};
//
//struct TriangleInfo {
//	float3 normals[3];
//	float2 texCoords[3];
//	float3 tangents[3];
//};
//
//struct MaterialInfo {
//	float4 baseColorFactor;
//	int baseColorTextureIndex;
//	int baseColorTectureSamplerIndex;
//	int normalTextureIndex;
//	int normalTectureSamplerIndex;
//};
//
//RWTexture2D<float4> colorTexture : register(u0);
//cbuffer constants : register(b0) {
//	float4x4 screenToWorldMat;
//	float3 eyePosition;
//};
//RaytracingAccelerationStructure scene : register(t0);
//StructuredBuffer<InstanceInfo> instanceInfos : register(t1);
//StructuredBuffer<TriangleInfo> triangleInfos : register(t2);
//StructuredBuffer<MaterialInfo> materialInfos : register(t3);
//Texture2D textures[] : register(t4);
//sampler sampler0 : register(s0);

//struct RayPayload {
//	float4 color;
//};
//
//void getEyeRay(uint2 dimensions, uint2 pixelIndex, out float3 origin, out float3 direction) {
//	float2 xy = pixelIndex + float2(0.5, 0.5);
//	float2 screenPos = (xy / dimensions) * 2.0 - 1.0;
//	screenPos.y = -screenPos.y;
//
//	float4 world = mul(float4(screenPos, 0, 1), screenToWorldMat);
//	world.xyz /= world.w;
//
//	origin = eyePosition;
//	direction = normalize(world.xyz - origin);
//}

[shader("raygeneration")]
void rayGen() {
	float4 result = 0.f;

	uint2 DispatchIndex = DispatchRaysIndex().xy;
	int rayIndex = DispatchIndex.x;                    // index of ray within a probe
	int probeIndex = DispatchIndex.y;                  // index of current probe

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
	int2 texelPosition = DDGIGetProbeTexelPosition(probeIndex, DDGIVolume.probeGridCounts);
	int  probeState = DDGIProbeStates[texelPosition];
	if (probeState == PROBE_STATE_INACTIVE)
	{
		return;  // if the probe is inactive, do not shoot rays
	}
#endif

#if RTXGI_DDGI_PROBE_RELOCATION
	float3 probeWorldPosition = DDGIGetProbeWorldPositionWithOffset(probeIndex, DDGIVolume.origin, DDGIVolume.probeGridCounts, DDGIVolume.probeGridSpacing, DDGIProbeOffsets);
#else
	float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeIndex, DDGIVolume.origin, DDGIVolume.probeGridCounts, DDGIVolume.probeGridSpacing);
#endif

	float3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform);

	// Setup the probe ray
	RayDesc ray;
	ray.Origin = probeWorldPosition;
	ray.Direction = probeRayDirection;
	ray.TMin = 0.f;
	ray.TMax = 1e27f;

	// Probe Ray Trace
	PayloadData payload = (PayloadData)0;
	TraceRay(
		SceneBVH,
		RAY_FLAG_NONE,
		0xFF,
		0,
		1,
		0,
		ray,
		payload);

	result = float4(payload.baseColor, payload.hitT);

	// Ray miss. Set hit distance to a large value and exit early.
	if (payload.hitT < 0.f)
	{
		result.w = 1e27f;
		DDGIProbeRTRadiance[DispatchIndex.xy] = result;
		return;
	}

	// Hit a surface backface. Set the radiance to black and exit early.
	if (payload.hitKind == HIT_KIND_TRIANGLE_BACK_FACE)
	{
		// Shorten the hit distance on a backface hit by 20%
		// Make distance negative to encode backface for the probe position preprocess.
		DDGIProbeRTRadiance[DispatchIndex.xy] = float4(0.f, 0.f, 0.f, -payload.hitT * 0.2f);
		return;
	}

	// Direct Lighting and Shadowing
	//float3 diffuse = DirectDiffuseLighting(
	//	payload.baseColor,
	//	payload.worldPosition,
	//	payload.normal,
	//	NormalBias,
	//	ViewBias,
	//	SceneBVH);
	float3 diffuse = { 0, 0, 0 };

	// Indirect Lighting (recursive)
	float3 irradiance = 0.f;
#if RTXGI_DDGI_COMPUTE_IRRADIANCE_RECURSIVE
	float3 surfaceBias = DDGIGetSurfaceBias(payload.normal, ray.Direction, DDGIVolume);

	DDGIVolumeResources resources;
	resources.probeIrradianceSRV = DDGIProbeIrradianceSRV;
	resources.probeDistanceSRV = DDGIProbeDistanceSRV;
	resources.trilinearSampler = TrilinearSampler;
#if RTXGI_DDGI_PROBE_RELOCATION
	resources.probeOffsets = DDGIProbeOffsets;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
	resources.probeStates = DDGIProbeStates;
#endif

	// Compute volume blending weight
	float volumeBlendWeight = DDGIGetVolumeBlendWeight(payload.worldPosition, DDGIVolume);

	// Avoid evaluating irradiance when the surface is outside the volume
	if (volumeBlendWeight > 0)
	{
		// Get irradiance from the DDGIVolume
		irradiance = DDGIGetVolumeIrradiance(
			payload.worldPosition,
			surfaceBias,
			payload.normal,
			DDGIVolume,
			resources);

		// Attenuate irradiance by the blend weight
		irradiance *= volumeBlendWeight;
	}
#endif

	// Compute final color
	result = float4(diffuse + ((payload.baseColor / PI) * irradiance), payload.hitT);

	DDGIProbeRTRadiance[DispatchIndex.xy] = result;
}

//float2 barycentricsInterpolate(float2 barycentrics, float2 vertexAttrib[3]) {
//	return vertexAttrib[0] + barycentrics.x * (vertexAttrib[1] - vertexAttrib[0]) + barycentrics.y * (vertexAttrib[2] - vertexAttrib[0]);
//}
//
//float3 barycentricsInterpolate(float2 barycentrics, float3 vertexAttrib[3]) {
//	return vertexAttrib[0] + barycentrics.x * (vertexAttrib[1] - vertexAttrib[0]) + barycentrics.y * (vertexAttrib[2] - vertexAttrib[0]);
//}

//[shader("closesthit")]
//void closestHit(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
//	InstanceInfo instanceInfo = instanceInfos[InstanceIndex()];
//	TriangleInfo triangleInfo = triangleInfos[instanceInfo.triangleOffset + PrimitiveIndex()];
//	MaterialInfo materialInfo = materialInfos[instanceInfo.materialIndex];
//	float2 texCoord = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.texCoords);
//	Texture2D baseColorTexture = textures[materialInfo.baseColorTextureIndex];
//	float4 color = baseColorTexture.SampleLevel(sampler0, texCoord, 0);
//	color *= materialInfo.baseColorFactor;
//	float3 normal = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.normals);
//	if (materialInfo.normalTextureIndex >= 0) {
//		float3 tangent = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.tangents);
//		float3 bitangent = cross(normal, tangent);
//		float3x3 tbn = transpose(float3x3(tangent, bitangent, normal));
//		Texture2D normalTexture = textures[materialInfo.normalTextureIndex];
//		float3 n = normalTexture.SampleLevel(sampler0, texCoord, 0).xyz;
//		normal = mul(tbn, n);
//	}
//	float n = max(0, dot(normal, normalize(float3(1, 1, 1))));
//	rayPayload.color = color * (0.0 + n);
//};
//
//[shader("anyhit")]
//void anyHit(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
//	InstanceInfo instanceInfo = instanceInfos[InstanceIndex()];
//	TriangleInfo triangleInfo = triangleInfos[instanceInfo.triangleOffset + PrimitiveIndex()];
//	MaterialInfo materialInfo = materialInfos[instanceInfo.materialIndex];
//	float2 texCoord = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.texCoords);
//	Texture2D baseColorTexture = textures[materialInfo.baseColorTextureIndex];
//	float4 color = baseColorTexture.SampleLevel(sampler0, texCoord, 0);
//	if (color.w < 1) {
//		IgnoreHit();
//	}
//}
//
//[shader("miss")]
//void miss(inout RayPayload rayPayload) {
//	rayPayload.color = float4(0, 0, 0, 0);
//}

