#include "tinyrender.h"

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <webgpu-utils/webgpu-utils.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;
using namespace wgpu;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat4;

namespace tinyrender {

	struct VertexAttributes {
		vec3 position;
		vec3 normal;
	};

	struct ObjectUniforms {
		mat4 modelMatrix;
	};
	static_assert(sizeof(ObjectUniforms) % 16 == 0);

	struct SceneUniforms {
		mat4 projMatrix;
		mat4 viewMatrix;
	};
	static_assert(sizeof(SceneUniforms) % 16 == 0);

	struct ObjectInternal {
		Buffer vertexBuffer;
		Buffer indexBuffer;
		uint16_t drawCount;

		ObjectUniforms uniforms;
		Buffer uniformBuffer;
		BindGroup bindGroup;
	};

	struct Camera {
		float zNear = 0.1f, zFar = 500.0f;
		vec3 eye = vec3(3, -3, 0);
		vec3 at = vec3(0, 0, 0);
		vec3 up = vec3(0, 0, 1);
	};

	struct Scene {
		GLFWwindow* window;
		int width, height;
		Device device;
		Queue queue;
		Surface surface;
		RenderPipeline renderPipeline;
		Camera camera;

		SceneUniforms uniforms;
		Buffer uniformBuffer;
		BindGroup bindGroup;
		std::vector<BindGroupLayout> bindGroupLayouts;
	};

	static Scene scene;
	static std::unordered_map<uint32_t, ObjectInternal> objects;
	static Texture depthTexture;
	static TextureView depthTextureView;

	static mat4 _internalComputeModelMatrix(const vec3& t, const vec3& r, const vec3& s) {
		mat4 ret = glm::identity<mat4>();
		ret = glm::translate(ret, t);
		ret = ret * glm::eulerAngleXYZ(glm::radians(r.x), glm::radians(r.y), glm::radians(r.z));
		ret = glm::scale(ret, s);
		return ret;
	}

	static TextureView _internalNextSurfaceTextureView() {
		SurfaceTexture surfaceTexture;
		scene.surface.getCurrentTexture(&surfaceTexture);
		if (surfaceTexture.status != SurfaceGetCurrentTextureStatus::Success) {
			return nullptr;
		}
		Texture texture = surfaceTexture.texture;

		TextureViewDescriptor viewDescriptor;
		viewDescriptor.label = "Surface texture view";
		viewDescriptor.format = texture.getFormat();
		viewDescriptor.dimension = TextureViewDimension::_2D;
		viewDescriptor.baseMipLevel = 0;
		viewDescriptor.mipLevelCount = 1;
		viewDescriptor.baseArrayLayer = 0;
		viewDescriptor.arrayLayerCount = 1;
		viewDescriptor.aspect = TextureAspect::All;
		TextureView targetView = texture.createView(viewDescriptor);

		wgpuTextureRelease(surfaceTexture.texture);

		return targetView;
	}

	static RequiredLimits _internalSetupWpuLimits(Adapter adapter) {
		SupportedLimits supportedLimits;
		adapter.getLimits(&supportedLimits);

		RequiredLimits requiredLimits = Default;
		requiredLimits.limits.maxVertexAttributes = 3;
		requiredLimits.limits.maxVertexBuffers = 1;
		requiredLimits.limits.maxBufferSize = 2000 * (sizeof(VertexAttributes));
		requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
		requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
		requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
		requiredLimits.limits.maxInterStageShaderComponents = 6;
		requiredLimits.limits.maxBindGroups = 2;
		requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
		requiredLimits.limits.maxUniformBufferBindingSize = 64 * 4 * sizeof(float);
		requiredLimits.limits.maxTextureDimension1D = 4000;
		requiredLimits.limits.maxTextureDimension2D = 4000;
		requiredLimits.limits.maxTextureArrayLayers = 1;

		return requiredLimits;
	}

