#ifndef SKY_COMMON_HLSL
#define SKY_COMMON_HLSL

cbuffer CONSTANT_BUFFER : register(b0)
{
	float4x4 gViewProjMat;

	float4 gColor;

	float3 gSunIlluminance;
	int gScatteringMaxPathDepth;

	uint2 gResolution;
	float gFrameTimeSec;
	float gTimeSec;

	uint2 gMouseLastDownPos;
	uint gFrameId;
	uint gTerrainResolution;

	float2 RayMarchMinMaxSPP;
    uint2 gGameResolution;
	
    uint2 rayMarchingResolution;
    uint2 cameraVolumeResolution;
	
	float gScreenshotCaptureActive;
};

Texture2D<float4>  texture2d							: register(t0);
Texture2D<float4>  BlueNoise2dTexture					: register(t1);

RWTexture2D<float4> rwTexture2d							: register(u0);

SamplerState samplerLinearClamp : register(s0);
SamplerComparisonState  samplerShadow : register(s1);


////////////////////////////////////////////////////////////////////////////////////////////////////



struct VertexInput
{
	float4 position		: POSITION;
};

struct VertexOutput
{
	float4 position		: SV_POSITION;
	nointerpolation uint   sliceId		: SLICEINDEX;
};

struct CommonVertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

struct SlicedVertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
    nointerpolation uint sliceId : SLICEINDEX;
};

struct GeometryOutput
{
	float4 position		: SV_POSITION;
	nointerpolation uint   sliceId		: SV_RenderTargetArrayIndex;	//write to a specific slice, it can also be read in the pixel shader.
};



VertexOutput DefaultVertexShader(VertexInput input)
{
	VertexOutput output = (VertexOutput)0;

	// Calculate the position of the vertex against the world, view, and projection matrices.
	output.position = mul(input.position, gViewProjMat);

	output.sliceId = 0;

	return output;
}



VertexOutput ScreenTriangleVertexShader(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
	VertexOutput output = (VertexOutput)0;

	// For a range on screen in [-0.5,0.5]
	float2 uv = -1.0f;
	uv = vertexId == 1 ? float2(-1.0f, 3.0f) : uv;
	uv = vertexId == 2 ? float2( 3.0f,-1.0f) : uv;
	output.position = float4(uv, 0.0f, 1.0f);

	output.sliceId = instanceId;

	return output;
}



[maxvertexcount(3)]
void LutGS(triangle SlicedVertexOut input[3], inout TriangleStream<GeometryOutput> gsout)
{
	GeometryOutput output;
	for (uint i = 0; i < 3; i++)
	{
		output.position = input[i].PosH;
		output.sliceId = input[0].sliceId;
		gsout.Append(output);
	}
	gsout.RestartStrip();
}

#endif // SKY_COMMON_HLSL
