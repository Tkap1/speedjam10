
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
	// color = v_color.rgb;
	vec2 uv = v_uv * 2.0 - 1.0;
	float n = texture(noise, v_uv * 0.5 + vec2(cam_pos.z * 0.001, cam_pos.z * 0.005)).r;
	// n = max(0.0, n - 0.3);
	// n = pow(n, 0.25);
	// color = vec3(texture(noise, v_uv).r) * v_color.rgb;
	color.rgb = vec3(0.0);
	float a = smoothstep(0.2, 0.5, abs(uv.x)+n*0.1);
	// float a = 1;
	out_color = vec4(color, a);
}
#endif