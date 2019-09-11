#include <metal_stdlib>

using namespace metal;

typedef struct {
	packed_float2 position;
	packed_float2 texcoord;
} Vertex;

typedef struct {
	float4 position [[position]];
	float2 texcoord;
} VSOut;

vertex VSOut vs(
	device Vertex *verticies [[buffer(0)]],
	unsigned int vid [[vertex_id]]
) {
	device Vertex &v = verticies[vid];

	VSOut out;
	out.position = float4(float2(v.position), 0.0, 1.0);
	out.texcoord = v.texcoord;

	return out;
}

fragment float4 fs(
	VSOut in [[stage_in]],
	texture2d<float, access::sample> tex [[texture(0)]],
	sampler s [[sampler(0)]]
) {
	return tex.sample(s, in.texcoord);
}
