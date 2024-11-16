#include "tinyrender.h"

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <webgpu-utils/webgpu-utils.h>

#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

using namespace wgpu;

namespace tinyrender {

	static GLFWwindow* m_window;
	int m_width, m_height;
	static Device m_device;
	static Queue m_queue;
	static Surface m_surface;
	static SwapChain m_swapChain;

	static ShaderModule m_shader;
	static RenderPipeline m_renderPipeline;

	struct VertexAttributes {
		glm::vec3 vertex;
		glm::vec3 normal;
	};

	static TextureView _internalNextSurfaceTextureView() {
		SurfaceTexture surfaceTexture;
		m_surface.getCurrentTexture(&surfaceTexture);
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

	static int _internalCreateObject(const object& obj) {
		return 0;
	}

	static void _internalSetupRenderPipeline() {
		// Vertex fetch
		std::vector<VertexAttribute> vertexAttribs(2);

		// Vertex
		vertexAttribs[0].shaderLocation = 0;
		vertexAttribs[0].format = VertexFormat::Float32x3;
		vertexAttribs[0].offset = 0;

		// Normal
		vertexAttribs[1].shaderLocation = 1;
		vertexAttribs[1].format = VertexFormat::Float32x3;
		vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

		VertexBufferLayout vertexBufferLayout;
		vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
		vertexBufferLayout.attributes = vertexAttribs.data();
		vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
		vertexBufferLayout.stepMode = VertexStepMode::Vertex;

		RenderPipelineDescriptor pipelineDesc;
		pipelineDesc.vertex.bufferCount = 1;
		pipelineDesc.vertex.buffers = &vertexBufferLayout;

		pipelineDesc.vertex.module = m_shader;
		pipelineDesc.vertex.entryPoint = "vs_main";
		pipelineDesc.vertex.constantCount = 0;
		pipelineDesc.vertex.constants = nullptr;

		pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
		pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
		pipelineDesc.primitive.frontFace = FrontFace::CCW;
		pipelineDesc.primitive.cullMode = CullMode::None;

		FragmentState fragmentState;
		fragmentState.module = m_shader;
		fragmentState.entryPoint = "fs_main";
		fragmentState.constantCount = 0;
		fragmentState.constants = nullptr;
		pipelineDesc.fragment = &fragmentState;

		BlendState blendState;
		blendState.color.srcFactor = BlendFactor::SrcAlpha;
		blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
		blendState.color.operation = BlendOperation::Add;
		blendState.alpha.srcFactor = BlendFactor::Zero;
		blendState.alpha.dstFactor = BlendFactor::One;
		blendState.alpha.operation = BlendOperation::Add;

		ColorTargetState colorTarget;
		colorTarget.format = TextureFormat::BGRA8Unorm;;
		colorTarget.blend = &blendState;
		colorTarget.writeMask = ColorWriteMask::All;

		fragmentState.targetCount = 1;
		fragmentState.targets = &colorTarget;

		DepthStencilState depthStencilState = Default;
		depthStencilState.depthCompare = CompareFunction::Less;
		depthStencilState.depthWriteEnabled = true;
		TextureFormat depthTextureFormat = TextureFormat::Depth24Plus;
		depthStencilState.format = depthTextureFormat;
		depthStencilState.stencilReadMask = 0;
		depthStencilState.stencilWriteMask = 0;
		pipelineDesc.depthStencil = &depthStencilState;

		pipelineDesc.multisample.count = 1;
		pipelineDesc.multisample.mask = ~0u;
		pipelineDesc.multisample.alphaToCoverageEnabled = false;

		// Create binding layout (don't forget to = Default)
		BindGroupLayoutEntry bindingLayout = Default;
		bindingLayout.binding = 0;
		bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
		bindingLayout.buffer.type = BufferBindingType::Uniform;
		bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

		// Create a bind group layout
		BindGroupLayoutDescriptor bindGroupLayoutDesc{};
		bindGroupLayoutDesc.entryCount = 1;
		bindGroupLayoutDesc.entries = &bindingLayout;
		BindGroupLayout bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

		// Create the pipeline layout
		PipelineLayoutDescriptor layoutDesc{};
		layoutDesc.bindGroupLayoutCount = 1;
		layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
		PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
		pipelineDesc.layout = layout;

		m_renderPipeline = m_device.createRenderPipeline(pipelineDesc);
	}

	bool init(const char* windowName, int width, int height) {
		m_width = width;
		m_height = height;

		// GLFW
		if (!glfwInit()) {
			std::cerr << "Error: could not initialize GLFW" << std::endl;
			return false;
		}

		// Window
		m_window = glfwCreateWindow(width, height, windowName, nullptr, nullptr);
		if (!m_window) {
			std::cerr << "Error: could not open window" << std::endl;
			glfwTerminate();
			return false;
		}

		// Instance
		Instance instance = wgpuCreateInstance(nullptr);

		// Adapter
		m_surface = glfwGetWGPUSurface(instance, m_window);
		RequestAdapterOptions adapterOpts = {};
		adapterOpts.nextInChain = nullptr;
		adapterOpts.compatibleSurface = m_surface;
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
		deviceDesc.label = "TinyrenderDevice";
		deviceDesc.requiredFeatureCount = 0;
		deviceDesc.requiredLimits = nullptr;
		deviceDesc.defaultQueue.nextInChain = nullptr;
		deviceDesc.defaultQueue.label = "The default queue";
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
		m_device = requestDeviceSync(adapter, &deviceDesc);
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
		wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr);

		// Queue
		m_queue = wgpuDeviceGetQueue(m_device);

		// Release the adapter only after it has been fully utilized
		adapter.release();

		// Swap chain
		SwapChainDescriptor swapChainDesc;
		swapChainDesc.width = m_width;
		swapChainDesc.height = m_height;
		swapChainDesc.usage = TextureUsage::RenderAttachment;
		swapChainDesc.format = TextureFormat::BGRA8Unorm;
		swapChainDesc.presentMode = PresentMode::Fifo;
		m_swapChain = m_device.createSwapChain(m_surface, swapChainDesc);
		std::cout << "Got swapchain: " << m_swapChain << std::endl;

		// Shader
		m_shader = _internalLoadShaderModule(RESOURCES_DIR + std::string("/shader.wgsl"), m_device);
		std::cout << "Loaded shaders." << std::endl;

		// Render pipeline
		_internalSetupRenderPipeline();
		std::cout << "Got render pipeline." << std::endl;

		return true;
	}
	
	bool shouldQuit() {
		return glfwWindowShouldClose(m_window);
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
		CommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &encoderDesc);

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

		// Create the render pass and end it immediately
		RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
		renderPass.end();
		renderPass.release();

		// Finally encode and submit the render pass
		CommandBufferDescriptor cmdBufferDescriptor = {};
		cmdBufferDescriptor.label = "Command buffer";
		CommandBuffer command = encoder.finish(cmdBufferDescriptor);
		encoder.release();

		m_queue.submit(1, &command);
		command.release();

		// At the end of the frame
		targetView.release();
	}

	void swap() {
		m_surface.present();
		m_device.tick();
	}

	void terminate() {
		m_surface.unconfigure();
		
		m_queue.release();
		m_surface.release();
		m_device.release();

		glfwDestroyWindow(m_window);
		glfwTerminate();
	}


	int addObject(const object& obj) {
		_internalCreateObject(obj);
		return 0;
	}
	
	void removeObject(int /*id*/) {

	}

	int addSphere(float /*r*/, int /*n*/) {
		return 0;
	}

} // namespace tinyrender
