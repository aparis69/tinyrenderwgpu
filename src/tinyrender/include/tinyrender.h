#pragma once 

#include <GLFW/glfw3.h>
#include <glfw3webgpu/glfw3webgpu.h>
#include <glm/glm.hpp>

#include <vector>

namespace tinyrender {

	// Public interface
	struct ObjectDescriptor {
	public:
		glm::vec3 translation = { 0, 0, 0 };
		glm::vec3 rotation = { 0, 0, 0 };
		glm::vec3 scale = { 1, 1, 1 };
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec3> colors;
		std::vector<uint16_t> triangles;
	};

	// Windowing
	bool init(
		const char* windowName = "tinyrender", 
		int width = -1, 
		int height = -1
	);
	bool shouldQuit();
	void update();
	void render();
	void swap();
	void terminate();

	// Object management
	uint32_t addObject(
		const ObjectDescriptor& objDesc
	);
	void removeObject(
		uint32_t id
	);
	void updateObject(uint32_t id, 
		const glm::vec3& t, 
		const glm::vec3& r, 
		const glm::vec3& s
	);

	// Camera
	void setCameraEye(
		const glm::vec3& eye
	);

	// Primitives
	uint32_t addSphere(float r, int n);
	uint32_t addPlane(float size, int n);
	uint32_t addBox(float r);

} // namespace tinyrender
