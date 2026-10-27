#ifndef WEBCAM_APP_APP_H
#define WEBCAM_APP_APP_H

#include "Assets.h"
#include "Camera.h"
#include "Music.h"
#include "Settings.h"
#include "Sound.h"
#include "Stickers.h"
#include "Style.h"
#include "Texture.h"
#include "Vision.h"

#include "core/EffectChain.h"
#include "core/Effects.h"

#include <imgui.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace app {

struct UiFonts
{
	ImFont* text = nullptr; // with CJK glyphs when the system has them
	ImFont* bold = nullptr;
};

// An interactive photo booth. Every camera frame goes through:
//   mirror -> AI person mask -> new or blurred background -> core effects
//   (filters, game, wave-to-shoot) -> looks -> face and placed stickers -> frame
// and the UI around it: a short guide, the big picture, the ways to play
// along the bottom, recent photos, and everything technical in "Advanced".
class App
{
public:
	// `persistent` false (screenshots, tests) neither reads nor writes settings.
	App(const Assets& assets, UiFonts fonts, bool persistent);
	~App();
	App(const App&) = delete;
	App& operator=(const App&) = delete;

	void update();

	// Image files dropped on the window: transparent PNGs become stickers,
	// other pictures become backgrounds. Copies are kept for next time.
	void onDrop(const std::vector<std::filesystem::path>& files);

	// How main switches the window to and from full screen.
	void setFullscreenHandler(std::function<void(bool)> handler) { setFullscreen_ = std::move(handler); }

	// For --screenshot: a still picture and a known person shape instead of
	// a camera and the AI. Scenes: 0 start, 1 playing, 2 Advanced, 3 stickers,
	// 4 filters, 5 game, 6 game over, 7 four-shot countdown.
	void stageScreenshot(int scene, const cv::Mat& room, const cv::Mat& person, const cv::Mat& personMask);

private:
	using Clock = std::chrono::steady_clock;

	enum class Drawer { None, Background, Stickers, Filters, Game };
	enum class BackgroundMode { Off, Replace, Blur };
	enum class CutOut { Ai, Difference, GreenScreen };
	enum class GameState { Idle, Playing, Over };

	struct Picture
	{
		std::string name;
		cv::Mat bgr;
		Texture thumbnail;
		std::shared_ptr<const webcam::Image> image; // for the non-AI cut-outs
	};

	// A sticker placed on the picture. Position and size are fractions of
	// the frame, so they survive a change of resolution.
	struct Placed
	{
		std::string name;
		cv::Mat bgra;
		Texture thumbnail;
		bool on = false;
		cv::Point2f center;
		float width = 0.1f;
	};

	struct Gecko
	{
		std::shared_ptr<webcam::Sprite> sprite;
		std::shared_ptr<webcam::TouchToHideEffect> effect;
		cv::Point2f velocity;
		cv::Point2f position;
		bool wasVisible = true;
		bool respawnPending = false;
		Clock::time_point respawnAt;
	};

	struct Photo
	{
		std::filesystem::path file;
		Texture thumbnail;
	};

	// Frames
	void processFrame(const cv::Mat& frame);
	void syncEffects();
	void runTimers();
	void moveGeckos(double seconds);
	bool aiTouch() const;
	bool needsLearnedBackground() const;
	bool backgroundReady() const;

	// Actions
	void openCamera();
	void closeCamera();
	void startLearning();
	void startCountdown(int seconds);
	void startFourShot();
	void startGame();
	void respawnGecko(Gecko& gecko);
	bool onPerson(const cv::Rect& area) const;
	void takePicture(const cv::Mat& bgr);
	void toggleRecording();
	void toggleFullscreen();
	void resetEffects();
	void copyToClipboard(const cv::Mat& bgr);
	void toast(const std::string& text);
	void log(const std::string& line);
	void loadUserPictures();
	bool addBackground(const std::string& name, const cv::Mat& bgr);
	bool addSticker(const std::string& name, const cv::Mat& bgra);
	void refreshBoxSticker();
	void loadSettings();
	void saveSettings();

	// UI
	void drawTopBar();
	void drawVideo(ImVec2 size);
	void handleVideoMouse(ImVec2 corner, ImVec2 fitted);
	void drawOverlay(ImVec2 min, ImVec2 max);
	void drawDrawer();
	void drawBottomBar();
	void drawPhotoStrip();
	void drawAdvanced();
	bool bigToggle(const char* label, bool active, bool marked, ImVec2 size);
	bool thumbnailButton(const char* id, const Texture& texture, float height, bool selected);

	UiFonts fonts_;
	float scale_ = 1.0f;
	bool persistent_;
	std::filesystem::path home_;
	Settings settings_;
	std::function<void(bool)> setFullscreen_;
	bool fullscreen_ = false;

