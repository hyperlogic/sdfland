uniform vec4 color;
uniform sampler2D sdfTexture;
varying vec2 frag_uv;

#define STRIPES 0

void main(void)
{
    float dist = texture2D(sdfTexture, frag_uv).r;

#if STRIPES
    float s1 = 1.0 - exp(-3.0 * abs(dist));
    float s2 = 0.9 + 0.3 * cos(180.0 * dist);
    float shade = s1 * s2;
    vec3 c;
    c.r = (1.0 - sign(dist) * 0.1) * shade;
    c.g = (1.0 - sign(dist) * 0.4) * shade;
    c.b = (1.0 - sign(dist) * 0.7) * shade;
    gl_FragColor = vec4(c, color.a);
#else
    float c = step(0.0, dist);
	gl_FragColor = vec4(c, c, c, color.a);
#endif

}
