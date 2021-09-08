#version 110
uniform mat4 u_lens;//This is the gl_perspective matrix equivalent
uniform mat4 u_cam;//Camera look direction (rotation around eye)
uniform vec4 u_baseColor;
attribute vec3 a_loc;
varying vec4 color;
void main()
{
	gl_Position = u_lens * u_cam * vec4(a_loc, 1.0);
	color = u_baseColor;
}
