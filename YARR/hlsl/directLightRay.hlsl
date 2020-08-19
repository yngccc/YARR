#include "utils.hlsli"
#include "sceneStructs.hlsli"

ConstantBuffer<SceneConstants> constants : register(b0);

Texture2D<float3> positionTexture : register(t0);
Texture2D<float3> normalTexture : register(t1);
Texture2D<float3> baseColorTexture : register(t2);
RaytracingAccelerationStructure sceneBVH : register(t3);
StructuredBuffer<SceneLight> lights : register(t4);

RWTexture2D<float3> outputTexture : register(u0);

struct RayPayload {
	bool hit;
};

[shader("raygeneration")]
void rayGen() {
	uint2 pixelIndex = DispatchRaysIndex().xy;

	float3 position = positionTexture[pixelIndex];
	float3 normal = normalTexture[pixelIndex];
	float3 baseColor = baseColorTexture[pixelIndex];

	float3 outputColor = float3(0, 0, 0);

	for (int i = 0; i < constants.lightCount; i += 1) {
		SceneLight light = lights[i];
		if (light.type == DIRECTIONAL_LIGHT) {
			RayDesc rayDesc;
			rayDesc.Origin = position;
			rayDesc.Direction = light.direction;
			rayDesc.TMin = 0.001;
			rayDesc.TMax = 500;
			RayPayload payload = { false };
			TraceRay(sceneBVH, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);
			if (!payload.hit) {
				outputColor += light.color * dot(normal, rayDesc.Direction);
			}
		}
		else if (light.type == POINT_LIGHT) {
			RayDesc rayDesc;
			rayDesc.Origin = position;
			rayDesc.Direction = normalize(light.position - position);
			rayDesc.TMin = 0.001;
			rayDesc.TMax = 500;
			RayPayload payload = { false };
			TraceRay(sceneBVH, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);
			if (!payload.hit) {
				outputColor += light.color * dot(normal, rayDesc.Direction);
			}
		}
	}
	outputTexture[pixelIndex] = outputColor * baseColor;
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
	payload.hit = true;
}

[shader("anyhit")]
void anyHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes triangleAttribs) {
}

[shader("miss")]
void miss(inout RayPayload payload) {
}