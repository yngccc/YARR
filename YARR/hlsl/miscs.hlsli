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

float rand_xorshift(inout uint rng_state) {
	rng_state ^= (rng_state << 13);
	rng_state ^= (rng_state >> 17);
	rng_state ^= (rng_state << 5);
	return float(rng_state) * (1.0 / 4294967296.0);
}

uint wang_hash(uint seed) {
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}