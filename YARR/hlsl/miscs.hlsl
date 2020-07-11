#ifndef MISCS_HLSL
#define MISCS_HLSL

static const float PI = 3.1415926535897932f;

float3 linearToSRGB(float3 rgb) {
	rgb = clamp(rgb, float3(0, 0, 0), float3(1, 1, 1));
	return max(1.055 * pow(rgb, 0.416666667) - 0.055, 0);
}

float3 acesFilmToneMap(float3 x) {
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float2 barycentricsInterpolate(float2 barycentrics, float2 vertexAttrib[3]) {
	return vertexAttrib[0] + barycentrics.x * (vertexAttrib[1] - vertexAttrib[0]) + barycentrics.y * (vertexAttrib[2] - vertexAttrib[0]);
}

float3 barycentricsInterpolate(float2 barycentrics, float3 vertexAttrib[3]) {
	return vertexAttrib[0] + barycentrics.x * (vertexAttrib[1] - vertexAttrib[0]) + barycentrics.y * (vertexAttrib[2] - vertexAttrib[0]);
}

float4 barycentricsInterpolate(float2 barycentrics, float4 vertexAttrib[3]) {
	return vertexAttrib[0] + barycentrics.x * (vertexAttrib[1] - vertexAttrib[0]) + barycentrics.y * (vertexAttrib[2] - vertexAttrib[0]);
}

uint wangHash(inout uint seed) {
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}

float randFloat01(inout uint state) {
	return float(wangHash(state)) * (1.0 / 4294967296.0);
}

float3 uniformSampleSphere(inout uint state) {
	float z = 1.0 - randFloat01(state) * 2.0;
	float phi = randFloat01(state) * 2.0 * PI;
	float r = sqrt(max(0.0, 1.0 - z * z));
	return float3(r * cos(phi), r * sin(phi), z);
}

float uniformSampleSpherePDF() {
	return 1.0 / (4.0 * PI);
}

float3 uniformSampleHemisphere(inout uint state) {
	float z = randFloat01(state);
	float phi = randFloat01(state) * 2.0 * PI;
	float r = sqrt(max(0.0, 1.0 - z * z));
	return float3(r * cos(phi), r * sin(phi), z);
}

float uniformSampleHemispherePDF() {
	return 1.0 / (2.0 * PI);
}

float3 cosineSampleHemisphere(inout uint state) {
	float x1 = randFloat01(state);
	float x2 = randFloat01(state);
	float r = sqrt(x1);
	float phi = x2 * 2.0 * PI;
	return float3(r * cos(phi), r * sin(phi), sqrt(1.0 - x1));
}

float cosineSampleHemispherePDF(float3 vec) {
	float cosAngle = dot(vec, float3(0, 0, 1));
	return cosAngle * (1.0 / PI);
}

float3 getPerpendicularVec(float3 vec) {
	float3 a = abs(vec);
	uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
	uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(vec, float3(xm, ym, zm));
}

float3 transformSampleVec(float3 sampleVec, float3 normal) {
	float3 bitangent = getPerpendicularVec(normal);
	float3 tangent = cross(bitangent, normal);
	return tangent * sampleVec.x + bitangent * sampleVec.y + normal * sampleVec.z;
}

// Ray Tracing Gem - Chapter 6
// A Fast and Robust Method for Avoiding Self Intersection
float3 offsetRayOrigin(const float3 p, const float3 n) {
	const float origin = 1.0 / 32.0;
	const float floatScale = 1.0 / 65536.0;
	const float intScale = 256.0;
	int3 of_i = n * intScale;
	float3 p_i = float3(
		(float)((int)p.x + ((p.x < 0) ? -of_i.x : of_i.x)),
		(float)((int)p.y + ((p.y < 0) ? -of_i.y : of_i.y)),
		(float)((int)p.z + ((p.z < 0) ? -of_i.z : of_i.z)));
	return float3(
		abs(p.x) < origin ? p.x + floatScale * n.x : p_i.x,
		abs(p.y) < origin ? p.y + floatScale * n.y : p_i.y,
		abs(p.z) < origin ? p.z + floatScale * n.z : p_i.z);
}

#endif