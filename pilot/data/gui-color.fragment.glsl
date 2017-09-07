precision lowp float;

uniform sampler2D g_tex;
uniform vec4 g_color;

varying vec4 f_color;

void main() {
    gl_FragColor = g_color * f_color;
}
