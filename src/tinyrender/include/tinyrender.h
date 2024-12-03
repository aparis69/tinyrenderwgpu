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
	bool init(const char* windowName = "tinyrender", int width = -1, int height = -1);
	bool shouldQuit();
	void update();
	void render();
	void swap();
	void terminate();

	// Object management
	int addObject(const ObjectDescriptor& objDesc);
	void removeObject(int id);
	void updateObject(int id, const glm::vec3& p, const glm::vec3& s, const glm::vec3& r);

	// Camera
	void setCameraEye(const glm::vec3& eye);

	// Primitives
	int addSphere(float r, int n);
	int addPlane(float size, int n);

} // namespace tinyrender
