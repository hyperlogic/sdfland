// 2d sdf renderer

uniform vec4 color;
uniform mat4 modelViewProjMat;
uniform mat3 uvMat;
attribute vec3 position;
attribute vec2 uv;

varying vec2 frag_uv;

void main(void)
{
	gl_Position = modelViewProjMat * vec4(position, 1);
	frag_uv = (uvMat * vec3(uv, 1)).rg;
}
