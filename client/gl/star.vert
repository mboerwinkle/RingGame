#version 110
uniform mat4 u_lens;//This is the gl_perspective matrix equivalent
uniform mat4 u_cam;//Camera look direction (rotation around eye)
uniform vec3 u_offset;
attribute vec3 a_loc;
void main()
{
	gl_Position = u_lens * u_cam * vec4(1000.0*normalize(a_loc-u_offset), 1.0);
	//gl_Position = u_lens * u_cam * vec4(1.0, 1.0, 1.0, 1.0);
}
