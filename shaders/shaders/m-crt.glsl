
#pragma parameter OVERSCAN_X "X-Axis Overscan Amount" 1.02 0.95 1.05 0.01
#pragma parameter OVERSCAN_Y "Y-Axis Overscan Amount" 1.02 0.95 1.05 0.01
#pragma parameter CURVATURE_X "X-Axis Curvature Amount" 0.021 0.0 0.125 0.01
#pragma parameter CURVATURE_Y "X-Axis Curvature Amount" 0.021 0.0 0.125 0.01
#pragma parameter BLUR_STRENGTH "Blur Strength" 0.8 0.0 8.0 0.05
#pragma parameter MONOCHROME_DISPLAY "Monochrome Display" 0.0 0.0 1.0 1.0
#pragma parameter MONOCHROME_TYPE "Monochrome Phosphorus Type" 1.0 1.0 3.0 1.0
#pragma parameter MONOCHROME_COLORS "Monochrome Colors" 256.0 2.0 256.0 1.0
#pragma parameter CORNER_SIZE "Corner Size" 0.01 0.0 0.1 0.01
#pragma parameter SCANLINES_STRENGTH "Scanlines Strength" 0.05 0.0 0.25 0.01
#pragma parameter LIGHT_STRENGTH "Vignetting Strength" 8.0 0.0 20.0 1.0
#pragma parameter GAMMA "Gamma" 0.8 0.4 1.2 0.1
#pragma parameter CONTRAST "Contrast" 1.0 0.4 1.2 0.1
#pragma parameter SATURATION "Saturation" 1.0 0.8 1.2 0.1
#pragma parameter BRIGHTNESS "Brightness" 1.0 0.8 1.6 0.1

#ifdef PARAMETER_UNIFORM
uniform float OVERSCAN_X;
uniform float OVERSCAN_Y;
uniform float CURVATURE_X;
uniform float CURVATURE_Y;
uniform float BLUR_STRENGTH;
uniform float MONOCHROME_DISPLAY;
uniform float MONOCHROME_TYPE;
uniform float MONOCHROME_COLORS;
uniform float CORNER_SIZE;
uniform float SCANLINES_STRENGTH;
uniform float LIGHT_STRENGTH;
uniform float GAMMA;
uniform float CONTRAST;
uniform float SATURATION;
uniform float BRIGHTNESS;
#else
#define OVERSCAN_X 1.02
#define OVERSCAN_Y 1.02
#define CURVATURE_X .021
#define CURVATURE_Y .021
#define BLUR_STRENGTH .8
#define MONOCHROME_DISPLAY 0.
#define MONOCHROME_TYPE 1.
#define MONOCHROME_COLORS 256
#define CORNER_SIZE .01
#define SCANLINES_STRENGTH 0.05
#define LIGHT_STRENGTH 8.0
#define GAMMA .8
#define CONTRAST 1.
#define SATURATION 1.
#define BRIGHTNESS 1.
#endif

#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 TEX0;

uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

void main()
{
	gl_Position = VertexCoord.x * MVPMatrix[0] + VertexCoord.y * MVPMatrix[1] + VertexCoord.z * MVPMatrix[2] + VertexCoord.w * MVPMatrix[3];
	TEX0.xy = TexCoord.xy;
}

#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

uniform sampler2D source;
COMPAT_VARYING vec4 TEX0;

vec3 postEffects(in vec3 rgb)
{
	rgb = pow(rgb, vec3(GAMMA));
	rgb = mix(vec3(.5), mix(vec3(dot(vec3(.2125, .7154, .0721), rgb * BRIGHTNESS)), rgb * BRIGHTNESS, SATURATION), CONTRAST);
	return rgb;
}

