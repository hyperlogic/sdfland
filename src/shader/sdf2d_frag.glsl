uniform vec4 color;
uniform sampler2D sdfTexture;
varying vec2 frag_uv;

void main(void)
{
    float dist = texture2D(sdfTexture, frag_uv).r;

    //vec3 fuck = vec3(0.7, 0.7, 0.9) * color;
    // convert dist into a color (with fancy stripes)
    //vec3 xxx = mix(fuck, vec3(0.7, 0.7, 0.9), 1.0 - exp(-0.0001 * dist * dist * dist));

    float s1 = 1.0 - exp(-3.0 * abs(dist));
    float s2 = 0.9 + 0.3 * cos(180.0 * dist);
    float shade = s1 * s2;
    vec3 c;
    c.r = (1.0 - sign(dist) * 0.1) * shade;
    c.g = (1.0 - sign(dist) * 0.4) * shade;
    c.b = (1.0 - sign(dist) * 0.7) * shade;
	gl_FragColor = vec4(c, color.a);
}
