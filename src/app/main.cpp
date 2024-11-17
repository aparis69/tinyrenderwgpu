#include <tinyrender.h>

int main(int /*argc*/, const char** /*argv*/) {
	tinyrender::init("tinyrenderwgpu", 640, 480);
	tinyrender::addSphere(1.0f, 16);
	while (!tinyrender::shouldQuit()) {
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}
