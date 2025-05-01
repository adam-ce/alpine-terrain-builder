#version 460 core

in VS_OUT {
    vec4 world_pos;
} fs_in;

out vec4 frag_color;

void main() {
    frag_color = vec4(1.0f);
}
