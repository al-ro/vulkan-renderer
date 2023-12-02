#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
};

void main() {
    gl_Position = proj * view * model * vec4(pos, 1.0);
    fragColor = col;
    fragTexCoord = inTexCoord;
}