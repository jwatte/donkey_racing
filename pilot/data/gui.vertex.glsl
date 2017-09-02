attribute vec2 v_pos;
attribute vec2 v_tex;
attribute vec4 v_color;

uniform mat4 g_transform;
uniform vec4 g_color;

varying vec4 f_color;
varying vec2 f_texture;

void main() {
    gl_Position = g_transform * vec4(v_pos.x, v_pos.y, 0, 1);
    f_color = v_color * g_color;
    f_texture = v_tex;
}
