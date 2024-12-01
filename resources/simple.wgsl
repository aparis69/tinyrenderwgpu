// Uniform structs
struct SceneUniforms {
	projMatrix: mat4x4f,
	viewMatrix: mat4x4f
};
@group(0) @binding(0) var<uniform> uSceneUniforms: SceneUniforms;

struct ModelUniforms {
	modelMatrix: mat4x4f
};
@group(1) @binding(0) var<uniform> uModelUniforms: ModelUniforms;

struct VertexIn {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
};

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
	var out: VertexOut;
    out.position = uSceneUniforms.projMatrix * uSceneUniforms.viewMatrix * vec4f(in.position, 1.0f);
    out.normal = (vec4f(in.normal, 0.0f)).xyz;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    return vec4f(0.2f * (vec3f(3.0f) + 2.0f * in.normal.xyz), 1.0f);
}
