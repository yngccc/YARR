#include "utils.hlsli"
#include "sceneStructs.hlsli"

cbuffer constants : register(b0) {
	float4x4 screenToWorldMat;
	float4 eyePosition;
	int bounceCount;
	int sampleCount;
	int frameCount;
	int lightCount;
};

RWTexture2D<float3> positionTexture : register(u0);
RWTexture2D<float3> normalTexture : register(u1);
RWTexture2D<float3> baseColorTexture : register(u2);
RWTexture2D<float3> outputTexture : register(u3);

RaytracingAccelerationStructure scene : register(t0);
StructuredBuffer<SceneLight> lights : register(t1);

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

	for (int i = 0; i < lightCount; i += 1) {
		SceneLight light = lights[i];
		if (light.type == DIRECTIONAL_LIGHT) {
			RayDesc rayDesc;
			rayDesc.Origin = position;
			rayDesc.Direction = light.direction;
			rayDesc.TMin = 0.001;
			rayDesc.TMax = 500;
			RayPayload payload;
			payload.hit = false;
			TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);
			if (!payload.hit) {
				outputColor += light.color * dot(normal, rayDesc.Direction);
			}
		}
		else if (light.type == POINT_LIGHT) {
			RayDesc rayDesc;
			rayDesc.Origin = position;
			rayDesc.Direction = light.position - position;
			rayDesc.TMin = 0.001;
			rayDesc.TMax = 500;
			RayPayload payload;
			payload.hit = false;
			TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);
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