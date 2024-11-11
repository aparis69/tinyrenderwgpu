#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>
#include <glfw3webgpu/glfw3webgpu.h>

#include <glm/glm.hpp>

namespace tinyrender {

	// Public interface
	struct object {
	public:
		glm::vec3 translation = { 0, 0, 0 };
		glm::vec3 rotation = { 0, 0, 0 };
		glm::vec3 scale = { 1, 1, 1 };
		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec3> colors;
		std::vector<int> triangles;
	};

	// Windowing
	void init(const char* windowName = "tinyrender", int width = -1, int height = -1);
	bool shouldQuit();
	void update();
	void render();
	void swap();
	void terminate();

	// Object management
	int addObject(const object& obj);
	void removeObject(int id);

	// Primitives
	int addSphere(float r, int n);

} // namespace tinyrender
