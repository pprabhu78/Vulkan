#version 420 core

out vec4 out_color;

in vec2 vp_out_texcoord;

uniform sampler2D tex;

void main(void)
{
    out_color =  texture(tex, vp_out_texcoord);
}