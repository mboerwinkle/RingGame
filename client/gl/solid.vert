#version 110
uniform float u_scale;
uniform mat4 u_lens;//This is the gl_perspective matrix equivalent
uniform mat4 u_cam;//Camera look direction (rotation around eye)
uniform mat4 u_camTrs;//position relative to camera translation
uniform mat4 u_modRot;//model rotation
uniform vec3 u_baseColor;
attribute vec3 a_loc;
attribute vec3 a_norm;
varying vec3 color;
void main()
{
	gl_Position = u_lens * u_cam * u_camTrs * u_modRot * vec4(u_scale*a_loc.x, u_scale*a_loc.y, u_scale*a_loc.z, 1.0);
	color = u_baseColor * (0.2 + 0.8 * abs(dot(normalize((u_modRot * vec4(a_norm, 1.0)).xyz), vec3(0, 0, 1.0))));
}
