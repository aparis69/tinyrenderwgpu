#include "tinyrender.h"

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <webgpu-utils/webgpu-utils.h>

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

using namespace wgpu;

namespace tinyrender {

	struct ObjectInternal {
		Buffer positionBuffer;
		Buffer normalBuffer;
		Buffer indexBuffer;
		uint32_t indexCount;
	};

	struct Camera {
		float zNear = 0.1f, zFar = 500.0f;
		glm::vec3 eye = { 10, 0, 0 };
		glm::vec3 at = { 0, 0, 0 };
		glm::vec3 up = { 0, 0, 1 };
	};

	struct SceneUniforms {
		glm::mat4 projMatrix;
		glm::mat4 viewMatrix;
	};
	static_assert(sizeof(SceneUniforms) % 16 == 0);

	struct Scene {
		GLFWwindow* window;
		int width, height;
		Device device;
		Queue queue;
		Surface surface;
		RenderPipeline renderPipeline;
		Camera camera;

		SceneUniforms sceneUniforms;
		Buffer sceneUniformBuffer;
	};

	static Scene scene;
	static std::vector<ObjectInternal> objects;


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
		requiredLimits.limits.maxVertexAttributes = 2;
		requiredLimits.limits.maxVertexBuffers = 1;
		requiredLimits.limits.maxBufferSize = 1000 * sizeof(float);
		requiredLimits.limits.maxVertexBufferArrayStride = 6 * sizeof(float);
		requiredLimits.limits.maxInterStageShaderComponents = 3;
		requiredLimits.limits.maxDynamicUniformBuffersPerPipelineLayout = 1;

		requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
		requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;

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
		ShaderModule shader = _internalLoadShaderModule(
			RESOURCES_DIR + std::string("/simple.wgsl"), 
			scene.device
		);

		struct VertexAttributes {
			glm::vec3 vertex;
			glm::vec3 normal;
		};

		// Create the render pipeline
		RenderPipelineDescriptor pipelineDesc;

		// Configure the vertex pipeline
		VertexBufferLayout vertexBufferLayout;
		std::vector<VertexAttribute> vertexAttribs(2);

		// Describe the position attribute
		vertexAttribs[0].shaderLocation = 0;
		vertexAttribs[0].format = VertexFormat::Float32x3;
		vertexAttribs[0].offset = 0;

		// Describe the normal attribute
		vertexAttribs[1].shaderLocation = 1;
		vertexAttribs[1].format = VertexFormat::Float32x3;
		vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

		vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
		vertexBufferLayout.attributes = vertexAttribs.data();
		vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
		vertexBufferLayout.stepMode = VertexStepMode::Vertex;

		pipelineDesc.vertex.bufferCount = 1;
		pipelineDesc.vertex.buffers = &vertexBufferLayout;
		pipelineDesc.vertex.module = shader;
		pipelineDesc.vertex.entryPoint = "vs_main";
		pipelineDesc.vertex.constantCount = 0;
		pipelineDesc.vertex.constants = nullptr;

		pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
		pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
		pipelineDesc.primitive.frontFace = FrontFace::CCW;
		pipelineDesc.primitive.cullMode = CullMode::None;

		FragmentState fragmentState;
		fragmentState.module = shader;
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
		pipelineDesc.depthStencil = nullptr;
		pipelineDesc.multisample.count = 1;
		pipelineDesc.multisample.mask = ~0u;
		pipelineDesc.multisample.alphaToCoverageEnabled = false;
		pipelineDesc.layout = nullptr;

		// Create binding layout (don't forget to = Default)
		BindGroupLayoutEntry bindingLayout = Default;
		bindingLayout.binding = 0;
		bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
		bindingLayout.buffer.type = BufferBindingType::Uniform;
		bindingLayout.buffer.minBindingSize = sizeof(SceneUniforms);

		// Create a bind group layout
		BindGroupLayoutDescriptor bindGroupLayoutDesc{};
		bindGroupLayoutDesc.entryCount = 1;
		bindGroupLayoutDesc.entries = &bindingLayout;
		BindGroupLayout bindGroupLayout = scene.device.createBindGroupLayout(bindGroupLayoutDesc);

		// Create the pipeline layout
		PipelineLayoutDescriptor layoutDesc{};
		layoutDesc.bindGroupLayoutCount = 1;
		layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
		PipelineLayout layout = scene.device.createPipelineLayout(layoutDesc);
		pipelineDesc.layout = layout;