	// Camera
	Camera camera_;
	Camera::Request request_;
	Camera::State lastCameraState_ = Camera::State::Closed;
	bool cameraStarted_ = false;
	int sizeIndex_ = 1;
	int backendIndex_ = 0;
	double previewInterval_ = 0.0;
	Clock::time_point lastProcessed_;
	Clock::time_point lastUpdate_;
	bool frozen_ = false;
	cv::Mat captured_;
	std::uint64_t seenFrame_ = 0;
	cv::Mat mirrored_;     // camera frame after mirroring, before anything else
	std::vector<std::uint8_t> yuy2_;
	cv::Mat shown_;
	Texture video_;
	int frameWidth_ = 0;
	int frameHeight_ = 0;
	std::uint64_t framesProcessed_ = 0;
	double processingMs_ = 0.0;
	double aiMs_ = 0.0;
	int fpsIndex_ = 0;            // camera frame rate to ask for: default, 30, 60, 120
	int processedInWindow_ = 0;
	Clock::time_point fpsWindow_;
	double processingFps_ = 0.0;

	// Screenshot staging
	bool staged_ = false;
	cv::Mat stagedFrame_;
	cv::Mat stagedMask_;
	std::vector<Face> stagedFaces_;

	// AI
	AiWorker ai_;
	std::uint64_t aiSeen_ = 0;   // last AiWorker result used
	cv::Mat personAlpha_;
	std::deque<cv::Mat> maskHistory_; // recent person masks, for "something moved here"
	std::vector<Face> faces_;

	// Pipeline and what the user switched on
	webcam::EffectChain effects_;
	std::shared_ptr<webcam::YuvRange> colorRange_;
	Styler styler_;
	bool mirror_ = true;
	Drawer drawer_ = Drawer::None;

	BackgroundMode backgroundMode_ = BackgroundMode::Off;
	CutOut cutOut_ = CutOut::Ai;
	int backgroundIndex_ = 0;
	std::vector<Picture> backgrounds_;
	std::shared_ptr<webcam::BackgroundReplaceEffect> cutOutByDifference_;
	std::shared_ptr<webcam::ChromaKeyEffect> cutOutByColor_;

	FaceStickers faceArt_;
	int faceSticker_ = -1; // FaceStickers::Kind, or -1
	Texture faceThumbs_[FaceStickers::kCount];
	std::vector<Placed> placed_;  // gecko, dragon ball, color box, then dropped ones
	cv::Mat dragonBalls_[7];      // BGRA
	int dragonBallIndex_ = 3;
	float boxColor_[3] = { 0.0f, 1.0f, 0.0f };
	int dragging_ = -1;
	cv::Point2f dragOffset_;

	Look look_ = Look::None;
	Adjustments adjust_;
	FrameStyle frameStyle_ = FrameStyle::None;
	bool dateStamp_ = false;
	bool gray_ = false;
	std::shared_ptr<webcam::TintEffect> grayEffect_;
	bool tint_ = false;
	float tintColor_[3] = { 0.25f, 0.45f, 1.0f };
	std::shared_ptr<webcam::TintEffect> tintEffect_;
	bool skinMark_ = false;
	std::shared_ptr<webcam::ColorHighlightEffect> skinEffect_;
	bool colorMark_ = false;
	std::shared_ptr<webcam::ColorHighlightEffect> colorMarkEffect_;

	// Game
	GameState game_ = GameState::Idle;
	int difficulty_ = 1;
	int score_ = 0;
	int best_[3] = { 0, 0, 0 };
	bool newBest_ = false;
	Clock::time_point gameEnds_;
	bool showBlocks_ = false;
	std::shared_ptr<const webcam::Image> geckoImage_;
	std::vector<Gecko> geckos_;

	// Taking pictures
	bool wavePhoto_ = true;
	std::shared_ptr<webcam::Sprite> cameraButton_;
	std::shared_ptr<webcam::TouchTriggerEffect> photoTrigger_;
	bool waveShot_ = false;
	bool countdown_ = false;
	Clock::time_point shootAt_;
	int lastBeep_ = 0;
	bool shootNextFrame_ = false;
	int fourShot_ = -1;             // shots taken so far in a four-shot run, or -1 when idle
	std::vector<cv::Mat> fourShots_;
	bool saveCameraCopy_ = false;
	Clock::time_point flashAt_;
	std::filesystem::path outputDir_;
	std::deque<Photo> photos_;

	// Debug views and detection
	int debugView_ = 0;
	std::shared_ptr<webcam::DifferenceViewEffect> difference_;
	int threshold_ = webcam::ChangeDetector::kDefaultThreshold;
	float adaptation_ = 0.005f;
	bool learnPending_ = false;
	Clock::time_point learnAt_;
	bool backgroundJustLearned_ = false;
	Texture backgroundPreview_;

	// Recording
	cv::VideoWriter writer_;
	bool recording_ = false;
	std::string recordingFile_;
	std::uint64_t recordedFrames_ = 0;
	Clock::time_point recordingStart_;
	int codecIndex_ = 0;

	// Sound and music
	Sound sound_;
	Music music_;
	char musicPath_[1024] = {};
	float volume_ = 0.8f;
	bool loop_ = true;

	// Messages and windows
	std::string toast_;
	Clock::time_point toastAt_;
	std::deque<std::string> log_;
	bool advanced_ = false;
	float drawerHeight_ = 0.0f;
	float bottomHeight_ = 0.0f;
};

} // namespace app

#endif
