/**
 * tinyrenderwebpu - version 0.1
 * Author - Axel Paris
 *
 * This is free and unencumbered software released into the public domain.
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.

 * Documentation
 * -------------------------------
 * tinyrenderwebpu is a minimalist single .h/.cpp viewer based on WebGPU and GLFW with few dependencies.
 * The API is meant to be as simple as possible, with possibly many options not exposed to the user. Feel
 * free to modify it for your own purposes, the set of examples in main.cpp is rather simple.
 * 
 * Note: functions marked as internal are not meant to be used outside the renderer. Use at your own risk.
 * Note: the renderer is not meant to be fast or efficient.
 * 
 * Library is currently very WIP. All of this is inspired by (https://github.com/aparis69/tinyrender), 
 * my previous attempt at doing a tinyrender lib, using OpenGL.
 *
 * Functionalities
 *   -The up direction is (0, 0, 1)
 *   -Internal representation: an object is a triangle mesh
 *   -Scene API: objects can be added, deleted, and modified at runtime. 
 *	  Each object can be translated/rotated/scaled.
 *
 * Controls
 *	 -Rotation around focus point: left button + move for rotation
 *   -Screen-space panning using middle button + move
 *   -Zoom using mouse scroll
 *
 * Dependencies (all are included either in CMake or in the source tree)
 *	 -Dear imgui
 *   -WebGPU (Dawn implementation)
 *   -GLFW
 *   -GLM
 *	 -STL
*/

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

	// Utilities
	glm::vec2 getMousePosition();
	void setCameraEye(
		const glm::vec3& eye
	);

	// Primitives
	uint32_t addSphere(float r, int n);
	uint32_t addPlane(float size, int n);
	uint32_t addBox(float r);

} // namespace tinyrender