	static ShaderModule _internalLoadShaderModule(const fs::path& path, Device device) {
		std::ifstream file(path);
		if (!file.is_open()) {
			return nullptr;
		}
		file.seekg(0, std::ios::end);
		size_t size = file.tellg();
		std::string shaderSource(size, ' ');
		file.seekg(0);
		file.read(shaderSource.data(), size);

		ShaderModuleWGSLDescriptor shaderCodeDesc;
		shaderCodeDesc.chain.next = nullptr;
		shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
		shaderCodeDesc.code = shaderSource.c_str();
		ShaderModuleDescriptor shaderDesc;
		shaderDesc.nextInChain = &shaderCodeDesc.chain;

		return device.createShaderModule(shaderDesc);
	}

	static void _internalSetupRenderPipeline() {
		// Load shader module
		ShaderModule shaderModule = _internalLoadShaderModule(
			RESOURCES_DIR + std::string("/simple.wgsl"),
			scene.device
		);

		// Configure the vertex buffer layout
		VertexBufferLayout vertexBufferLayout;
		std::vector<VertexAttribute> attributes(2);

		// Position attribute
		attributes[0].shaderLocation = 0;
		attributes[0].format = VertexFormat::Float32x3;
		attributes[0].offset = 0;

		// Normal attribute
		attributes[1].shaderLocation = 1;
		attributes[1].format = VertexFormat::Float32x3;
		attributes[1].offset = sizeof(vec3); // offset of normal

		vertexBufferLayout.attributeCount = 2;
		vertexBufferLayout.attributes = attributes.data();
		vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
		vertexBufferLayout.stepMode = VertexStepMode::Vertex;

		// Create the render pipeline desc
		RenderPipelineDescriptor pipelineDesc;

		// Vertex state
		pipelineDesc.vertex.bufferCount = 1;
		pipelineDesc.vertex.buffers = &vertexBufferLayout;
		pipelineDesc.vertex.module = shaderModule;
		pipelineDesc.vertex.entryPoint = "vs_main";
		pipelineDesc.vertex.constantCount = 0;
		pipelineDesc.vertex.constants = nullptr;

		// Primitive topology: triangles only
		pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
		pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
		pipelineDesc.primitive.frontFace = FrontFace::CCW;
		pipelineDesc.primitive.cullMode = CullMode::None;

		// Fragment state
		FragmentState fragmentState;
		fragmentState.module = shaderModule;
		fragmentState.entryPoint = "fs_main";
		fragmentState.constantCount = 0;
		fragmentState.constants = nullptr;

		BlendState blendState;
		blendState.color.srcFactor = BlendFactor::SrcAlpha;
		blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
		blendState.color.operation = BlendOperation::Add;
		blendState.alpha.srcFactor = BlendFactor::Zero;
		blendState.alpha.dstFactor = BlendFactor::One;
		blendState.alpha.operation = BlendOperation::Add;

		ColorTargetState colorTarget;
		colorTarget.format = TextureFormat::BGRA8Unorm;
		colorTarget.blend = &blendState;
		colorTarget.writeMask = ColorWriteMask::All;

		fragmentState.targetCount = 1;
		fragmentState.targets = &colorTarget;
		pipelineDesc.fragment = &fragmentState;

		// Depth buffer
		DepthStencilState depthStencilState = Default;
		depthStencilState.depthCompare = CompareFunction::Less;
		depthStencilState.depthWriteEnabled = true;
		TextureFormat depthTextureFormat = TextureFormat::Depth24Plus;
		depthStencilState.format = depthTextureFormat;
		depthStencilState.stencilReadMask = 0;
		depthStencilState.stencilWriteMask = 0;
		pipelineDesc.depthStencil = &depthStencilState;

		// Multisample
		pipelineDesc.multisample.count = 1;
		pipelineDesc.multisample.mask = ~0u;
		pipelineDesc.multisample.alphaToCoverageEnabled = false;

		// Layout 
		scene.bindGroupLayouts = {};
		BindGroupLayoutDescriptor bindGroupLayoutDesc{};

		BindGroupLayoutEntry bindingLayout = Default;
		bindingLayout.binding = 0;
		bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
		bindingLayout.buffer.type = BufferBindingType::Uniform;
		bindingLayout.buffer.minBindingSize = sizeof(SceneUniforms);
		bindGroupLayoutDesc.entryCount = 1;
		bindGroupLayoutDesc.entries = &bindingLayout;
		scene.bindGroupLayouts.push_back(scene.device.createBindGroupLayout(bindGroupLayoutDesc));

		bindingLayout.binding = 0;
		bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
		bindingLayout.buffer.type = BufferBindingType::Uniform;
		bindingLayout.buffer.minBindingSize = sizeof(ObjectUniforms);
		bindGroupLayoutDesc.entryCount = 1;
		bindGroupLayoutDesc.entries = &bindingLayout;
		scene.bindGroupLayouts.push_back(scene.device.createBindGroupLayout(bindGroupLayoutDesc));

		// Actually create the pipeline
		PipelineLayoutDescriptor layoutDesc{};
		layoutDesc.bindGroupLayoutCount = scene.bindGroupLayouts.size();
		layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)scene.bindGroupLayouts.data();
		pipelineDesc.layout = scene.device.createPipelineLayout(layoutDesc);
		scene.renderPipeline = scene.device.createRenderPipeline(pipelineDesc);

