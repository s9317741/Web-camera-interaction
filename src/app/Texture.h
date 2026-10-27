#ifndef WEBCAM_APP_TEXTURE_H
#define WEBCAM_APP_TEXTURE_H

#include <imgui.h>
#include <opencv2/core.hpp>

namespace app {

// An OpenGL texture showing an 8-bit BGR or BGRA image in ImGui.
class Texture
{
public:
	Texture() = default;
	~Texture();
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;

	// 8-bit BGR or BGRA. Re-allocates only when the size or format changes.
	void upload(const cv::Mat& image);

	// Like a VCL TImage with Transparent set: pixels of the bottom-left
	// pixel's color become see-through.
	void uploadTransparent(const cv::Mat& bgr);

	bool valid() const { return id_ != 0 && width_ > 0; }
	int width() const { return width_; }
	int height() const { return height_; }
	ImTextureID id() const { return static_cast<ImTextureID>(id_); }

	// Largest size that fits the box without changing the aspect ratio.
	ImVec2 fit(ImVec2 box) const;

private:
	unsigned int id_ = 0;
	int width_ = 0;
	int height_ = 0;
	int channels_ = 0;
};

} // namespace app

#endif
