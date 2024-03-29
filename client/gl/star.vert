#version 110
uniform mat4 u_lens;//This is the gl_perspective matrix equivalent
uniform mat4 u_cam;//Camera look direction (rotation around eye)
uniform vec3 u_offset;
attribute vec3 a_loc;
varying vec4 color;
void main()
{
	vec3 rel = a_loc-u_offset;
	float br = 50000.0/length(rel);
	color = vec4(br,br,br,1.0);
	gl_Position = u_lens * u_cam * vec4(rel, 1.0);
}
