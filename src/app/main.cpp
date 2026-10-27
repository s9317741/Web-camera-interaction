#include "App.h"
#include "Assets.h"
#include "Platform.h"
#include "Stickers.h"
#include "Style.h"
#include "Vision.h"

#include "core/EffectChain.h"
#include "core/Effects.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <memory>

namespace {

const char* const kWindowTitle = "視訊互動拍照亭";

void glfwError(int code, const char* description)
{
	std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
}

const char* firstExisting(std::initializer_list<const char*> paths)
{
	for (const char* path : paths)
	{
		std::error_code ec;
		if (std::filesystem::exists(path, ec))
			return path;
	}
	return nullptr;
}

// The UI is in Traditional Chinese, so the main font must have CJK glyphs.
// Without one (e.g. Linux without Noto CJK) Chinese shows as '?'.
app::UiFonts loadFonts()
{
	const char* regular = firstExisting({
#if defined(_WIN32)
		"C:/Windows/Fonts/msjh.ttc", "C:/Windows/Fonts/msyh.ttc",
#elif defined(__APPLE__)
		"/System/Library/Fonts/PingFang.ttc", "/System/Library/Fonts/STHeiti Medium.ttc",
		"/System/Library/Fonts/Hiragino Sans GB.ttc",
#else
		"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
#endif
	});
	const char* bold = firstExisting({
#if defined(_WIN32)
		"C:/Windows/Fonts/msjhbd.ttc", "C:/Windows/Fonts/msyhbd.ttc",
#elif defined(__APPLE__)
		"/System/Library/Fonts/STHeiti Medium.ttc",
#else
		"/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
		"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc",
#endif
	});
	ImGuiIO& io = ImGui::GetIO();
	app::UiFonts fonts;
	fonts.text = regular ? io.Fonts->AddFontFromFileTTF(regular, 17.0f) : nullptr;
	if (fonts.text == nullptr)
		fonts.text = io.Fonts->AddFontDefault();
	fonts.bold = bold ? io.Fonts->AddFontFromFileTTF(bold, 17.0f) : nullptr;
	if (fonts.bold == nullptr)
		fonts.bold = fonts.text;
	io.FontDefault = fonts.text;
	return fonts;
}

// Dark, rounded, with the orange of the camera icon as the accent.
void applyStyle(float scale)
{
	ImGuiStyle& style = ImGui::GetStyle();
	style = ImGuiStyle();
	style.WindowRounding = 8.0f;
	style.ChildRounding = 8.0f;
	style.FrameRounding = 6.0f;
	style.PopupRounding = 6.0f;
	style.GrabRounding = 6.0f;
	style.WindowPadding = ImVec2(14.0f, 10.0f);
	style.FramePadding = ImVec2(12.0f, 6.0f);
	style.ItemSpacing = ImVec2(10.0f, 8.0f);
	style.WindowBorderSize = 0.0f;
	style.ScaleAllSizes(scale);
	style.FontScaleDpi = scale;
	style.FontSizeBase = 17.0f;

	ImVec4* c = style.Colors;
	const ImVec4 accent(0.96f, 0.58f, 0.11f, 1.0f);
	c[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.93f, 1.0f);
	c[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.56f, 0.58f, 1.0f);
	c[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
	c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.15f, 0.17f, 0.98f);
	c[ImGuiCol_Border] = ImVec4(0.28f, 0.29f, 0.31f, 1.0f);
	c[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.19f, 0.21f, 1.0f);
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.23f, 0.24f, 0.27f, 1.0f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.27f, 0.28f, 0.31f, 1.0f);
	c[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.17f, 1.0f);
	c[ImGuiCol_TitleBgActive] = ImVec4(0.19f, 0.19f, 0.22f, 1.0f);
	c[ImGuiCol_Button] = ImVec4(0.22f, 0.23f, 0.26f, 1.0f);
	c[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.31f, 0.35f, 1.0f);
	c[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.37f, 0.42f, 1.0f);
	c[ImGuiCol_Header] = ImVec4(0.22f, 0.23f, 0.26f, 1.0f);
	c[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.31f, 0.35f, 1.0f);
	c[ImGuiCol_HeaderActive] = ImVec4(0.36f, 0.37f, 0.42f, 1.0f);
	c[ImGuiCol_CheckMark] = accent;
	c[ImGuiCol_SliderGrab] = accent;
	c[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.68f, 0.25f, 1.0f);
	c[ImGuiCol_Separator] = c[ImGuiCol_Border];
	c[ImGuiCol_TextSelectedBg] = ImVec4(0.96f, 0.58f, 0.11f, 0.35f);
	c[ImGuiCol_NavCursor] = accent;
}

