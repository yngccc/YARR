LocalRootSignature rootSig = {
	"DescriptorTable(UAV(u0), CBV(b0), SRV(t0, numDescriptors = 4), SRV(t4, numDescriptors = unbounded)),"
	"StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP)"
};

SubobjectToExportsAssociation rootSigRayGenAssociation = {
	"rootSig",  "rayGen"
};

SubobjectToExportsAssociation rootSigHitGroupAssociation = {
	"rootSig",  "hitGroup"
};

TriangleHitGroup hitGroup = {
	"", "closestHit"
};

RaytracingShaderConfig  shaderConfig = {
	16, // max payload size
	8   // max attribute size
};

RaytracingPipelineConfig pipelineConfig = {
	2 // max depth
};

struct InstanceInfo {
	float4x4 transformMat;
	int triangleOffset;
	int triangleCount;
	int materialIndex;
};

struct TriangleInfo {
	float3 normals[3];
	float2 texCoords[3];
};

struct MaterialInfo {
	float4 baseColorFactor;
	int baseColorTextureIndex;
};

RWTexture2D<float3> colorTexture : register(u0);
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
	float3 color;
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
	rayDesc.TMax = 1000;
	RayPayload rayPayload;
	TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, rayPayload);
	colorTexture[pixelIndex.xy] = rayPayload.color;
}

float2 barycentricsInterpolate(float2 barycentrics, float2 vertexAttrib[3]) {
	return vertexAttrib[0] + barycentrics.x * (vertexAttrib[1] - vertexAttrib[0]) + barycentrics.y * (vertexAttrib[2] - vertexAttrib[0]);
}

float3 barycentricsInterpolate(float2 barycentrics, float3 vertexAttrib[3]) {
	return vertexAttrib[0] + barycentrics.x * (vertexAttrib[1] - vertexAttrib[0]) + barycentrics.y * (vertexAttrib[2] - vertexAttrib[0]);
}

[shader("closesthit")]
void closestHit(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes attribs) {
	InstanceInfo instanceInfo = instanceInfos[InstanceIndex()];
	TriangleInfo triangleInfo = triangleInfos[instanceInfo.triangleOffset + PrimitiveIndex()];
	float2 texCoord = barycentricsInterpolate(attribs.barycentrics, triangleInfo.texCoords);
	Texture2D baseColorTexture = textures[materialInfos[instanceInfo.materialIndex].baseColorTextureIndex];
	float4 color = baseColorTexture.SampleLevel(sampler0, texCoord, 0);
	rayPayload.color = color.xyz;
}

[shader("miss")]
void miss(inout RayPayload rayPayload) {
	rayPayload.color = float3(0, 0, 0);
}

