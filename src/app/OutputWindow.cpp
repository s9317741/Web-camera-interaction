#include "OutputWindow.h"

#include <GLFW/glfw3.h>

#include <algorithm>

#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif

// OpenGL's calling convention; only 32-bit Windows has one of its own.
#if defined(_WIN32) && !defined(_WIN64)
#define WEBCAM_GL_CALL __stdcall
#else
#define WEBCAM_GL_CALL
#endif

namespace app {

namespace {

using GenFramebuffers = void(WEBCAM_GL_CALL*)(GLsizei, GLuint*);
using DeleteFramebuffers = void(WEBCAM_GL_CALL*)(GLsizei, const GLuint*);
using BindFramebuffer = void(WEBCAM_GL_CALL*)(GLenum, GLuint);
using FramebufferTexture2D = void(WEBCAM_GL_CALL*)(GLenum, GLenum, GLenum, GLuint, GLint);
using BlitFramebuffer = void(WEBCAM_GL_CALL*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

} // namespace

bool OutputWindow::open(GLFWwindow* main, const char* title)
{
	if (window_ != nullptr)
		return true;
	main_ = main;
	glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
	window_ = glfwCreateWindow(1280, 720, title, nullptr, main); // shares the main window's textures
	if (window_ == nullptr)
		return false;
	glfwMakeContextCurrent(window_);
	glfwSwapInterval(0); // the main window already waits for the screen; waiting twice halves the rate
	genFramebuffers_ = glfwGetProcAddress("glGenFramebuffers");
	deleteFramebuffers_ = glfwGetProcAddress("glDeleteFramebuffers");
	bindFramebuffer_ = glfwGetProcAddress("glBindFramebuffer");
	framebufferTexture2D_ = glfwGetProcAddress("glFramebufferTexture2D");
	blitFramebuffer_ = glfwGetProcAddress("glBlitFramebuffer");
	glfwMakeContextCurrent(main_);
	if (genFramebuffers_ == nullptr || deleteFramebuffers_ == nullptr || bindFramebuffer_ == nullptr ||
		framebufferTexture2D_ == nullptr || blitFramebuffer_ == nullptr)
	{
		close();
		return false;
	}
	return true;
}

void OutputWindow::close()
{
	if (window_ == nullptr)
		return;
	if (framebuffer_ != 0)
	{
		// Framebuffers belong to the context that made them.
		glfwMakeContextCurrent(window_);
		reinterpret_cast<DeleteFramebuffers>(deleteFramebuffers_)(1, &framebuffer_);
		framebuffer_ = 0;
		glfwMakeContextCurrent(main_);
	}
	glfwDestroyWindow(window_);
	window_ = nullptr;
}

bool OutputWindow::closeRequested() const
{
	return window_ != nullptr && glfwWindowShouldClose(window_) != 0;
}

void OutputWindow::present(unsigned int texture, int width, int height)
{
	if (window_ == nullptr)
		return;
	glfwMakeContextCurrent(window_);
	int fbWidth = 0, fbHeight = 0;
	glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
	glViewport(0, 0, fbWidth, fbHeight);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	if (texture != 0 && width > 0 && height > 0 && fbWidth > 0 && fbHeight > 0)
	{
		if (framebuffer_ == 0)
			reinterpret_cast<GenFramebuffers>(genFramebuffers_)(1, &framebuffer_);
		const auto bind = reinterpret_cast<BindFramebuffer>(bindFramebuffer_);
		bind(GL_READ_FRAMEBUFFER, framebuffer_);
		reinterpret_cast<FramebufferTexture2D>(framebufferTexture2D_)(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, texture, 0);
		const float scale = std::min(static_cast<float>(fbWidth) / width, static_cast<float>(fbHeight) / height);
		const int w = static_cast<int>(width * scale), h = static_cast<int>(height * scale);
		const int x = (fbWidth - w) / 2, y = (fbHeight - h) / 2;
		// The texture's first row is the picture's top, so flip it on the way.
		reinterpret_cast<BlitFramebuffer>(blitFramebuffer_)(0, 0, width, height, x, y + h, x + w, y,
			GL_COLOR_BUFFER_BIT, GL_LINEAR);
		bind(GL_READ_FRAMEBUFFER, 0);
	}
	glfwSwapBuffers(window_);
	glfwMakeContextCurrent(main_);
}

} // namespace app
