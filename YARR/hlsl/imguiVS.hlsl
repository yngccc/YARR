#define rootSig \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
"RootConstants(num32BitConstants = 2, b0, visibility = SHADER_VISIBILITY_VERTEX)," \
"DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP)"

cbuffer constants : register(b0) {
	int width, height;
};
Texture2D<float4> imguiTexture : register(t0);
sampler imguiTextureSampler : register(s0);

struct VSInput {
	float2 position : POSITION;
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

[RootSignature(rootSig)]
VSOutput vertexShader(VSInput vsInput) {
	VSOutput output;
	output.position.x = (vsInput.position.x / width) * 2 - 1;
	output.position.y = -((vsInput.position.y / height) * 2 - 1);
	output.position.zw = float2(0, 1);
	output.texCoord = vsInput.texCoord;
	output.color = vsInput.color;
	// output.color.rgb = pow(abs(output.color.rgb), float3(2.2f, 2.2f, 2.2f));
	return output;
}

struct PSOutput {
	float4 color : SV_Target;
};

[RootSignature(rootSig)]
PSOutput pixelShader(VSOutput vsOutput) {
	PSOutput output;
	output.color = vsOutput.color;
	output.color *= imguiTexture.Sample(imguiTextureSampler, vsOutput.texCoord);
	return output;
}
