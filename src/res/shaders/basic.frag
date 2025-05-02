#version 460 core

in VS_OUT {
    vec4 world_pos;
} fs_in;

out vec4 FragColor;

void main() {
    FragColor = vec4(1.0f);
}
