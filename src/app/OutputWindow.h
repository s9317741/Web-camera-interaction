#ifndef WEBCAM_APP_OUTPUTWINDOW_H
#define WEBCAM_APP_OUTPUTWINDOW_H

struct GLFWwindow;

namespace app {

// A plain window with nothing but the picture in it, for OBS (or any screen
// capture) to pick up and pass on to a video call as a camera. No buttons,
// countdowns or hints ever show in it. It shares textures with the main
// window, so the picture is drawn without copying it back from the GPU.
class OutputWindow
{
public:
	OutputWindow() = default;
	~OutputWindow() { close(); }
	OutputWindow(const OutputWindow&) = delete;
	OutputWindow& operator=(const OutputWindow&) = delete;

	// `main` must be the current context; it is current again afterwards.
	bool open(GLFWwindow* main, const char* title);
	void close();
	bool isOpen() const { return window_ != nullptr; }
	bool closeRequested() const; // its own close button was pressed

	// Draws the texture (an RGB or RGBA texture of the main context) to fit,
	// in black bars, without waiting for the screen.
	void present(unsigned int texture, int width, int height);

private:
	GLFWwindow* window_ = nullptr;
	GLFWwindow* main_ = nullptr;
	unsigned int framebuffer_ = 0;

	// OpenGL 3.0 framebuffer calls, looked up when the window opens (Windows
	// headers stop at 1.1). Stored untyped, cast where they are called.
	using Proc = void (*)();
	Proc genFramebuffers_ = nullptr;
	Proc deleteFramebuffers_ = nullptr;
	Proc bindFramebuffer_ = nullptr;
	Proc framebufferTexture2D_ = nullptr;
	Proc blitFramebuffer_ = nullptr;
};

} // namespace app

#endif
