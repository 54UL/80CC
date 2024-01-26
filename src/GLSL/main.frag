#version 330

out vec4 fgcolor;
in vec3 Tc;

//uniform sampler2D TEXTURE;

void main()
{

//fgcolor = vec4 (0.4,0.5,0.1,1);
  fgcolor = vec4(Tc.xyz,1);

}