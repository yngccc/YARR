#include "utils.hlsli"

#define rootSig \
"RootFlags(0)," \
"DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP)"

Texture2D<float4> colorTexture : register(t0);
sampler colorTextureSampler : register(s0);

static const float3x3 acesInputMat = {
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};

static const float3x3 acesOutputMat = {
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};

float3 rrtOdtFit(float3 v) {
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 acesToneMap(float3 color) {
    color = mul(acesInputMat, color);
    color = rrtOdtFit(color);
    color = mul(acesOutputMat, color);
    color = saturate(color);
    return color;
}

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD;
};

[RootSignature(rootSig)] 
VSOutput vertexShader(uint vertexID : SV_VertexID) {
	VSOutput output;
	output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
	output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);
	return output;
}

struct PSOutput {
	float4 color : SV_Target;
};

[RootSignature(rootSig)]
PSOutput pixelShader(VSOutput vsOutput) {
	PSOutput output;
	output.color = colorTexture.Sample(colorTextureSampler, vsOutput.texCoord);
	//output.color.rgb = acesToneMap(output.color.rgb);
	output.color.rgb = linearToSRGB(output.color.rgb);
	return output;
}