		scene.renderPipeline = scene.device.createRenderPipeline(pipelineDesc);

		shader.release();

		// Create the depth texture
		TextureDescriptor depthTextureDesc;
		depthTextureDesc.dimension = TextureDimension::_2D;
		depthTextureDesc.format = depthTextureFormat;
		depthTextureDesc.mipLevelCount = 1;
		depthTextureDesc.sampleCount = 1;
		depthTextureDesc.size = { 640, 480, 1 };
		depthTextureDesc.usage = TextureUsage::RenderAttachment;
		depthTextureDesc.viewFormatCount = 1;
		depthTextureDesc.viewFormats = (WGPUTextureFormat*)&depthTextureFormat;
		Texture depthTexture = scene.device.createTexture(depthTextureDesc);

		// Create the view of the depth texture manipulated by the rasterizer
		TextureViewDescriptor depthTextureViewDesc;
		depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
		depthTextureViewDesc.baseArrayLayer = 0;
		depthTextureViewDesc.arrayLayerCount = 1;
		depthTextureViewDesc.baseMipLevel = 0;
		depthTextureViewDesc.mipLevelCount = 1;
		depthTextureViewDesc.dimension = TextureViewDimension::_2D;
		depthTextureViewDesc.format = depthTextureFormat;
		TextureView depthTextureView = depthTexture.createView(depthTextureViewDesc);
		// TODO: store depthTexture and depthTexture view for the render pass to happen later
	}

	static int _internalCreateObject(const ObjectDescriptor& objDesc) {
		ObjectInternal newObj;

		// Position buffer
		BufferDescriptor bufferDesc;
		bufferDesc.size = objDesc.vertices.size() * sizeof(glm::vec3);
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
		bufferDesc.mappedAtCreation = false;
		newObj.positionBuffer = scene.device.createBuffer(bufferDesc);
		scene.queue.writeBuffer(newObj.positionBuffer, 0, objDesc.vertices.data(), bufferDesc.size);

		// Normal buffer
		bufferDesc.size = objDesc.normals.size() * sizeof(glm::vec3);
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
		bufferDesc.mappedAtCreation = false;
		newObj.normalBuffer = scene.device.createBuffer(bufferDesc);
		scene.queue.writeBuffer(newObj.normalBuffer, 0, objDesc.normals.data(), bufferDesc.size);

		// Triangle buffer
		newObj.indexCount = static_cast<uint32_t>(objDesc.vertices.size() / 3);
		bufferDesc.size = objDesc.triangles.size() * sizeof(int);
		bufferDesc.size = (bufferDesc.size + 3) & ~3;
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;
		newObj.indexBuffer = scene.device.createBuffer(bufferDesc);
		scene.queue.writeBuffer(newObj.indexBuffer, 0, objDesc.triangles.data(), bufferDesc.size);

		// Return index in vector
		objects.push_back(newObj);
		return static_cast<int>(objects.size() - 1);
	}


	bool init(const char* windowName, int width, int height) {
		scene.width = width;
		scene.height = height;

		// GLFW
		if (!glfwInit()) {
			std::cerr << "Error: could not initialize GLFW" << std::endl;
			return false;
		}

		// Window
		scene.window = glfwCreateWindow(width, height, windowName, nullptr, nullptr);
		if (!scene.window) {
			std::cerr << "Error: could not open window" << std::endl;
			glfwTerminate();
			return false;
		}

		// Instance
		Instance instance = wgpuCreateInstance(nullptr);

		// Adapter
		scene.surface = glfwGetWGPUSurface(instance, scene.window);
		RequestAdapterOptions adapterOpts = {};
		adapterOpts.nextInChain = nullptr;
		adapterOpts.compatibleSurface = scene.surface;
		Adapter adapter = requestAdapterSync(instance, &adapterOpts);
		std::cout << "Got adapter." << std::endl;

		// Printing adapter info
		AdapterProperties properties = {};
		properties.nextInChain = nullptr;
		wgpuAdapterGetProperties(adapter, &properties);
		if (properties.name) {
			std::cout << "Adapter name: " << properties.name << std::endl;
		}
		wgpuInstanceRelease(instance);

		// Device
		std::cout << "Requesting device..." << std::endl;
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
		std::cout << "Got device." << std::endl;

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
		config.presentMode = PresentMode::Fifo;
		config.alphaMode = CompositeAlphaMode::Auto;
		scene.surface.configure(config);
		std::cout << "Got surface." << std::endl;

		// Render pipeline
		_internalSetupRenderPipeline();
		std::cout << "Got render pipeline." << std::endl;

		// Uniform buffer
		BufferDescriptor bufferDesc;
		bufferDesc.size = sizeof(SceneUniforms);
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
		bufferDesc.mappedAtCreation = false;
		scene.sceneUniformBuffer = scene.device.createBuffer(bufferDesc);

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
		encoderDesc.label = "My command encoder";
		CommandEncoder encoder = wgpuDeviceCreateCommandEncoder(scene.device, &encoderDesc);

		// Create the render pass that clears the screen with our color
		RenderPassColorAttachment renderPassColorAttachment = {};
		renderPassColorAttachment.view = targetView;
		renderPassColorAttachment.resolveTarget = nullptr;
		renderPassColorAttachment.loadOp = LoadOp::Clear;
		renderPassColorAttachment.storeOp = StoreOp::Store;
		renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };
		renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
		RenderPassDescriptor renderPassDesc = {};
		renderPassDesc.colorAttachmentCount = 1;
		renderPassDesc.colorAttachments = &renderPassColorAttachment;
		renderPassDesc.depthStencilAttachment = nullptr;
		renderPassDesc.timestampWrites = nullptr;

		// Update camera data & buffer
		scene.sceneUniforms.projMatrix = glm::perspective(
			glm::radians(45.0f),
			float(scene.width) / float(scene.height),
			scene.camera.zNear,
			scene.camera.zFar
		);
		scene.sceneUniforms.viewMatrix = glm::lookAt(
			scene.camera.eye,
			scene.camera.at,
			scene.camera.up
		);
		scene.queue.writeBuffer(
			scene.sceneUniformBuffer, 
			0, 
			&scene.sceneUniforms, 
			sizeof(SceneUniforms)
		);

		// Create the render pass
		RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
		renderPass.setPipeline(scene.renderPipeline);
		for (auto& obj : objects) {
			renderPass.setVertexBuffer(0, 
				obj.positionBuffer, 
				0, 
				obj.positionBuffer.getSize()
			);
			renderPass.setIndexBuffer(obj.indexBuffer, 
				IndexFormat::Uint32, 
				0, 
				obj.indexBuffer.getSize()
			);
			renderPass.drawIndexed(obj.indexCount, 1, 0, 0, 0);
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
		for (auto& obj : objects) {
			obj.indexBuffer.release();
			obj.positionBuffer.release();
			obj.normalBuffer.release();
		}

		scene.surface.unconfigure();
		scene.surface.release();
		scene.queue.release();
		scene.device.release();

		glfwDestroyWindow(scene.window);
		glfwTerminate();
	}


	int addObject(const ObjectDescriptor& objDesc) {
		return _internalCreateObject(objDesc);
	}
	
	void removeObject(int /*id*/) {

	}

	int addSphere(float r, int n) {
		ObjectDescriptor newObj;

		const int p = 2 * n;
		const int s = (2 * n) * (n - 1) + 2;
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
				glm::vec3 u = { cos(t) * cos(f), sin(f), sin(t) * cos(f) };
				newObj.normals[k] = u;
				newObj.vertices[k] = u * r;
				k++;
				t += dt;
			}
		}
		// North pole
		newObj.normals[s - 2] = { 0, 1, 0 };
		newObj.vertices[s - 2] = { 0, r, 0 };

		// South
		newObj.normals[s - 1] = { 0, -1, 0 };
		newObj.vertices[s - 1] = { 0, -r, 0 };

		// Reserve space for the smooth triangle array
		newObj.triangles.reserve(4 * n * (n - 1) * 3);

		// South cap
		for (int i = 0; i < 2 * n; i++)
		{
			newObj.triangles.push_back(s - 1);
			newObj.triangles.push_back((i + 1) % p);
			newObj.triangles.push_back(i);
		}

		// North cap
		for (int i = 0; i < 2 * n; i++)
		{
			newObj.triangles.push_back(s - 2);
			newObj.triangles.push_back(2 * n * (n - 2) + i);
			newObj.triangles.push_back(2 * n * (n - 2) + (i + 1) % p);
		}

		// Sphere
		for (int j = 1; j < n - 1; j++)
		{
			for (int i = 0; i < 2 * n; i++)
			{
				const int v0 = (j - 1) * p + i;
				const int v1 = (j - 1) * p + (i + 1) % p;
				const int v2 = j * p + (i + 1) % p;
				const int v3 = j * p + i;

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

} // namespace tinyrender
