#version 450 // GLSL 4.5

layout(location = 0) out vec3 fragColor; // output color for frag shader

// Triangle vertex positions
vec3 positions[3] = vec3[](
	vec3(0.0, -0.4, 0.0),
	vec3(0.4, 0.4, 0.0),
	vec3(-0.4, 0.4, 0.0)
);

// Triangle vertex colors
vec3 colours[3] = vec3[] (
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 1.0);
	fragColor = colours[gl_VertexIndex];
}