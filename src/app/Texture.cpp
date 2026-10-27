#include "Texture.h"

#include <GLFW/glfw3.h>

#include <algorithm>

// Windows ships OpenGL 1.1 headers; these are core since 1.2.
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

namespace app {

Texture::~Texture()
{
	if (id_ != 0)
		glDeleteTextures(1, &id_);
}

Texture::Texture(Texture&& other) noexcept
	: id_(other.id_), width_(other.width_), height_(other.height_), channels_(other.channels_)
{
	other.id_ = 0;
	other.width_ = other.height_ = other.channels_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
	if (this != &other)
	{
		if (id_ != 0)
			glDeleteTextures(1, &id_);
		id_ = other.id_;
		width_ = other.width_;
		height_ = other.height_;
		channels_ = other.channels_;
		other.id_ = 0;
		other.width_ = other.height_ = other.channels_ = 0;
	}
	return *this;
}

void Texture::upload(const cv::Mat& image)
{
	if (image.empty() || (image.type() != CV_8UC3 && image.type() != CV_8UC4))
		return;
	if (id_ == 0)
	{
		glGenTextures(1, &id_);
		glBindTexture(GL_TEXTURE_2D, id_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glBindTexture(GL_TEXTURE_2D, id_);

	// Rows may be padded; upload a tightly packed copy if so.
	const cv::Mat packed = image.isContinuous() ? image : image.clone();
	const int channels = packed.channels();
	const GLenum format = channels == 4 ? GL_BGRA : GL_BGR;
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	if (packed.cols != width_ || packed.rows != height_ || channels != channels_)
	{
		width_ = packed.cols;
		height_ = packed.rows;
		channels_ = channels;
		glTexImage2D(GL_TEXTURE_2D, 0, channels == 4 ? GL_RGBA : GL_RGB, width_, height_, 0, format,
			GL_UNSIGNED_BYTE, packed.data);
	}
	else
	{
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, format, GL_UNSIGNED_BYTE, packed.data);
	}
}

void Texture::uploadTransparent(const cv::Mat& bgr)
{
	if (bgr.empty() || bgr.type() != CV_8UC3)
		return;
	const cv::Vec3b key = bgr.at<cv::Vec3b>(bgr.rows - 1, 0);
	cv::Mat bgra(bgr.size(), CV_8UC4);
	for (int y = 0; y < bgr.rows; ++y)
		for (int x = 0; x < bgr.cols; ++x)
		{
			const cv::Vec3b c = bgr.at<cv::Vec3b>(y, x);
			bgra.at<cv::Vec4b>(y, x) = cv::Vec4b(c[0], c[1], c[2], c == key ? 0 : 255);
		}
	upload(bgra);
}

ImVec2 Texture::fit(ImVec2 box) const
{
	if (width_ <= 0 || height_ <= 0 || box.x <= 0 || box.y <= 0)
		return ImVec2(0, 0);
	const float scale = std::min(box.x / width_, box.y / height_);
	return ImVec2(width_ * scale, height_ * scale);
}

} // namespace app
