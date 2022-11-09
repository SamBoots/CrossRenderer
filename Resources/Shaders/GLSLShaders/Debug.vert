#version 460

struct ModelInstance
{
    mat4 model;
    mat4 normalModel;
};

layout(set = 0, binding = 0) readonly buffer camBuffer
{
    mat4 view;
    mat4 proj;
} cam;

layout(set = 0, binding = 1) readonly buffer ModelBuffer
{
    ModelInstance instances;
} models;

layout( push_constant ) uniform constants
{
	uint model;
    uint paddingTo64Bytes[15];
} indices;


layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() 
{
    gl_Position = cam.proj * cam.view * models.instances.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}