
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
shared_var vec4 v_light_frag_pos;
shared_var vec3 v_frag_pos;
shared_var vec2 v_uv;

#if defined(m_vertex)
void main()
{
	vec3 vertex = vertex_pos;
	gl_Position = projection * view * instance_model * vec4(vertex, 1.0);
	v_color = vertex_color * instance_color;
	v_normal = vertex_normal;
	v_light_frag_pos = light_projection * light_view * instance_model * vec4(vertex, 1.0);
	v_uv = vertex_uv;
	v_frag_pos = (instance_model * vec4(vertex_pos, 1.0)).xyz;
}
#endif

#if !defined(m_vertex)

uniform sampler2D in_texture;

out vec4 out_color;

void main()
{
	vec3 normal = normalize(v_normal);
	vec3 color = vec3(0.0);

	vec3 dir = normalize(cam_pos.xyz - v_frag_pos);
	float n = dot(dir, normal);
	float n2 = 0.0;
	const float step = 0.9;
	if(n > 0.0) {
		n2 = smoothstep(step, 0.0, n);
	}
	else {
		n2 = smoothstep(-step, 0.0, n);
	}
	// color.r += n2;
	vec3 fresnel_color = v_color.rgb * 2.0;
	color = mix(v_color.rgb * 0.5, fresnel_color, n2);

	out_color = vec4(color, 1.0);
}
#endif