#version 450 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(push_constant) uniform pushConstants {
    mat4 modelViewProjection;
} u_pushConstants;

layout(location = 0) out vec3 out_normal;

void main() {
    mat4 modelViewProjection = u_pushConstants.modelViewProjection;
    gl_Position = modelViewProjection * vec4(in_position.x, in_position.y, in_position.z, 1.0);
    out_normal = in_normal;
}