		// Release shader module, no need anymore
		shaderModule.release();
	}

	static void _internalSetupDepthTexture() {
		// Create the texture
		TextureDescriptor depthTextureDesc;
		depthTextureDesc.dimension = TextureDimension::_2D;
		depthTextureDesc.format = TextureFormat::Depth24Plus;
		depthTextureDesc.mipLevelCount = 1;
		depthTextureDesc.sampleCount = 1;
		depthTextureDesc.size = { (uint32_t)scene.width, (uint32_t)scene.height, 1 };
		depthTextureDesc.usage = TextureUsage::RenderAttachment;
		depthTextureDesc.viewFormatCount = 1;
		depthTextureDesc.viewFormats = (WGPUTextureFormat*)&TextureFormat::Depth24Plus;
		depthTexture = scene.device.createTexture(depthTextureDesc);

		// Create the view of the texture manipulated by the rasterizer
		TextureViewDescriptor depthTextureViewDesc;
		depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
		depthTextureViewDesc.baseArrayLayer = 0;
		depthTextureViewDesc.arrayLayerCount = 1;
		depthTextureViewDesc.baseMipLevel = 0;
		depthTextureViewDesc.mipLevelCount = 1;
		depthTextureViewDesc.dimension = TextureViewDimension::_2D;
		depthTextureViewDesc.format = TextureFormat::Depth24Plus;
		depthTextureView = depthTexture.createView(depthTextureViewDesc);
	}

	static void _internalSetupSceneData() {
		// Buffer
		BufferDescriptor bufferDesc;
		bufferDesc.size = sizeof(SceneUniforms);
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
		bufferDesc.mappedAtCreation = false;
		scene.uniformBuffer = scene.device.createBuffer(bufferDesc);

		// Binding
		BindGroupEntry binding{};
		binding.binding = 0;
		binding.buffer = scene.uniformBuffer;
		binding.offset = 0;
		binding.size = sizeof(SceneUniforms);

		// Associated bind group with its layout
		BindGroupDescriptor bindGroupDesc;
		bindGroupDesc.layout = scene.bindGroupLayouts[0];
		bindGroupDesc.entryCount = 1;
		bindGroupDesc.entries = &binding;
		scene.bindGroup = scene.device.createBindGroup(bindGroupDesc);
	}

	static void _internalSetupCallbacks() {
		glfwSetInputMode(scene.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

		glfwSetWindowSizeCallback(scene.window, [](
			GLFWwindow* /*window*/, 
			int /*w*/, 
			int /*h*/
			) {
				// TODO: find proper way
			}
		);

		glfwSetMouseButtonCallback(scene.window, [](
			GLFWwindow* /*window*/, 			
			int /*button*/, 
			int /*action*/,
			int /*mods*/
			) {

			}
		);

		glfwSetKeyCallback(scene.window, [](
			GLFWwindow* /*window*/, 
			int /*key*/, 
			int /*scancode*/, 
			int /*action*/, 
			int /*mods*/
			) {

			}
		);

		glfwSetScrollCallback(scene.window, [](
			GLFWwindow* /*window*/,
			double /*x*/,
			double y
			) {
				Camera& cam = scene.camera;
				vec3 viewDir = cam.at - cam.eye;
				cam.eye += viewDir * float(y) * 0.025f;
			}
		);
	}

	static uint32_t _internalCreateObject(const ObjectDescriptor& objDesc) {
		ObjectInternal newObj;

		// Compute the flattened buffer with interleaved position & normal
		std::vector<vec3> flattenedData(objDesc.vertices.size() * 2);
		for (int i = 0; i < objDesc.vertices.size(); i++) {
			flattenedData[(i * 2) + 0] = objDesc.vertices[i];
			flattenedData[(i * 2) + 1] = objDesc.normals[i];
		}

		// Vertex buffer (position + normal)
		BufferDescriptor bufferDesc;
		bufferDesc.size = flattenedData.size() * sizeof(vec3);
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
		bufferDesc.mappedAtCreation = false;
		newObj.vertexBuffer = scene.device.createBuffer(bufferDesc);
		scene.queue.writeBuffer(newObj.vertexBuffer, 0, flattenedData.data(), bufferDesc.size);

		// Triangle buffer
		newObj.drawCount = static_cast<uint16_t>(objDesc.triangles.size());
		bufferDesc.size = objDesc.triangles.size() * sizeof(uint16_t);
		bufferDesc.size = (bufferDesc.size + 3) & ~3; // round up to the next multiple of 4
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;
		newObj.indexBuffer = scene.device.createBuffer(bufferDesc);
		scene.queue.writeBuffer(newObj.indexBuffer, 0, objDesc.triangles.data(), bufferDesc.size);

		// Uniform buffer (with model matrix)
		newObj.uniforms.modelMatrix = _internalComputeModelMatrix(
			objDesc.translation, 
			objDesc.rotation, 
			objDesc.scale
		);
		bufferDesc.size = sizeof(ObjectUniforms);
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
		bufferDesc.mappedAtCreation = false;
		newObj.uniformBuffer = scene.device.createBuffer(bufferDesc);
		scene.queue.writeBuffer(newObj.uniformBuffer, 0, &newObj.uniforms.modelMatrix, bufferDesc.size);

		// Create a binding
		BindGroupLayoutEntry bindingLayout = Default;
		bindingLayout.binding = 0;
		bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
		bindingLayout.buffer.type = BufferBindingType::Uniform;
		bindingLayout.buffer.minBindingSize = sizeof(ObjectUniforms);

		// Create a bind group layout
		BindGroupLayoutDescriptor bindGroupLayoutDesc{};
		bindGroupLayoutDesc.entryCount = 1;
		bindGroupLayoutDesc.entries = &bindingLayout;
		BindGroupLayout bindGroupLayout = scene.device.createBindGroupLayout(bindGroupLayoutDesc);

		BindGroupEntry binding{};
		binding.binding = 0;
		binding.buffer = newObj.uniformBuffer;
		binding.offset = 0;
		binding.size = sizeof(ObjectUniforms);

		// A bind group contains one or multiple bindings
		BindGroupDescriptor bindGroupDesc;
		bindGroupDesc.layout = bindGroupLayout;
		bindGroupDesc.entryCount = bindGroupLayoutDesc.entryCount;
		bindGroupDesc.entries = &binding;
		newObj.bindGroup = scene.device.createBindGroup(bindGroupDesc);

		// Return index in vector
		uint32_t id = uint32_t(objects.size());
		objects.insert({ id, newObj });
		return id;
	}


	bool init(const char* windowName, int width, int height) {
		scene.width = width;
		scene.height = height;
		std::cout << "Initialization" << std::endl;

		// GLFW
		if (!glfwInit()) {
			std::cerr << "Error: could not initialize GLFW" << std::endl;
			return false;
		}
		std::cout << "--- GLFW" << std::endl;

		// Window
		scene.window = glfwCreateWindow(width, height, windowName, nullptr, nullptr);
		if (!scene.window) {
			std::cerr << "Error: could not open window" << std::endl;
			glfwTerminate();
			return false;
		}
		std::cout << "--- window" << std::endl;

		// Instance
		Instance instance = wgpuCreateInstance(nullptr);

		// Adapter
		scene.surface = glfwGetWGPUSurface(instance, scene.window);
		RequestAdapterOptions adapterOpts = {};
		adapterOpts.nextInChain = nullptr;
		adapterOpts.compatibleSurface = scene.surface;
		//adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;
		Adapter adapter = requestAdapterSync(instance, &adapterOpts);
		std::cout << "--- adapter" << std::endl;
		AdapterProperties properties = {};
		properties.nextInChain = nullptr;
		wgpuAdapterGetProperties(adapter, &properties);
		if (properties.name) {
			std::cout << "--- adapter name: " << properties.name << std::endl;
		}
		wgpuInstanceRelease(instance);

		// Device
		DeviceDescriptor deviceDesc = {};
		deviceDesc.nextInChain = nullptr;
		deviceDesc.label = "TinyRenderDevice";
		deviceDesc.requiredFeatureCount = 0;
		auto limits = _internalSetupWpuLimits(adapter);
		deviceDesc.requiredLimits = &limits;
		deviceDesc.defaultQueue.nextInChain = nullptr;
		deviceDesc.defaultQueue.label = "Default queue";
		deviceDesc.deviceLostCallbackInfo.callback = [](
			WGPUDevice const* /*device*/,
			WGPUDeviceLostReason reason,
			char const* message,
			void* /*userdata*/
			) {
				std::cout << "Device lost: reason " << reason;
				if (message) {
					std::cout << " (" << message << ")";
				}
				std::cout << std::endl;
			};
		scene.device = requestDeviceSync(adapter, &deviceDesc);
		std::cout << "-- device" << std::endl;

		// Device error callback
		auto onDeviceError = [](
			WGPUErrorType type,
			char const* message,
			void* /* pUserData */
			) {
				std::cout << "Uncaptured device error: type " << type;
				if (message) {
					std::cout << " (" << message << ")";
				}
				std::cout << std::endl;
			};
		wgpuDeviceSetUncapturedErrorCallback(scene.device, onDeviceError, nullptr);

		// Queue
		scene.queue = wgpuDeviceGetQueue(scene.device);

		// Release the adapter only after it has been fully utilized
		adapter.release();

		// Surface
		SurfaceConfiguration config = {};
		config.width = scene.width;
		config.height = scene.height;
		config.usage = TextureUsage::RenderAttachment;
		wgpu::SurfaceCapabilities capabilities;
		scene.surface.getCapabilities(adapter, &capabilities);
		config.format = capabilities.formats[0];
		config.viewFormatCount = 0;
		config.viewFormats = nullptr;
		config.device = scene.device;
		config.presentMode = PresentMode::FifoRelaxed;
		config.alphaMode = CompositeAlphaMode::Auto;
		scene.surface.configure(config);
		std::cout << "-- surface" << std::endl;

		_internalSetupRenderPipeline();
		std::cout << "-- render pipeline" << std::endl;

		_internalSetupDepthTexture();
		std::cout << "-- depth texture" << std::endl;

		_internalSetupSceneData();
		std::cout << "-- scene buffer and bind groups" << std::endl;

		_internalSetupCallbacks();
		std::cout << "-- callbacks" << std::endl;

		return true;
	}

	bool shouldQuit() {
		return glfwWindowShouldClose(scene.window);
	}

	void update() {
		glfwPollEvents();
	}

	void render() {
		// Get the next target texture view
		TextureView targetView = _internalNextSurfaceTextureView();
		if (!targetView) return;

		// Create a command encoder for the draw call
		CommandEncoderDescriptor encoderDesc = {};
		encoderDesc.label = "Draw Call Encoder";
		CommandEncoder encoder = wgpuDeviceCreateCommandEncoder(scene.device, &encoderDesc);

		// Create the render pass that clears the screen with our color
		RenderPassColorAttachment renderPassColorAttachment = {};
		renderPassColorAttachment.view = targetView;
		renderPassColorAttachment.resolveTarget = nullptr;
		renderPassColorAttachment.loadOp = LoadOp::Clear;
		renderPassColorAttachment.storeOp = StoreOp::Store;
		renderPassColorAttachment.clearValue = Color{ 0.2, 0.2, 0.2, 1.0 };
		renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
		RenderPassDescriptor renderPassDesc = {};
		renderPassDesc.colorAttachmentCount = 1;
		renderPassDesc.colorAttachments = &renderPassColorAttachment;

		// We now add depth/stencil attachment
		RenderPassDepthStencilAttachment depthStencilAttachment;
		depthStencilAttachment.view = depthTextureView;
		depthStencilAttachment.depthClearValue = 1.0f;
		depthStencilAttachment.depthLoadOp = LoadOp::Clear;
		depthStencilAttachment.depthStoreOp = StoreOp::Store;
		depthStencilAttachment.depthReadOnly = false;
		depthStencilAttachment.stencilClearValue = 0;
		depthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
		depthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
		depthStencilAttachment.stencilReadOnly = true;
		renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

		renderPassDesc.timestampWrites = nullptr;

		// Update camera data & buffer
		scene.uniforms.projMatrix = glm::perspective(
			glm::radians(45.0f),
			float(scene.width) / float(scene.height),
			scene.camera.zNear,
			scene.camera.zFar
		);
		scene.uniforms.viewMatrix = glm::lookAt(
			scene.camera.eye,
			scene.camera.at,
			scene.camera.up
		);
		scene.queue.writeBuffer(
			scene.uniformBuffer,
			0,
			&scene.uniforms,
			sizeof(SceneUniforms)
		);

		// Create the render pass
		RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
		renderPass.setPipeline(scene.renderPipeline);
		for (auto& it : objects) {
			auto& obj = it.second;
			renderPass.setVertexBuffer(0,
				obj.vertexBuffer,
				0,
				obj.vertexBuffer.getSize()
			);
			renderPass.setIndexBuffer(obj.indexBuffer,
				IndexFormat::Uint16,
				0,
				obj.indexBuffer.getSize()
			);

			renderPass.setBindGroup(0, scene.bindGroup, 0, nullptr);
			renderPass.setBindGroup(1, obj.bindGroup, 0, nullptr);

			renderPass.drawIndexed(obj.drawCount, 1, 0, 0, 0);
		}

		renderPass.end();
		renderPass.release();

		// Finally encode and submit the render pass
		CommandBufferDescriptor cmdBufferDescriptor = {};
		cmdBufferDescriptor.label = "Command buffer";
		CommandBuffer command = encoder.finish(cmdBufferDescriptor);
		encoder.release();

		scene.queue.submit(1, &command);
		command.release();

		// At the end of the frame
		targetView.release();
	}

	void swap() {
		scene.surface.present();
		scene.device.tick();
	}

	void terminate() {
		for (auto& it : objects) {
			auto& obj = it.second;
			obj.indexBuffer.destroy();
			obj.vertexBuffer.destroy();
			obj.indexBuffer.release();
			obj.vertexBuffer.release();
		}

		depthTexture.destroy();
		depthTexture.release();
		depthTextureView.release();

		scene.surface.unconfigure();
		scene.surface.release();
		scene.queue.release();
		scene.device.release();

		glfwDestroyWindow(scene.window);
		glfwTerminate();
	}


	uint32_t addObject(const ObjectDescriptor& objDesc) {
		return _internalCreateObject(objDesc);
	}

	void removeObject(uint32_t id) {
		assert(id < objects.size());
		ObjectInternal& obj = objects[id];
		obj.indexBuffer.destroy();
		obj.vertexBuffer.destroy();
		obj.indexBuffer.release();
		obj.vertexBuffer.release();
		objects.erase(id);
	}

	void updateObject(uint32_t id, const vec3& t, const vec3& r, const vec3& s) {
		assert(id < objects.size());
		ObjectInternal& obj = objects[id];
		obj.uniforms.modelMatrix = _internalComputeModelMatrix(t, r, s);
		scene.queue.writeBuffer(obj.uniformBuffer, 0, &obj.uniforms.modelMatrix, sizeof(ObjectUniforms));
	}

	uint32_t addSphere(float r, int n) {
		ObjectDescriptor newObj;

		const uint16_t p = 2 * (uint16_t)n;
		const uint16_t s = (2 * (uint16_t)n) * ((uint16_t)n - 1) + 2;
		newObj.vertices.resize(s);
		newObj.normals.resize(s);

		// Create set of vertices
		const float Pi = 3.14159265358979323846f;
		const float HalfPi = Pi / 2.0f;
		const float dt = Pi / float(n);
		const float df = Pi / float(n);
		int k = 0;

		float f = -HalfPi;
		for (int j = 1; j < n; j++)
		{
			f += df;

			// Theta
			float t = 0.0;
			for (int i = 0; i < 2 * n; i++)
			{
				vec3 u = { cos(t) * cos(f), sin(t) * cos(f), sin(f) };
				newObj.normals[k] = u;
				newObj.vertices[k] = u * r;
				k++;
				t += dt;
			}
		}
		// North pole
		newObj.normals[s - 2] = { 0, 0, 1 };
		newObj.vertices[s - 2] = { 0, 0, r };

		// South
		newObj.normals[s - 1] = { 0, 0, -1 };
		newObj.vertices[s - 1] = { 0, 0, -r };

		// Reserve space for the smooth triangle array
		newObj.triangles.reserve(4 * n * (n - 1) * 3);

		// South cap
		for (uint16_t i = 0; i < 2 * n; i++)
		{
			newObj.triangles.push_back(s - 1);
			newObj.triangles.push_back((i + 1) % p);
			newObj.triangles.push_back(i);
		}

		// North cap
		for (uint16_t i = 0; i < 2 * n; i++)
		{
			newObj.triangles.push_back(s - 2);
			newObj.triangles.push_back(2 * (uint16_t)n * ((uint16_t)n - 2) + i);
			newObj.triangles.push_back(2 * (uint16_t)n * ((uint16_t)n - 2) + (i + 1) % p);
		}

		// Sphere
		for (uint16_t j = 1; j < n - 1; j++)
		{
			for (uint16_t i = 0; i < 2 * n; i++)
			{
				const uint16_t v0 = (j - 1) * p + i;
				const uint16_t v1 = (j - 1) * p + (i + 1) % p;
				const uint16_t v2 = j * p + (i + 1) % p;
				const uint16_t v3 = j * p + i;

				newObj.triangles.push_back(v0);
				newObj.triangles.push_back(v1);
				newObj.triangles.push_back(v2);

				newObj.triangles.push_back(v0);
				newObj.triangles.push_back(v2);
				newObj.triangles.push_back(v3);
			}
		}

		return addObject(newObj);
	}

	uint32_t addPlane(float size, int n) {
		n = n + 1;
		vec3 a({ -size, 0.0f, -size });
		vec3 b({ size, 0.0f, size });
		vec3 step = (b - a) / float(n - 1);
		ObjectDescriptor planeObject;

		// Vertices
		for (int i = 0; i < n; i++)
		{
			for (int j = 0; j < n; j++)
			{
				vec3 v = a + vec3({ step.x * i, 0.f, step.z * j });
				planeObject.vertices.push_back(v);
				planeObject.normals.push_back({ 0.f, 1.f, 0.f });
				planeObject.colors.push_back({ 0.7f, 0.7f, 0.7f });
			}
		}

		// Triangles
		for (int i = 0; i < n - 1; i++)
		{
			for (int j = 0; j < n - 1; j++)
			{
				int v0 = (j * n) + i;
				int v1 = (j * n) + i + 1;
				int v2 = ((j + 1) * n) + i;
				int v3 = ((j + 1) * n) + i + 1;

				// tri 0
				planeObject.triangles.push_back((uint16_t)v0);
				planeObject.triangles.push_back((uint16_t)v1);
				planeObject.triangles.push_back((uint16_t)v2);

				// tri 1
				planeObject.triangles.push_back((uint16_t)v2);
				planeObject.triangles.push_back((uint16_t)v1);
				planeObject.triangles.push_back((uint16_t)v3);
			}
		}

		return addObject(planeObject);
	}

	uint32_t addBox(float r) {
		const vec3 a = vec3(-r);
		const vec3 b = vec3(r);

		ObjectDescriptor newObj;

		// x negative
		newObj.vertices.push_back({ a.x, a.y, a.z }); newObj.vertices.push_back({ a.x, b.y, a.z });
		newObj.vertices.push_back({ a.x, b.y, b.z }); newObj.vertices.push_back({ a.x, a.y, b.z });
		newObj.normals.push_back({ -1, 0, 0 });	newObj.normals.push_back({ -1, 0, 0 });
		newObj.normals.push_back({ -1, 0, 0 });	newObj.normals.push_back({ -1, 0, 0 });
		newObj.triangles.push_back(0); newObj.triangles.push_back(1); newObj.triangles.push_back(2);
		newObj.triangles.push_back(0); newObj.triangles.push_back(2); newObj.triangles.push_back(3);

		// x positive
		newObj.vertices.push_back({ b.x, a.y, a.z }); newObj.vertices.push_back({ b.x, b.y, a.z });
		newObj.vertices.push_back({ b.x, b.y, b.z }); newObj.vertices.push_back({ b.x, a.y, b.z });
		newObj.normals.push_back({ 1, 0, 0 });	newObj.normals.push_back({ 1, 0, 0 });
		newObj.normals.push_back({ 1, 0, 0 });	newObj.normals.push_back({ 1, 0, 0 });
		newObj.triangles.push_back(4); newObj.triangles.push_back(5); newObj.triangles.push_back(6);
		newObj.triangles.push_back(4); newObj.triangles.push_back(6); newObj.triangles.push_back(7);

		// y negative
		newObj.vertices.push_back({ a.x, a.y, a.z }); newObj.vertices.push_back({ a.x, a.y, b.z });
		newObj.vertices.push_back({ b.x, a.y, b.z }); newObj.vertices.push_back({ b.x, a.y, a.z });
		newObj.normals.push_back({ 0, -1, 0 });	newObj.normals.push_back({ 0, -1, 0 });
		newObj.normals.push_back({ 0, -1, 0 });	newObj.normals.push_back({ 0, -1, 0 });
		newObj.triangles.push_back(8); newObj.triangles.push_back(9); newObj.triangles.push_back(10);
		newObj.triangles.push_back(8); newObj.triangles.push_back(10); newObj.triangles.push_back(11);

		// y positive
		newObj.vertices.push_back({ a.x, b.y, a.z }); newObj.vertices.push_back({ a.x, b.y, b.z });
		newObj.vertices.push_back({ b.x, b.y, b.z }); newObj.vertices.push_back({ b.x, b.y, a.z });
		newObj.normals.push_back({ 0, 1, 0 });	newObj.normals.push_back({ 0, 1, 0 });
		newObj.normals.push_back({ 0, 1, 0 });	newObj.normals.push_back({ 0, 1, 0 });
		newObj.triangles.push_back(12); newObj.triangles.push_back(13); newObj.triangles.push_back(14);
		newObj.triangles.push_back(12); newObj.triangles.push_back(14); newObj.triangles.push_back(15);

		// z negative
		newObj.vertices.push_back({ a.x, a.y, a.z }); newObj.vertices.push_back({ a.x, b.y, a.z });
		newObj.vertices.push_back({ b.x, b.y, a.z }); newObj.vertices.push_back({ b.x, a.y, a.z });
		newObj.normals.push_back({ 0, 0, -1 });	newObj.normals.push_back({ 0, 0, -1 });
		newObj.normals.push_back({ 0, 0, -1 });	newObj.normals.push_back({ 0, 0, -1 });
		newObj.triangles.push_back(16); newObj.triangles.push_back(17); newObj.triangles.push_back(18);
		newObj.triangles.push_back(16); newObj.triangles.push_back(18); newObj.triangles.push_back(19);

		// z positive
		newObj.vertices.push_back({ a.x, a.y, b.z }); newObj.vertices.push_back({ a.x, b.y, b.z });
		newObj.vertices.push_back({ b.x, b.y, b.z }); newObj.vertices.push_back({ b.x, a.y, b.z });
		newObj.normals.push_back({ 0, 0, 1 });	newObj.normals.push_back({ 0, 0, 1 });
		newObj.normals.push_back({ 0, 0, 1 });	newObj.normals.push_back({ 0, 0, 1 });
		newObj.triangles.push_back(20); newObj.triangles.push_back(21); newObj.triangles.push_back(22);
		newObj.triangles.push_back(20); newObj.triangles.push_back(22); newObj.triangles.push_back(23);

		return addObject(newObj);
	}


	void setCameraEye(const vec3& eye) {
		scene.camera.eye = eye;
	}

} // namespace tinyrender