// Sigma 1. Size 3
vec3 gaussian(in vec2 uv)
{
	float b = BLUR_STRENGTH / (OutputSize.x / OutputSize.y);

	vec3 col = COMPAT_TEXTURE(source, vec2(uv.x - b / OutputSize.x, uv.y - b / OutputSize.y)).rgb * 0.077847;
	col += COMPAT_TEXTURE(source, vec2(uv.x - b / OutputSize.x, uv.y)).rgb * 0.123317;
	col += COMPAT_TEXTURE(source, vec2(uv.x - b / OutputSize.x, uv.y + b / OutputSize.y)).rgb * 0.077847;

	col += COMPAT_TEXTURE(source, vec2(uv.x, uv.y - b / OutputSize.y)).rgb * 0.123317;
	col += COMPAT_TEXTURE(source, vec2(uv.x, uv.y)).rgb * 0.195346;
	col += COMPAT_TEXTURE(source, vec2(uv.x, uv.y + b / OutputSize.y)).rgb * 0.123317;

	col += COMPAT_TEXTURE(source, vec2(uv.x + b / OutputSize.x, uv.y - b / OutputSize.y)).rgb * 0.077847;
	col += COMPAT_TEXTURE(source, vec2(uv.x + b / OutputSize.x, uv.y)).rgb * 0.123317;
	col += COMPAT_TEXTURE(source, vec2(uv.x + b / OutputSize.x, uv.y + b / OutputSize.y)).rgb * 0.077847;

	return col;
}

// Corner roundness
float corners(vec2 coord)
{
	coord *= TextureSize.xy / InputSize.xy;
	coord = (coord - vec2(0.5)) * 1.0 + vec2(0.5);
	coord = min(coord, vec2(1.0) - coord) * vec2(1.0, InputSize.y / InputSize.x);
	vec2 cdist = vec2(CORNER_SIZE);
	coord = (cdist - min(coord, cdist));
	float dist = sqrt(dot(coord, coord));
	return clamp((cdist.x - dist) * 1000.0, 0.0, 1.0);
}

void main()
{
	vec2 st = TEX0.xy * (TextureSize.xy / InputSize.xy);

	// Overscan
	st = st * 2.0 - 1.0;
	st *= vec2(OVERSCAN_X, OVERSCAN_Y);
	st = st * 0.5 + 0.5;

	vec2 uv = st;

	// Curvature
	if (CURVATURE_X > 0.0 && CURVATURE_Y > 0.0)
	{
		vec2 crtDistortion = vec2(CURVATURE_X, CURVATURE_Y) * 15.0;
		vec2 curvedCoords = st * 2.0 - 1.0;
		float curvedCoordsDistance = sqrt(curvedCoords.x * curvedCoords.x + curvedCoords.y * curvedCoords.y);
		curvedCoords = curvedCoords / curvedCoordsDistance;
		curvedCoords = curvedCoords * (1.0 - pow(vec2(1.0 - (curvedCoordsDistance / 1.4142135623730950488016887242097)), (1.0 / (1.0 + crtDistortion * 0.2))));
		curvedCoords = curvedCoords / (1.0 - pow(vec2(0.29289321881345247559915563789515), (1.0 / (vec2(1.0) + crtDistortion * 0.2))));
		uv = curvedCoords * 0.5 + 0.5;
	}

	float d = length((uv - .5) * .5 * (uv - .5) * .5);
	uv *= InputSize.xy / TextureSize.xy;
	vec3 color = COMPAT_TEXTURE(source, uv).rgb;

	// Color blur
	if (BLUR_STRENGTH > 0.0)
		color = gaussian(uv);

	// Monochrome display
	if (MONOCHROME_DISPLAY == 1.0)
	{
		vec3 ink;
		if (MONOCHROME_TYPE == 1.0)
		{
			ink = vec3(0.5, 0.5, 0.5); // gray
		}
		else if (MONOCHROME_TYPE == 2.0)
		{
			ink = vec3(0., 0.4, 0.); // green
		}
		else if (MONOCHROME_TYPE == 3.0)
		{
			ink = vec3(0.5, 0.3, 0.0); // amber
		}
		color = vec3(floor(MONOCHROME_COLORS * length(color)) / MONOCHROME_COLORS * ink);
	}

	// Corners
	if (CORNER_SIZE > 0.0)
		color *= corners(uv);

	// Light
	color *= 1.0 - min(1.0, d * LIGHT_STRENGTH);

	// Scanlines
	float showScanlines = 1.0;
	if (OutputSize.y < 360.0)
		showScanlines = 0.0;
	float s = 1. - smoothstep(320., 1440., OutputSize.y) + 1.;
	float j = cos(uv.y * OutputSize.y * s) * SCANLINES_STRENGTH;
	color = abs(showScanlines - 1.) * color + showScanlines * (color - color * j);
	color *= 1. - (.01 + ceil(mod((st.x + .5) * OutputSize.x, 3.0)) * (.995 - 1.01)) * showScanlines;

	// Color correction
	color = postEffects(color);

	FragColor = vec4(color, 1.0);
}

#endif