#version 410 core

layout (location = 0) in vec4 position;

out vec2 vp_out_texcoord;

uniform vec4 window_params;

uniform float viewport_width;
uniform float viewport_height;

void main(void)
{
    float window_xpos = window_params.x;
    float window_ypos = window_params.y;

    float window_width  = window_params.z;
    float window_height = window_params.w;

    float aspect = viewport_width/viewport_height;

    vec2 coords = vec2(position.x,position.y)*0.5 + 0.5;
    coords = coords*vec2((window_width/viewport_width), window_height/viewport_height);
    coords = coords*2 - 1;
    coords += vec2((window_xpos/viewport_width),window_ypos/viewport_height);

    gl_Position = vec4(coords.x, -coords.y, 0, 1);

    vp_out_texcoord = vec2(position.z, position.w);
}
