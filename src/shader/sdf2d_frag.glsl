uniform vec4 color;
uniform sampler2D sdfTexture;

varying vec2 frag_uv;

void main(void)
{
    //float dist = texture2D(sdfTexture, frag_uv).r;
	gl_FragColor = color * texture2D(sdfTexture, frag_uv);
}
