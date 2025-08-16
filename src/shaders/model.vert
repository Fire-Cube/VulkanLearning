#version 450 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;

layout(set = 0, binding = 0) uniform transforms {
    mat4 modelViewProjection;
} u_transforms;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec2 out_texcoord;
void main() {
    mat4 modelViewProjection = u_transforms.modelViewProjection;
    gl_Position = modelViewProjection * vec4(in_position.x, in_position.y, in_position.z, 1.0);
    out_normal = in_normal;
    out_texcoord = in_texcoord;
}
