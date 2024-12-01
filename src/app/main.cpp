#include <tinyrender.h>

int main(int /*argc*/, const char** /*argv*/) {
	tinyrender::init("tinyrenderwgpu", 1920, 1080);
	tinyrender::setCameraEye(glm::vec3(0.f, 0.f, -5.0f));

	tinyrender::addSphere(1.0f, 16);

	//for (int i = 0; i < 1; i++) {
	//	const float x = float(rand() % 50) - 25.0f;
	//	const float y = float(rand() % 50) - 25.0f;
	//	const float z = float(rand() % 50) - 25.0f;
	//	const float r = (float(rand()) / float(RAND_MAX)) * 1.5f + 0.5f;
	//	const int id = tinyrender::addSphere(r, 16);
	//	tinyrender::updateObject(id, glm::vec3(x, y, z), glm::vec3(0.0f), glm::vec3(1.0f));
	//}
	
	while (!tinyrender::shouldQuit()) {
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}
