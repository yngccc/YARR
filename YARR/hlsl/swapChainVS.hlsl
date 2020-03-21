#include "miscs.hlsli"

#define rootSig \
"RootFlags(0)," \
"DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP)"

Texture2D<float4> colorTexture : register(t0);
sampler colorTextureSampler : register(s0);

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
	// output.color.rgb = acesFilmToneMap(output.color.rgb);
	output.color.rgb = linearToSRGB(output.color.rgb);
	return output;
}