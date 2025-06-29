
#if defined(m_vertex)
layout (location = 0) in vec3 vertex_pos;
layout (location = 1) in vec3 vertex_normal;
layout (location = 2) in vec4 vertex_color;
layout (location = 3) in vec2 vertex_uv;
layout (location = 4) in vec4 instance_color;
layout (location = 5) in vec4 instance_uv_min;
layout (location = 6) in vec4 instance_uv_max;
layout (location = 7) in mat4 instance_model;
#endif

shared_var vec4 v_color;
shared_var vec3 v_normal;
shared_var vec2 v_uv;

#if defined(m_vertex)
void main()
{
	vec3 vertex = vertex_pos;
	gl_Position = projection * view * instance_model * vec4(vertex, 1);
	v_color = vertex_color * instance_color;
	v_normal = vertex_normal;
	v_uv = vertex_uv;
}
#endif

#if !defined(m_vertex)

uniform sampler2D in_texture;
uniform sampler2D noise;

out vec4 out_color;

void main()
{
	vec3 color = vec3(0.0);
	vec2 uv = v_uv * 2.0 - 1.0;
	float curve = abs(uv.x);
	curve = smoothstep(0.5, 0.0, curve);
	vec2 uv2 = v_uv * vec2(curve, 1.0);
	float n = texture(noise, uv2 + vec2(0.0, cam_pos.z * 0.01)).r;
	float strength = 0.1;
	float d = distance(vec2(player_pos.x * 0.04, 0.5), uv);
	d = smoothstep(0.4, 0.0, d);
	strength += d * 0.5;
	strength -= pow(smoothstep(0.2, 0.9, curve), 16.0) * 0.05;
	color = vec3(n * strength);
	out_color = vec4(color, 1.0);
}
#endif