// A still "room" and the same room with someone in it, for --screenshot.
cv::Mat stagedRoom(const app::Assets& assets)
{
	cv::Mat room;
	cv::resize(assets.bgr("Frame2.bmp"), room, cv::Size(1280, 720));
	return room;
}

void drawStagedPerson(cv::Mat& image, const cv::Scalar& skin, const cv::Scalar& shirt)
{
	cv::ellipse(image, cv::Point(560, 250), cv::Size(70, 90), 0, 0, 360, skin, cv::FILLED, cv::LINE_AA);
	cv::rectangle(image, cv::Rect(450, 340, 220, 380), shirt, cv::FILLED);
}

cv::Mat stagedPerson(const app::Assets& assets)
{
	cv::Mat person = stagedRoom(assets);
	drawStagedPerson(person, cv::Scalar(120, 160, 225), cv::Scalar(160, 90, 40));
	return person;
}

// Where the staged person is, as the AI would report it.
cv::Mat stagedMask()
{
	cv::Mat mask(720, 1280, CV_8U, cv::Scalar(0));
	drawStagedPerson(mask, cv::Scalar(255), cv::Scalar(255));
	cv::GaussianBlur(mask, mask, cv::Size(7, 7), 0);
	return mask;
}

template <typename F>
double timeMs(F f, int runs = 10)
{
	f(); // warm up: first calls allocate buffers and compile OpenCL kernels
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < runs; ++i)
		f();
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / runs;
}

