attribute vec2 v_pos;
attribute vec2 v_tex;

uniform mat4 g_transform;

varying vec2 f_texture;

void main() {
    gl_Position = g_transform * vec4(v_pos.x, v_pos.y, 0, 1);
    f_texture = v_tex;
}
