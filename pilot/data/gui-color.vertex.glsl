attribute vec2 v_pos;
attribute vec4 v_color;

uniform mat4 g_transform;

varying vec4 f_color;

void main() {
    gl_Position = g_transform * vec4(v_pos.x, v_pos.y, 0, 1);
    f_color = v_color;
}
