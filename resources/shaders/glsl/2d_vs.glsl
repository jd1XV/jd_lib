#version 420 core

layout (location = 0) in vec3 vert_xyz;
layout (location = 1) in vec3 vert_uvw;
layout (location = 2) in vec4 vert_col;
layout (location = 3) in vec4 vert_rect;
layout (location = 4) in float rounding;
layout (location = 5) in float softness;
layout (location = 6) in float thickness;

out vec3 fs_xyz;
out vec3 fs_uvw;
out vec4 fs_col;
out vec4 fs_rect;
out float fs_rounding;
out float fs_softness;
out float fs_thickness;

uniform vec2 screen_conv;

void main(){
    gl_Position.x = (vert_xyz.x * screen_conv.x) - 1;
    gl_Position.y = 1 - (vert_xyz.y * screen_conv.y);
    gl_Position.z = 0;
    gl_Position.w = 1;
    fs_uvw = vert_uvw;
    fs_col = vert_col;
    fs_xyz = vert_xyz;
    fs_rect = vert_rect;
    fs_rounding = rounding;
    fs_softness = softness;
    fs_thickness = thickness;
}