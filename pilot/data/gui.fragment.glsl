precision mediump float;

uniform sampler2D g_tex;
varying vec4 f_color;
varying vec2 f_texture;

void main() {
    gl_FragColor = f_color * texture2D(g_tex, f_texture);
}
