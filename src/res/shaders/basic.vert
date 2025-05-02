#version 460 core

layout (location = 0) in vec3 pos;

uniform mat4 view;
uniform mat4 projection;

out VS_OUT {
    vec4 world_pos;
} vs_out;

void main()
{
    vs_out.world_pos = projection * view * vec4(pos, 1.0);

    gl_Position = vs_out.world_pos;
}