// The OpenCV parts of the app: AI models, looks, frames, stickers, collage.
int selfTestOpenCv(const app::Assets& assets)
{
	app::PersonSegmenter segmenter;
	app::FaceTracker faces;
	std::string error;
	const bool haveSegmenter = segmenter.load(assets.root() / "models" / "human_segmentation_pphumanseg_2023mar.onnx", error);
	const bool haveFaces = faces.load(assets.root() / "models" / "face_detection_yunet_2023mar.onnx", error);
	std::cout << (haveSegmenter ? "ok   person segmentation model" : "skip person segmentation model (not found)") << std::endl;
	std::cout << (haveFaces ? "ok   face detection model" : "skip face detection model (not found)") << std::endl;

	const int sizes[3][2] = { { 640, 480 }, { 1280, 720 }, { 1920, 1080 } };
	app::FaceStickers stickers;
	for (const auto& size : sizes)
	{
		cv::Mat picture;
		cv::resize(assets.bgr("Frame2.bmp"), picture, cv::Size(size[0], size[1]));
		std::printf("     %dx%d:", size[0], size[1]);
		if (haveSegmenter)
		{
			cv::Mat alpha;
			const double ms = timeMs([&] { segmenter.segment(picture, alpha); });
			if (alpha.size() != picture.size() || alpha.type() != CV_8U)
			{
				std::cerr << std::endl << "FAIL segmentation mask has the wrong size or type" << std::endl;
				return 1;
			}
			std::printf(" segment %.1f ms,", ms);
			app::Styler styler;
			cv::Mat work = picture.clone();
			const cv::Mat background = assets.bgr("Frame3.bmp");
			std::printf(" replace %.1f ms,", timeMs([&] { styler.replaceBackground(work, alpha, background); }));
			std::printf(" blur %.1f ms,", timeMs([&] { styler.blurBackground(work, alpha); }));
		}
		if (haveFaces)
			std::printf(" faces %.1f ms,", timeMs([&] { faces.detect(picture); }));
		app::Styler styler;
		const char* names[] = { "beauty", "cartoon", "sketch", "vintage" };
		const app::Look looks[] = { app::Look::Beauty, app::Look::Cartoon, app::Look::Sketch, app::Look::Vintage };
		const bool gpu = !app::Styler::gpuName().empty();
		for (int i = 0; i < 4; ++i)
		{
			cv::Mat work;
			styler.setUseGpu(false);
			std::printf(" %s %.1f ms", names[i], timeMs([&] { work = picture.clone(); styler.applyLook(work, looks[i]); }, 5));
			if (gpu && i < 2) // the looks with a GPU path
			{
				styler.setUseGpu(true);
				cv::Mat onGpu;
				std::printf(" (GPU %.1f ms)", timeMs([&] { onGpu = picture.clone(); styler.applyLook(onGpu, looks[i]); }, 5));
				if (onGpu.size() != work.size() || onGpu.type() != work.type())
				{
					std::cerr << std::endl << "FAIL look " << names[i] << " on the GPU changed the picture's size or type" << std::endl;
					return 1;
				}
			}
			std::printf(",");
			if (work.size() != picture.size() || work.type() != CV_8UC3)
			{
				std::cerr << std::endl << "FAIL look " << names[i] << " changed the picture's size or type" << std::endl;
				return 1;
			}
		}
		app::Adjustments adjust;
		adjust.brightness = 20;
		adjust.contrast = 1.2f;
		adjust.saturation = 1.4f;
		adjust.vignette = true;
		cv::Mat work;
		std::printf(" adjust %.1f ms", timeMs([&] { work = picture.clone(); styler.applyAdjustments(work, adjust); }, 5));
		std::printf("%s", "\n");
		for (int frame = 0; frame < 4; ++frame)
		{
			cv::Mat framed = picture.clone();
			styler.applyFrame(framed, static_cast<app::FrameStyle>(frame), true);
			if (framed.size() != picture.size())
			{
				std::cerr << "FAIL frame " << frame << " changed the picture's size" << std::endl;
				return 1;
			}
		}
		app::Face face;
		face.eyeLeft = cv::Point2f(size[0] * 0.45f, size[1] * 0.4f);
		face.eyeRight = cv::Point2f(size[0] * 0.55f, size[1] * 0.42f);
		face.nose = cv::Point2f(size[0] * 0.5f, size[1] * 0.48f);
		face.mouth = cv::Point2f(size[0] * 0.5f, size[1] * 0.55f);
		for (int kind = 0; kind < app::FaceStickers::kCount; ++kind)
			stickers.draw(work, { face }, static_cast<app::FaceStickers::Kind>(kind));
	}
	const cv::Mat frame1 = assets.bgr("Frame1.bmp");
	if (app::makeCollage({ frame1, frame1, frame1, frame1 }, true).empty())
	{
		std::cerr << "FAIL four-shot collage" << std::endl;
		return 1;
	}
	std::cout << "ok   looks, frames, face stickers, four-shot collage" << std::endl;
	return 0;
}

