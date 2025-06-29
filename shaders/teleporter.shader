
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
	mat4 m = projection * view * instance_model;
	float d = sqrt(m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]);
	m[0][0] = d;
	m[1][1] = d;
	m[2][2] = d;

	m[0][1] = 0.0;
	m[0][2] = 0.0;
	m[0][3] = 0.0;

	m[1][0] = 0.0;
	m[1][2] = 0.0;
	m[1][3] = 0.0;

	m[2][0] = 0.0;
	m[2][1] = 0.0;
	m[2][3] = 0.0;

	vec3 vertex = vertex_pos;
	gl_Position = m * vec4(vertex, 1);
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
	vec2 uv = v_uv * 2.0 - 1.0;
	uv.x *= 16.0 / 9.0;
	vec3 color = vec3(0.0);
	float d = length(uv);
	float outter = smoothstep(-0.1, 0.3, d);

	vec2 dir[4] = vec2[](
		vec2(render_time),
		vec2(-render_time),
		vec2(render_time, -render_time),
		vec2(-render_time, render_time)
	);

	for(int ii = 0; ii < 8; ii += 1) {
		float i = float(ii);
		float n = texture(noise, v_uv * 0.5 + dir[ii % 4] * 0.1 + vec2(sin(i * 123456789.0))).r;
		n = max(n - pow(d, 0.5), 0.0);
		float g = smoothstep(0.3, 0.5, d) * 5.0;
		float b = smoothstep(0.4, 0.5, d) * 5.0;
		color += vec3(1.0, g, 2.0 + b) * outter * n;
	}
	float a = smoothstep(0.5, 0.45, d);
	color.rgb *= v_color.rgb;
	color.rgb += 0.05;
	// color = v_color.rgb * v_color.rgb * n;
	// color = vec3(n);
	out_color = vec4(color, a * v_color.a);
}
#endif