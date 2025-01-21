#include "../nclgl/window.h"
#include "./src/Renderer.h"

int main()	{
	Window w("Project Mars!", 1280, 720, true);

	if(!w.HasInitialised()) {
		return -1;
	}
	
	Renderer renderer(w);
	if(!renderer.HasInitialised()) {
		return -1;
	}

	w.LockMouseToWindow(true);
	w.ShowOSPointer(false);

	while(w.UpdateWindow() && !Window::GetKeyboard()->KeyDown(KEYBOARD_ESCAPE)){
		renderer.UpdateScene(w.GetTimer()->GetTimeDeltaSeconds());

		if (Window::GetKeyboard()->KeyDown(KEYBOARD_F5)) {
			Shader::ReloadAllShaders();
		}
		if (Window::GetKeyboard()->KeyTriggered(KEYBOARD_T)) {
			renderer.StartTransition();
		}
		if (Window::GetKeyboard()->KeyTriggered(KEYBOARD_F)) {
			renderer.ToggleTrackCamera();
		}
		if (Window::GetKeyboard()->KeyTriggered(KEYBOARD_V)) {
			renderer.ToggleSplitScreen();
		}
		if (renderer.IsSplitScreen()) {
			renderer.RenderSplitScreen();
		}
		if (Window::GetKeyboard()->KeyTriggered(KEYBOARD_H)) {
			renderer.ToggleHDR();
		}
		if (Window::GetKeyboard()->KeyTriggered(KEYBOARD_B)) {
			renderer.ToggleBloom();
		}
		if (Window::GetKeyboard()->KeyDown(KEYBOARD_LEFT)) {
			renderer.AdjustExposure(w.GetTimer()->GetTimeDeltaSeconds(), false);
		}
		if (Window::GetKeyboard()->KeyDown(KEYBOARD_RIGHT)) {
			renderer.AdjustExposure(w.GetTimer()->GetTimeDeltaSeconds(), true);
		}
		else {
			renderer.RenderScene();
		}

		renderer.SwapBuffers();
	}
	return 0;
}