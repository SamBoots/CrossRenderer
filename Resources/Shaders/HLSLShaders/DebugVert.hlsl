struct VertexInput
{
    float3 inPosition : POSITION;
    float3 inColor : COLOR;
};

struct VertexOutput
{
    float3 color : COLOR;
};

VertexOutput main(VertexInput vertexInput)
{
    float4 position = float4(inPosition, 0.0, 1.0);

    VertexOutput output;
    output.position = position;
    output.color = VertexInput.inColor;
    return output;
}