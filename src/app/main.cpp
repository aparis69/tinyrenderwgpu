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
		const float r = (float(rand()) / float(RAND_MAX)) * 1.5f + 0.5f;
		const int id = tinyrender::addSphere(r, 16);
		tinyrender::updateObject(id, glm::vec3(x, y, z), glm::vec3(0.0f), glm::vec3(1.0f));
	}
	while (!tinyrender::shouldQuit()) {
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}

int main(int /*argc*/, const char** /*argv*/) {
	ExampleEmptyWindow();
	//ExampleSphere();
	//ExampleManySpheres();
	return 0;
}
