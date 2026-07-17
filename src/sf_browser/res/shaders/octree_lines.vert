#version 460 core

layout (location = 0) in vec3 pos;
layout (location = 1) in float instance_active;     // 0: false, 1: true
layout (location = 2) in mat4 model;                //loc 2: column 0
                                                    //loc 3: column 1
                                                    //loc 4: column 2
                                                    //loc 5: column 3

uniform mat4 view;
uniform mat4 projection;

out VS_OUT {
    vec4 world_pos;
    vec4 color;
} vs_out;

void main()
{
    vs_out.world_pos = model * vec4(pos, 1.0);
    vs_out.color = mix(vec4(0.1f, 0.1f, 0.1f, 0.5f), vec4(1.0f, 1.0f, 0.0f, 1.0f), instance_active);

    vec4 view_space = view * vs_out.world_pos;
    view_space.z += 0.001f * instance_active;

    gl_Position = projection * view_space;
}
