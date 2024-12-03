#include <tinyrender.h>

void ExampleEmptyWindow() {
	tinyrender::init("tinyrenderwgpu", 1280, 720);
	tinyrender::setCameraEye(glm::vec3(0.f, 2.0f, -5.0f));
	while (!tinyrender::shouldQuit()) {
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}

void ExampleSphere() {
	tinyrender::init("tinyrenderwgpu", 1280, 720);
	tinyrender::setCameraEye(glm::vec3(0.f, 2.0f, -5.0f));
	tinyrender::addSphere(1.0f, 16);
	while (!tinyrender::shouldQuit()) {
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}

void ExampleManySpheres() {
	tinyrender::init("tinyrenderwgpu", 1280, 720);
	tinyrender::setCameraEye(glm::vec3(0.f, 1.f, -70.0f));
	for (int i = 0; i < 100; i++) {
		const float x = float(rand() % 50) - 25.0f;
		const float y = float(rand() % 50) - 25.0f;
		const float z = float(rand() % 50) - 25.0f;
		const glm::vec3 s = glm::vec3(1.0f) * (0.35f + float(rand()) / float(RAND_MAX));
		const uint32_t id = tinyrender::addSphere(1.0f, 16);
		tinyrender::updateObject(id, glm::vec3(x, y, z), glm::vec3(0.0f), s);
	}
	while (!tinyrender::shouldQuit()) {
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}

void ExampleRotatedBoxes() {
	tinyrender::init("tinyrenderwgpu", 1280, 720);
	tinyrender::setCameraEye(glm::vec3(0.f, 1.f, -70.0f));
	for (int i = 0; i < 100; i++) {
		const float x = float(rand() % 50) - 25.0f;
		const float y = float(rand() % 50) - 25.0f;
		const float z = float(rand() % 50) - 25.0f;
		const glm::vec3 s = glm::vec3(1.0f) * (0.35f + float(rand()) / float(RAND_MAX));
		const glm::vec3 r = glm::vec3(float(rand() % 180), float(rand() % 180), float(rand() % 180));
		const uint32_t id = tinyrender::addBox(1.0f);
		tinyrender::updateObject(id, glm::vec3(x, y, z), r, s);
	}
	while (!tinyrender::shouldQuit()) {
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}


int main(int /*argc*/, const char** /*argv*/) {
	//ExampleEmptyWindow();
	//ExampleSphere();
	//ExampleManySpheres();
	ExampleRotatedBoxes();
	return 0;
}
