struct VS_OUTPUT {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

VS_OUTPUT main(float2 position : POSITION, float2 texcoord : TEXCOORD)
{
	VS_OUTPUT output;

	output.position = float4(position.xy, 0, 1);
	output.texcoord = texcoord;

    return output;
}
