#version 460 core

in VS_OUT {
    vec4 world_pos;
    vec4 color;
} fs_in;

out vec4 FragColor;

void main() {
    FragColor = fs_in.color;
}
