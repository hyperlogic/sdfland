uniform vec4 color;
uniform sampler2D colorTexture;

varying vec2 frag_uv;

void main(void)
{
	gl_FragColor = color * texture2D(colorTexture, frag_uv);
}
