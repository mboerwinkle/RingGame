#version 110
varying vec4 color;
void main()
{
	gl_FragColor = vec4(color.xyz,1.0);
	if(color.w > 0.99 || (mod(gl_FragCoord.x+gl_FragCoord.y, 2.0) <= 0.5)){
		gl_FragDepth = gl_FragCoord.z;
	}else{
		gl_FragDepth = 1.0;
	}
}
