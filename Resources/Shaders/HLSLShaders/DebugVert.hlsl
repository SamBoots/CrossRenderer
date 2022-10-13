struct VertexInput
{
    float2 inPosition : POSITION;
    float3 inColor : COLOR;
};

struct VertexOutput
{
    float3 color : COLOR;
    float4 position : SV_Position;
};

VertexOutput main(VertexInput vertexInput)
{
    float4 position = float4(vertexInput.inPosition, 0.0, 1.0);

    VertexOutput output;
    output.position = position;
    output.color = vertexInput.inColor;
    return output;
}