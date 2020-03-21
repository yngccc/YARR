#define rootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
"DescriptorTable(CBV(b0), SRV(t0, numDescriptors = 4), visibility = SHADER_VISIBILITY_ALL)," \
"StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP)"

cbuffer constants : register(b0) {
	float4x4 viewProjMat;
	float4x4 modelMat;
	float4x4 meshMat;

	float4 diffuseFactor;
	float4 emissiveFactor;
	float2 roughnessMetallicFactor;
};

Texture2D<float3> diffuseTexture : register(t0);
Texture2D<float2> normalTexture : register(t1);
Texture2D<float2> roughnessMetallicTexture : register(t2);
Texture2D<float3> emissiveTexture : register(t3);

sampler textureSampler : register(s0);

struct VSInput {
	float3 position : POSITION;
	float3 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
};

struct VSOutput {
	float4 svPosition : SV_POSITION;
	float3 position : POSITIONT;
	float3 color : COLOR;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3x3 tbnMat : OUTPUT0;
};

[RootSignature(rootSig)]
VSOutput vertexShader(VSInput vsInput) {
	float4x4 transformMat = mul(modelMat, meshMat);
	float3x3 normalMat = float3x3(transformMat[0].xyz, transformMat[1].xyz, transformMat[2].xyz);

	VSOutput vsOutput;
	vsOutput.position = mul(transformMat, float4(vsInput.position, 1)).xyz;
	vsOutput.svPosition = mul(viewProjMat, float4(vsOutput.position, 1));
	vsOutput.color = vsInput.color;
	vsOutput.texCoord = vsInput.texCoord;
	vsOutput.normal = normalize(mul(normalMat, vsInput.normal));
	float3 tangent = normalize(mul(normalMat, vsInput.tangent));
	float3 bitangent = normalize(cross(vsOutput.normal, tangent));
	vsOutput.tbnMat = float3x3(tangent, bitangent, vsOutput.normal);

	return vsOutput;
}

struct PSOutput {
	float3 diffuse : SV_TARGET0;
	float3 position : SV_TARGET1;
	float3 normal : SV_TARGET2;
	float2 roughnessMetallic : SV_TARGET3;
	float3 emissive : SV_TARGET4;
};

[RootSignature(rootSig)]
PSOutput pixelShader(VSOutput vsOutput) {
	float3 diffuse = diffuseTexture.Sample(textureSampler, vsOutput.texCoord) * diffuseFactor.rgb;
	float2 tnormal = normalTexture.Sample(textureSampler, vsOutput.texCoord) * 2 - 1;
	float2 roughnessMetallic = roughnessMetallicTexture.Sample(textureSampler, vsOutput.texCoord) * roughnessMetallicFactor;
	float3 emissive = emissiveTexture.Sample(textureSampler, vsOutput.texCoord) * emissiveFactor.rgb;

	// float3 normal = mul(vsOutput.tbnMat, float3(tnormal, sqrt(1 - tnormal.x * tnormal.x - tnormal.y * tnormal.y)));
	float3 normal = normalize(vsOutput.normal);

	PSOutput psOutput;
	psOutput.diffuse = diffuse;
	psOutput.position = vsOutput.position;
	psOutput.normal = normal;
	psOutput.roughnessMetallic = roughnessMetallic;
	psOutput.emissive = emissive;
	return psOutput;
}