// Runs every effect on a synthetic scene at each supported size, without a
// window or a camera. Checks that the assets load, OpenCV's YUY2 conversion
// agrees with ours, and the pipeline is fast enough.
int selfTest(const app::Assets& assets)
{
	using namespace webcam;
	const Rgb white = { 255, 255, 255 };
	const Rgb red = { 255, 0, 0 };
	const char* names[] = { "SpottedGecko.bmp", "Camera.bmp", "Frame1.bmp", "Frame2.bmp", "Frame3.bmp",
		"DragonBall1.bmp", "DragonBall7.bmp" };
	for (const char* name : names)
	{
		const cv::Mat image = assets.bgr(name);
		if (image.empty())
		{
			std::cerr << "FAIL cannot load " << name << " from " << app::pathToUtf8(assets.root()) << "\n";
			return 1;
		}
		std::cout << "ok   " << name << " " << image.cols << "x" << image.rows << "\n";
	}

	const int sizes[3][2] = { { 640, 480 }, { 1280, 720 }, { 1920, 1080 } };
	for (const auto& size : sizes)
	{
		const int w = size[0], h = size[1];
		cv::Mat room(h, w, CV_8UC3, cv::Scalar(140, 150, 160));
		cv::Mat person = room.clone();
		cv::rectangle(person, cv::Rect(w / 3, h / 4, w / 5, h * 3 / 4), cv::Scalar(90, 120, 200), cv::FILLED);
		cv::Mat hand = person.clone(); // touches the camera button, bottom-right on screen
		cv::rectangle(hand, cv::Rect(40, h - 90, 70, 70), cv::Scalar(90, 120, 200), cv::FILLED);

		std::vector<std::uint8_t> bytes(Yuy2Frame::bytesFor(w, h));
		Yuy2Frame frame;
		frame.wrap(bytes.data(), bytes.size(), w, h);

		// The app converts with OpenCV; it must agree with the core's conversion
		// both ways, or learned backgrounds and effects would drift.
		cv::Mat noise(h, w, CV_8UC3);
		cv::randu(noise, 0, 256);
		cv::Mat yuy2(h, w, CV_8UC2, bytes.data());
		frame.copyFromBgr24(noise.data, noise.step);
		cv::Mat ourYuy2 = yuy2.clone(), theirYuy2;
		cv::cvtColor(noise, theirYuy2, cv::COLOR_BGR2YUV_YUY2);
		cv::Mat ourBgr(h, w, CV_8UC3), theirBgr;
		frame.copyToBgr24(ourBgr.data, ourBgr.step);
		cv::cvtColor(yuy2, theirBgr, cv::COLOR_YUV2BGR_YUY2);
		double worstIn = 0, worstOut = 0;
		cv::Mat diff;
		cv::absdiff(ourYuy2, theirYuy2, diff);
		cv::minMaxLoc(diff.reshape(1), nullptr, &worstIn);
		cv::absdiff(ourBgr, theirBgr, diff);
		cv::minMaxLoc(diff.reshape(1), nullptr, &worstOut);
		if (worstIn > 2 || worstOut > 2)
		{
			std::cerr << "FAIL OpenCV YUY2 conversion differs from the core by " << worstIn
				<< " (to YUY2) / " << worstOut << " (to BGR)\n";
			return 1;
		}

		EffectChain chain;
		chain.setMirror(true);
		chain.startBackgroundLearning(1, std::function<void()>());
		cv::cvtColor(room, yuy2, cv::COLOR_BGR2YUV_YUY2);
		chain.process(frame);

		std::shared_ptr<const Image> background = app::imageFromBgr(assets.bgr("Frame3.bmp"));
		chain.add(std::make_shared<BackgroundReplaceEffect>(background));
		chain.add(std::make_shared<SpriteEffect>(std::make_shared<Sprite>(assets.keyed("SpottedGecko.bmp", white), 50, 50)));
		chain.add(std::make_shared<SpriteEffect>(std::make_shared<Sprite>(assets.keyed("DragonBall4.bmp", red), 210, 80)));
		std::shared_ptr<Sprite> button = std::make_shared<Sprite>(assets.keyed("Camera.bmp", white));
		button->moveTo(w - button->width() - 15, h - button->height() - 5);
		int shots = 0;
		chain.add(std::make_shared<TouchTriggerEffect>(button,
			[&shots](const Yuy2Frame&, const Yuy2Frame&) { ++shots; }));

		const int runs = 20;
		double total = 0;
		for (int i = 0; i < runs; ++i)
		{
			const auto start = std::chrono::steady_clock::now();
			cv::cvtColor(person, yuy2, cv::COLOR_BGR2YUV_YUY2);
			chain.process(frame);
			cv::Mat shown;
			cv::cvtColor(cv::Mat(h, w, CV_8UC2, bytes.data()), shown, cv::COLOR_YUV2BGR_YUY2);
			total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		}
		if (shots != 0)
		{
			std::cerr << "FAIL picture taken without a touch at " << w << "x" << h << "\n";
			return 1;
		}
		cv::cvtColor(hand, yuy2, cv::COLOR_BGR2YUV_YUY2);
		chain.process(frame);
		if (shots != 1)
		{
			std::cerr << "FAIL touching the camera button took " << shots << " pictures at " << w << "x" << h << "\n";
			return 1;
		}
		std::vector<unsigned char> png;
		cv::Mat shown;
		cv::cvtColor(cv::Mat(h, w, CV_8UC2, bytes.data()), shown, cv::COLOR_YUV2BGR_YUY2);
		if (!cv::imencode(".png", shown, png))
		{
			std::cerr << "FAIL cannot encode PNG\n";
			return 1;
		}
		std::printf("ok   %dx%d: %.1f ms per frame (convert, mirror, detect, cut out, sprites, convert back)\n",
			w, h, total / runs);
	}
	if (selfTestOpenCv(assets) != 0)
		return 1;
	std::cout << "self-test passed\n";
	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	const std::filesystem::path assetDir = app::Assets::locate();
	if (assetDir.empty())
	{
		std::cerr << "Cannot find the assets folder (assets/images) next to the program.\n";
		return 1;
	}
	const app::Assets assets(assetDir);

	if (argc > 1 && std::strcmp(argv[1], "--self-test") == 0)
		return selfTest(assets);

	// --screenshot <file.png> [scene]: draws the window hidden, saves it, and
	// exits. A still picture stands in for the camera (see App::stageScreenshot).
	const bool screenshot = argc > 2 && std::strcmp(argv[1], "--screenshot") == 0;
	const std::filesystem::path screenshotFile = screenshot ? app::pathFromUtf8(argv[2]) : std::filesystem::path();
	const int screenshotTab = argc > 3 ? std::atoi(argv[3]) : 0;

	std::srand(static_cast<unsigned>(std::time(nullptr)));
	glfwSetErrorCallback(glfwError);
	if (!glfwInit())
		return 1;

	if (screenshot)
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#if defined(__APPLE__)
	const char* glslVersion = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
	const char* glslVersion = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

	const float scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
	GLFWwindow* window = glfwCreateWindow(static_cast<int>(1280 * scale), static_cast<int>(800 * scale),
		kWindowTitle, nullptr, nullptr);
	if (window == nullptr)
	{
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr; // the layout is fixed; nothing to remember
	applyStyle(scale);
	const app::UiFonts fonts = loadFonts();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glslVersion);

	{
		// Owns textures, so it must go before the GL context does.
		app::App application(assets, fonts, !screenshot);
		if (screenshot)
			application.stageScreenshot(screenshotTab, stagedRoom(assets), stagedPerson(assets), stagedMask());

		// Files dropped on the window, as UTF-8 paths.
		glfwSetWindowUserPointer(window, &application);
		glfwSetDropCallback(window, [](GLFWwindow* w, int count, const char** paths) {
			std::vector<std::filesystem::path> files;
			for (int i = 0; i < count; ++i)
				files.push_back(app::pathFromUtf8(paths[i]));
			static_cast<app::App*>(glfwGetWindowUserPointer(w))->onDrop(files);
		});

		// Full screen on the monitor the window is on; back to the old place after.
		int windowed[4] = { 0, 0, 0, 0 };
		application.setFullscreenHandler([window, &windowed](bool on) {
			if (!on)
			{
				glfwSetWindowMonitor(window, nullptr, windowed[0], windowed[1], windowed[2], windowed[3], 0);
				return;
			}
			glfwGetWindowPos(window, &windowed[0], &windowed[1]);
			glfwGetWindowSize(window, &windowed[2], &windowed[3]);
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			int count = 0;
			GLFWmonitor** monitors = glfwGetMonitors(&count);
			const int cx = windowed[0] + windowed[2] / 2, cy = windowed[1] + windowed[3] / 2;
			for (int i = 0; i < count; ++i)
			{
				int mx = 0, my = 0;
				glfwGetMonitorPos(monitors[i], &mx, &my);
				const GLFWvidmode* m = glfwGetVideoMode(monitors[i]);
				if (cx >= mx && cx < mx + m->width && cy >= my && cy < my + m->height)
					monitor = monitors[i];
			}
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		});

		int frame = 0;
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
			{
				ImGui_ImplGlfw_Sleep(10);
				continue;
			}
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			application.update();
			ImGui::Render();

			int width = 0, height = 0;
			glfwGetFramebufferSize(window, &width, &height);
			glViewport(0, 0, width, height);
			glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			if (screenshot && ++frame == 5) // a few frames for fonts and layout to settle
			{
				cv::Mat rgb(height, width, CV_8UC3);
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				glReadBuffer(GL_BACK);
				glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
				cv::Mat bgr;
				cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
				cv::flip(bgr, bgr, 0);
				std::vector<unsigned char> png;
				const bool ok = cv::imencode(".png", bgr, png) && app::writeFile(screenshotFile, png);
				std::cout << (ok ? "saved " : "could not save ") << app::pathToUtf8(screenshotFile) << std::endl;
				break;
			}
			glfwSwapBuffers(window);
		}
		glfwSetDropCallback(window, nullptr);
		glfwSetWindowUserPointer(window, nullptr);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
