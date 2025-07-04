
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
shared_var vec2 v_vertex_uv;
shared_var vec2 v_instance_uv;

#if defined(m_vertex)
void main()
{
	vec3 vertex = vertex_pos;
	gl_Position = projection * view * instance_model * vec4(vertex, 1);
	v_color = vertex_color * instance_color;
	v_normal = vertex_normal;
	v_vertex_uv = vertex_uv;

	vec2 uv_arr[6] = vec2[](
		vec2(instance_uv_min.x, instance_uv_min.y),
		vec2(instance_uv_max.x, instance_uv_min.y),
		vec2(instance_uv_max.x, instance_uv_max.y),
		vec2(instance_uv_min.x, instance_uv_min.y),
		vec2(instance_uv_max.x, instance_uv_max.y),
		vec2(instance_uv_min.x, instance_uv_max.y)
	);
	v_instance_uv = uv_arr[gl_VertexID % 6];
}
#endif

#if !defined(m_vertex)

uniform sampler2D in_texture;

out vec4 out_color;

void main()
{
	vec4 texture_color = texture(in_texture, v_instance_uv);
	out_color = texture_color * v_color;
	if(out_color.a <= 0.0) { discard; }
}
#endif