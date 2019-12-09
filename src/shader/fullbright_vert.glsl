//
// No lighting at all, solid color
//

uniform vec4 color;
uniform mat4 modelViewProjMat;
attribute vec3 position;

void main(void)
{
    gl_Position = modelViewProjMat * vec4(position, 1);
}
