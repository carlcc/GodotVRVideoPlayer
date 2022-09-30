shader_type canvas_item;

uniform sampler2D yTexture;
uniform sampler2D uTexture;
uniform sampler2D vTexture;

void fragment() {
	// Place fragment code here.
	float y = texture(yTexture, UV).r - 16.0/256.0;
	float u = texture(uTexture, UV).r - 0.5;
	float v = texture(vTexture, UV).r - 0.5;
	mat3 cvt = mat3(
		vec3(    1,       1,     1),
		vec3(    0, -.34413, 1.772),
		vec3(1.402, -.71414,     0));
	vec3 rgb = cvt * vec3(y, u, v);
	rgb = rgb.xyz;
	COLOR = vec4(rgb, 1.0);
}
