#include "tinyrender.h"

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <webgpu-utils/webgpu-utils.h>

#include <iostream>

using namespace wgpu;

namespace tinyrender {

	static GLFWwindow* m_window;
	int m_width, m_height;
	static Device m_device;
	static Queue m_queue;
	static Surface m_surface;

	static TextureView GetNextSurfaceTextureView() {
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

		// Configure render surface
		SurfaceConfiguration config = {};
		config.width = m_width;
		config.height = m_height;
		config.usage = TextureUsage::RenderAttachment;
		wgpu::SurfaceCapabilities capabilities;
		m_surface.getCapabilities(adapter, &capabilities);
		config.format = capabilities.formats[0];
		config.viewFormatCount = 0;
		config.viewFormats = nullptr;
		config.device = m_device;
		config.presentMode = PresentMode::Fifo;
		config.alphaMode = CompositeAlphaMode::Auto;

		m_surface.configure(config);

		// Release the adapter only after it has been fully utilized
		adapter.release();

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
		TextureView targetView = GetNextSurfaceTextureView();
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


	int addObject(const object& /*obj*/) {
		return 0;
	}
	
	void removeObject(int /*id*/) {

	}

	int addSphere(float /*r*/, int /*n*/) {
		return 0;
	}

} // namespace tinyrender