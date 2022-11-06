#version 460

struct ModelInstance
{
    mat4 matrix;
    mat4 normalMatrix;
};

layout(std140,set = 0, binding = 1) readonly buffer PerFrame
{
    mat4 view;
    mat4 proj;
    ModelInstance instances;
} perFrame;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() 
{
    gl_Position = perFrame.proj * perFrame.view * perFrame.instances.matrix * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}