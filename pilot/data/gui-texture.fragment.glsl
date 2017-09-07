precision lowp float;

uniform sampler2D g_tex;
uniform vec4 g_color;

varying vec2 f_texture;

void main() {
    gl_FragColor = g_color * vec4(1, 1, 1, texture2D(g_tex, f_texture).a);
}
