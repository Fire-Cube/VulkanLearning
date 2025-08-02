#version 450 core

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D in_sampled_texture;

void main() {
    vec4 textureSample = texture(in_sampled_texture, in_uv);
    // out_color = vec4(in_color, 1.0);
    out_color = textureSample;
}
