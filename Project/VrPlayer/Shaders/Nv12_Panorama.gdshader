shader_type spatial;

render_mode unshaded, cull_disabled;

uniform sampler2D yTexture;
uniform sampler2D uvTexture;

#define PI 3.141592653589793238

void fragment() {
	vec3 dir = normalize((INV_VIEW_MATRIX* vec4(VERTEX, 0.0)).xyz);
	dir.y = -dir.y;
	vec2 texCoord = vec2(atan(dir.z, dir.x), acos(dir.y));
	texCoord /= vec2(2.0 * PI, -PI);
    // texCoord.x -= 0.25;
	
	float y = texture(yTexture, texCoord).r - 16.0/256.0;
	vec2 uv = texture(uvTexture, texCoord).rg - vec2(0.5, 0.5);
	mat3 cvt = mat3(
		vec3(    1,       1,     1),
		vec3(    0, -.34413, 1.772),
		vec3(1.402, -.71414,     0));
	vec3 rgb = cvt * vec3(y, uv);
	ALBEDO = rgb;
	// Place fragment code here.
}
