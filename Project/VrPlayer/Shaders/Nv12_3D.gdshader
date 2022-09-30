shader_type spatial;

render_mode unshaded, cull_disabled;

uniform sampler2D yTexture;
uniform sampler2D uvTexture;

void fragment() {
	float y = texture(yTexture, UV).r - 16.0/256.0;
	vec2 uv = texture(uvTexture, UV).rg - vec2(0.5, 0.5);
	mat3 cvt = mat3(
		vec3(    1,       1,     1),
		vec3(    0, -.34413, 1.772),
		vec3(1.402, -.71414,     0));
	vec3 rgb = cvt * vec3(y, uv);
	ALBEDO = rgb;
	// Place fragment code here.
}
