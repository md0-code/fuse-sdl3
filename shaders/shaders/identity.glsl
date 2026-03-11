#version 110

#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#endif

COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec2 TEX0;

uniform mat4 MVPMatrix;

void main()
{
  gl_Position = MVPMatrix * VertexCoord;
  TEX0 = TexCoord.xy;
}

#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define COMPAT_TEXTURE texture2D
#define FragColor gl_FragColor
#endif

uniform sampler2D Texture;
uniform sampler2D source;
COMPAT_VARYING vec2 TEX0;

void main()
{
  vec4 colour = COMPAT_TEXTURE( Texture, TEX0 );
  if( colour.a == 0.0 ) colour = COMPAT_TEXTURE( source, TEX0 );
  colour.a = 1.0;
  FragColor = colour;
}

#endif