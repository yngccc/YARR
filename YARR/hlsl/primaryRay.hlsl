#include "utils.hlsli"
#include "sceneStructs.hlsli"

ConstantBuffer<SceneConstants> constants : register(b0);

RWTexture2D<float3> positionTexture : register(u0);
RWTexture2D<float3> normalTexture : register(u1);
RWTexture2D<float3> baseColorTexture : register(u2);
RWTexture2D<float3> emissiveTexture : register(u3);

RaytracingAccelerationStructure sceneBVH : register(t0);
StructuredBuffer<InstanceInfo> instanceInfos : register(t1);
StructuredBuffer<GeometryInfo> geometryInfos : register(t2);
StructuredBuffer<TriangleInfo> triangleInfos : register(t3);
StructuredBuffer<MaterialInfo> materialInfos : register(t4);
Texture2D textures[] : register(t5);
sampler sampler0 : register(s0);

void getEyeRay(uint2 dimensions, uint2 pixelIndex, inout float3 origin, inout float3 direction) {
	float2 xy = pixelIndex + float2(0.5, 0.5);
	float2 screenPos = (xy / dimensions) * 2.0 - 1.0;
	screenPos.y = -screenPos.y;

	float4 world = mul(float4(screenPos, 0, 1), constants.screenToWorldMat);
	world.xyz /= world.w;

	origin = constants.eyePosition.xyz;
	direction = normalize(world.xyz - origin);
}

struct RayPayload {
	float3 position;
	float3 normal;
	float3 color;
	float3 emissive;
};

[shader("raygeneration")]
void rayGen() {
	uint2 dimensions = DispatchRaysDimensions().xy;
	uint2 pixelIndex = DispatchRaysIndex().xy;

	float3 origin;
	float3 direction;
	getEyeRay(dimensions, pixelIndex, origin, direction);

	RayDesc rayDesc;
	rayDesc.Origin = origin;
	rayDesc.Direction = direction;
	rayDesc.TMin = 0;
	rayDesc.TMax = 500;

	RayPayload payload = (RayPayload)0;
	TraceRay(sceneBVH, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);
	positionTexture[pixelIndex] = payload.position;
	normalTexture[pixelIndex] = payload.normal;
	baseColorTexture[pixelIndex] = payload.color;
	emissiveTexture[pixelIndex] = payload.emissive;
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
	InstanceInfo instanceInfo = instanceInfos[InstanceIndex()];
	GeometryInfo geometryInfo = geometryInfos[instanceInfo.geometryOffset + GeometryIndex()];
	MaterialInfo materialInfo = materialInfos[geometryInfo.materialIndex];
	TriangleInfo triangleInfo = triangleInfos[geometryInfo.triangleOffset + PrimitiveIndex()];
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
void anyHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
	InstanceInfo instanceInfo = instanceInfos[InstanceIndex()];
	GeometryInfo geometryInfo = geometryInfos[instanceInfo.geometryOffset + GeometryIndex()];
	MaterialInfo materialInfo = materialInfos[geometryInfo.materialIndex];
	if (materialInfo.baseColorTextureIndex > 0) {
		TriangleInfo triangleInfo = triangleInfos[geometryInfo.triangleOffset + PrimitiveIndex()];
		float2 texCoord = barycentricsInterpolate(triangleAttribs.barycentrics, triangleInfo.texCoords);
		Texture2D baseColorTexture = textures[materialInfo.baseColorTextureIndex];
		float4 color = baseColorTexture.SampleLevel(sampler0, texCoord, 0);
		if (color.w < materialInfo.alphaCutoff) {
			IgnoreHit();
		}
	}
}

[shader("miss")]
void miss(inout RayPayload payload) {
	payload = (RayPayload)0;
}

