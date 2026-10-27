#include "App.h"

#include "Platform.h"

#include <imgui.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace app {

namespace {

const webcam::Rgb kWhite = { 255, 255, 255 };
const webcam::Rgb kGreen = { 0, 255, 0 };
const webcam::Rgb kSkinTint = { 95, 40, 20 };

const int kBackgroundFrames = 30;
const int kLearnDelaySeconds = 3;
const int kShootDelaySeconds = 3;
const int kFourShotGapSeconds = 2;
const int kRearmQuietFrames = 45; // the camera icon returns after ~1.5 s with nothing on it
const int kMotionFrames = 8;      // "moved here" compares with the mask this many frames ago (~0.25 s)
const double kGameSeconds = 30.0;
const double kRespawnSeconds = 0.6;
const double kToastSeconds = 2.5;
const double kFlashSeconds = 0.35;
const std::size_t kMaxPhotos = 12;

const float kTextSize = 17.0f;
const float kTitleSize = 22.0f;
const float kHugeSize = 110.0f;

const ImVec4 kAccent(0.96f, 0.58f, 0.11f, 1.0f);
const ImVec4 kAccentHover(1.00f, 0.68f, 0.25f, 1.0f);
const ImVec4 kDone(0.30f, 0.75f, 0.40f, 1.0f);
const ImVec4 kWarn(1.0f, 0.8f, 0.4f, 1.0f);

struct VideoSize
{
	const char* label;
	int width;
	int height;
};

const VideoSize kVideoSizes[] = {
	{ "640 x 480", 640, 480 },
	{ "1280 x 720", 1280, 720 },
	{ "1920 x 1080", 1920, 1080 },
};

struct CameraRate
{
	const char* label;
	int fps; // 0 leaves the driver's default
};

const CameraRate kCameraRates[] = {
	{ "相機預設", 0 },
	{ "30 fps", 30 },
	{ "60 fps", 60 },
	{ "120 fps", 120 },
};

struct Codec
{
	const char* label;
	char fourcc[5];
	const char* extension;
};

// Motion JPEG in AVI is written by OpenCV itself, so it works everywhere;
// the others depend on what the OpenCV build was compiled with.
const Codec kCodecs[] = {
	{ "Motion JPEG (.avi)", "MJPG", ".avi" },
	{ "MPEG-4 (.mp4)", "mp4v", ".mp4" },
	{ "Xvid (.avi)", "XVID", ".avi" },
};

const char* const kDifficulties[] = { "簡單", "普通", "困難" };

#if defined(_WIN32)
const bool kImageClipboard = true;
#else
const bool kImageClipboard = false; // copyBgrToClipboard is Windows only
#endif

cv::Mat toBgr(const webcam::Yuy2Frame& frame)
{
	const cv::Mat yuy2(frame.height(), frame.width(), CV_8UC2, const_cast<std::uint8_t*>(frame.data()));
	cv::Mat bgr;
	cv::cvtColor(yuy2, bgr, cv::COLOR_YUV2BGR_YUY2);
	return bgr;
}

webcam::Yuv yuvOf(const float rgb[3])
{
	const webcam::Rgb c = { static_cast<std::uint8_t>(std::lround(rgb[0] * 255.0f)),
		static_cast<std::uint8_t>(std::lround(rgb[1] * 255.0f)),
		static_cast<std::uint8_t>(std::lround(rgb[2] * 255.0f)) };
	return webcam::rgbToYuv(c);
}

// Pixels of the bottom-left pixel's color become transparent, as the
// original's bitmaps expected.
cv::Mat keyedToBgra(const cv::Mat& bgr)
{
	if (bgr.empty())
		return cv::Mat();
	const cv::Vec3b key = bgr.at<cv::Vec3b>(bgr.rows - 1, 0);
	cv::Mat bgra(bgr.size(), CV_8UC4);
	for (int y = 0; y < bgr.rows; ++y)
		for (int x = 0; x < bgr.cols; ++x)
		{
			const cv::Vec3b c = bgr.at<cv::Vec3b>(y, x);
			bgra.at<cv::Vec4b>(y, x) = cv::Vec4b(c[0], c[1], c[2], c == key ? 0 : 255);
		}
	return bgra;
}

void uploadThumbnail(Texture& texture, const cv::Mat& image, int maxSide)
{
	if (image.empty())
		return;
	const double scale = std::min(1.0, static_cast<double>(maxSide) / std::max(image.cols, image.rows));
	cv::Mat small;
	cv::resize(image, small, cv::Size(), scale, scale, cv::INTER_AREA);
	texture.upload(small);
}

cv::Mat decodeImage(const std::filesystem::path& file)
{
	std::vector<unsigned char> bytes;
	if (!readFile(file, bytes) || bytes.empty())
		return cv::Mat();
	return cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
}

// By name only: whether OpenCV can read it shows when it is opened.
bool isVideoFile(const std::filesystem::path& file)
{
	std::string ext = file.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const char* videos[] = { ".mp4", ".m4v", ".mov", ".avi", ".mkv", ".webm", ".wmv", ".mpg", ".mpeg", ".gif" };
	return std::find(std::begin(videos), std::end(videos), ext) != std::end(videos);
}

// What the music player can open (miniaudio's built-in decoders).
bool isAudioFile(const std::filesystem::path& file)
{
	std::string ext = file.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return ext == ".mp3" || ext == ".wav" || ext == ".flac";
}

// Index of the biggest face, or -1.
int biggestFace(const std::vector<Face>& faces)
{
	int best = -1;
	for (int i = 0; i < static_cast<int>(faces.size()); ++i)
		if (best < 0 || faces[i].eyeDistance() > faces[best].eyeDistance())
			best = i;
	return best;
}

bool hasTransparency(const cv::Mat& image)
{
	if (image.channels() != 4)
		return false;
	double lowest = 255;
	cv::Mat alpha;
	cv::extractChannel(image, alpha, 3);
	cv::minMaxLoc(alpha, &lowest);
	return lowest < 250;
}

cv::Mat toBgr8(const cv::Mat& image)
{
	cv::Mat out;
	if (image.depth() != CV_8U)
		image.convertTo(out, CV_8U, image.depth() == CV_16U ? 1.0 / 257.0 : 1.0);
	else
		out = image;
	if (out.channels() == 1)
		cv::cvtColor(out, out, cv::COLOR_GRAY2BGR);
	else if (out.channels() == 4)
		cv::cvtColor(out, out, cv::COLOR_BGRA2BGR);
	return out;
}

void pushPrimary()
{
	ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccent);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
}

void popPrimary()
{
	ImGui::PopStyleColor(4);
}

void hint(const char* text)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void drawCentered(ImDrawList* draw, ImFont* font, float size, ImVec2 center, ImU32 color, const char* text)
{
	const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
	draw->AddText(font, size, ImVec2(center.x - extent.x * 0.5f, center.y - extent.y * 0.5f), color, text);
}

// A numbered circle for the guide; green with a tick once done.
void stepBadge(int number, bool done, bool current, float scale)
{
	const float radius = 11.0f * scale;
	const ImVec2 cursor = ImGui::GetCursorScreenPos();
	const float lineHeight = ImGui::GetFrameHeight();
	const ImVec2 center(cursor.x + radius, cursor.y + lineHeight * 0.5f);
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImU32 fill = ImGui::GetColorU32(done ? kDone : (current ? kAccent : ImGui::GetStyleColorVec4(ImGuiCol_Button)));
	draw->AddCircleFilled(center, radius, fill);
	if (done)
	{
		const ImU32 white = IM_COL32(255, 255, 255, 255);
		draw->AddLine(ImVec2(center.x - radius * 0.45f, center.y), ImVec2(center.x - radius * 0.1f, center.y + radius * 0.4f), white, 2.0f * scale);
		draw->AddLine(ImVec2(center.x - radius * 0.1f, center.y + radius * 0.4f), ImVec2(center.x + radius * 0.5f, center.y - radius * 0.4f), white, 2.0f * scale);
	}
	else
	{
		const char digit[2] = { static_cast<char>('0' + number), 0 };
		drawCentered(draw, ImGui::GetFont(), ImGui::GetFontSize(), center,
			current ? IM_COL32(20, 20, 20, 255) : ImGui::GetColorU32(ImGuiCol_Text), digit);
	}
	ImGui::Dummy(ImVec2(radius * 2.0f + 6.0f * scale, lineHeight));
	ImGui::SameLine();
}

} // namespace

// ---------------------------------------------------------------------------
// Construction

App::App(const Assets& assets, UiFonts fonts, bool persistent) : fonts_(fonts), persistent_(persistent)
{
	const std::filesystem::path exeDir = executableDir();
	home_ = exeDir.empty() ? std::filesystem::current_path() : exeDir;
	outputDir_ = home_ / "captures";

	const webcam::YuvRange range = { 125, 255, 125, 255, 120, 255 };
	colorRange_ = std::make_shared<webcam::YuvRange>(range);

	// AI models; without them the booth falls back to learning the background.
	std::string error;
	if (!ai_.loadSegmenter(assets.root() / "models" / "human_segmentation_pphumanseg_2023mar.onnx", error))
		log("Person segmentation is off: " + error);
	if (!ai_.loadFaceTracker(assets.root() / "models" / "face_detection_yunet_2023mar.onnx", error))
		log("Face stickers are off: " + error);
	else if (!movie_.loadFaceTracker(assets.root() / "models" / "face_detection_yunet_2023mar.onnx", error) ||
		!grabber_.loadFaceTracker(assets.root() / "models" / "face_detection_yunet_2023mar.onnx", error))
		log("Starring in a video and pranks are off: " + error);
	fpsWindow_ = Clock::now();
	cutOut_ = ai_.segmenterReady() ? CutOut::Ai : CutOut::Difference;

	// Backgrounds
	for (int i = 0; i < 3; ++i)
		addBackground("Frame" + std::to_string(i + 1), assets.bgr("Frame" + std::to_string(i + 1) + ".bmp"));
	cutOutByDifference_ = std::make_shared<webcam::BackgroundReplaceEffect>(backgrounds_[0].image);
	cutOutByColor_ = std::make_shared<webcam::ChromaKeyEffect>(colorRange_, backgrounds_[0].image);

	// Stickers placed on the picture: gecko, dragon ball, color box.
	const cv::Mat gecko = assets.bgr("SpottedGecko.bmp");
	for (int i = 0; i < 7; ++i)
		dragonBalls_[i] = keyedToBgra(assets.bgr("DragonBall" + std::to_string(i + 1) + ".bmp"));
	Placed geckoSticker;
	geckoSticker.name = "gecko";
	geckoSticker.bgra = keyedToBgra(gecko);
	geckoSticker.center = cv::Point2f(0.14f, 0.2f);
	geckoSticker.width = 0.14f;
	placed_.push_back(std::move(geckoSticker));
	Placed ball;
	ball.name = "dragonball";
	ball.bgra = dragonBalls_[dragonBallIndex_];
	ball.center = cv::Point2f(0.24f, 0.14f);
	ball.width = 0.07f;
	placed_.push_back(std::move(ball));
	Placed box;
	box.name = "box";
	box.center = cv::Point2f(0.5f, 0.12f);
	box.width = 0.18f;
	placed_.push_back(std::move(box));
	for (std::size_t i = 0; i < 2; ++i)
		uploadThumbnail(placed_[i].thumbnail, placed_[i].bgra, 96);
	refreshBoxSticker();
	for (int i = 0; i < FaceStickers::kCount; ++i)
		uploadThumbnail(faceThumbs_[i], faceArt_.art(static_cast<FaceStickers::Kind>(i)), 96);

	grayEffect_ = std::make_shared<webcam::TintEffect>(webcam::Yuv{ 0, 128, 128 });
	tintEffect_ = std::make_shared<webcam::TintEffect>(yuvOf(tintColor_));
	skinEffect_ = std::make_shared<webcam::ColorHighlightEffect>(colorRange_, webcam::rgbToYuv(kSkinTint));
	colorMarkEffect_ = std::make_shared<webcam::ColorHighlightEffect>(colorRange_, webcam::rgbToYuv(kGreen));
	difference_ = std::make_shared<webcam::DifferenceViewEffect>(webcam::DifferenceViewEffect::kBinary);

	// The game's geckos and the wave-to-shoot camera icon.
	geckoImage_ = keyedImageFromBgr(gecko, kWhite);
	for (int i = 0; i < 3; ++i)
	{
		Gecko g;
		g.sprite = std::make_shared<webcam::Sprite>(geckoImage_);
		g.effect = std::make_shared<webcam::TouchToHideEffect>(g.sprite);
		geckos_.push_back(g);
	}
	cameraButton_ = std::make_shared<webcam::Sprite>(assets.keyed("Camera.bmp", kWhite));
	photoTrigger_ = std::make_shared<webcam::TouchTriggerEffect>(cameraButton_,
		[this](const webcam::Yuy2Frame&, const webcam::Yuy2Frame&) {
			// Saved once the whole picture is done, with stickers and frame.
			waveShot_ = true;
		});
	photoTrigger_->setRearmAfterQuietFrames(kRearmQuietFrames);

	request_.width = kVideoSizes[sizeIndex_].width;
	request_.height = kVideoSizes[sizeIndex_].height;

	std::filesystem::path music = home_ / pathFromUtf8("僕が死のうと思ったのは.mp3");
	std::error_code ec;
	if (!std::filesystem::exists(music, ec))
		music = home_ / "music.mp3";
	std::snprintf(musicPath_, sizeof(musicPath_), "%s", pathToUtf8(music).c_str());

	if (persistent_)
	{
		loadUserPictures();
		loadSettings();
	}
	std::error_code exists;
	if (std::filesystem::exists(pathFromUtf8(musicPath_), exists) && music_.load(pathFromUtf8(musicPath_), error))
	{
		music_.setVolume(volume_);
		music_.setLooping(loop_);
	}
	lastUpdate_ = Clock::now();
}

App::~App()
{
	if (recording_)
		writer_.release();
	camera_.close();
	if (persistent_)
		saveSettings();
}

void App::log(const std::string& line)
{
	std::cout << line << std::endl;
	log_.push_back("[" + timestamp().substr(9) + "] " + line);
	while (log_.size() > 300)
		log_.pop_front();
}

void App::toast(const std::string& text)
{
	toast_ = text;
	toastAt_ = Clock::now();
	log(text);
}

bool App::addBackground(const std::string& name, const cv::Mat& image)
{
	const cv::Mat bgr = toBgr8(image);
	if (bgr.empty())
		return false;
	Picture picture;
	picture.name = name;
	picture.bgr = bgr;
	picture.image = imageFromBgr(bgr);
	uploadThumbnail(picture.thumbnail, bgr, 160);
	backgrounds_.push_back(std::move(picture));
	return true;
}

bool App::addSticker(const std::string& name, const cv::Mat& bgra)
{
	if (bgra.empty() || bgra.type() != CV_8UC4)
		return false;
	Placed sticker;
	sticker.name = name;
	sticker.bgra = bgra;
	sticker.on = true;
	sticker.center = cv::Point2f(0.5f, 0.45f);
	sticker.width = 0.25f;
	uploadThumbnail(sticker.thumbnail, bgra, 96);
	placed_.push_back(std::move(sticker));
	return true;
}

bool App::addClip(const std::string& name, const std::filesystem::path& file)
{
	const cv::Mat first = MoviePlayer::firstFrame(file);
	if (first.empty())
		return false;
	Clip clip;
	clip.name = name;
	clip.file = file;
	// OpenCV reads no sound, so a video's soundtrack is a file of the same name beside it.
	std::error_code ec;
	for (const char* ext : { ".mp3", ".wav", ".flac", ".MP3", ".WAV", ".FLAC" })
	{
		std::filesystem::path audio = file;
		audio.replace_extension(ext);
		if (clip.audio.empty() && std::filesystem::exists(audio, ec))
			clip.audio = audio;
	}
	uploadThumbnail(clip.thumbnail, first, 160);
	clips_.push_back(std::move(clip));
	return true;
}

void App::refreshBoxSticker()
{
	cv::Mat box(50, 200, CV_8UC4, cv::Scalar(boxColor_[2] * 255.0f, boxColor_[1] * 255.0f, boxColor_[0] * 255.0f, 255));
	placed_[2].bgra = box;
	uploadThumbnail(placed_[2].thumbnail, box, 96);
}

void App::loadUserPictures()
{
	// Pictures and videos dropped on the window earlier.
	std::error_code ec;
	const std::filesystem::path videos = home_ / "user" / "videos";
	if (std::filesystem::is_directory(videos, ec))
		for (const auto& entry : std::filesystem::directory_iterator(videos, ec))
			if (isVideoFile(entry.path()))
				addClip(pathToUtf8(entry.path().filename()), entry.path());
	const char* folders[] = { "backgrounds", "stickers" };
	for (int kind = 0; kind < 2; ++kind)
	{
		const std::filesystem::path dir = home_ / "user" / folders[kind];
		if (!std::filesystem::is_directory(dir, ec))
			continue;
		for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
		{
			const cv::Mat image = decodeImage(entry.path());
			const std::string name = pathToUtf8(entry.path().filename());
			if (kind == 0)
				addBackground(name, image);
			else if (image.channels() == 4)
			{
				addSticker(name, image);
				placed_.back().on = false;
			}
		}
	}
}

void App::onDrop(const std::vector<std::filesystem::path>& files)
{
	int added = 0;
	// Where a copy goes, under a name not taken yet.
	const auto keep = [this](const std::filesystem::path& file, const char* folder) {
		const std::filesystem::path dir = home_ / "user" / folder;
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		std::filesystem::path copy = dir / file.filename();
		for (int i = 2; std::filesystem::exists(copy, ec); ++i)
			copy = dir / (file.stem().native() + pathFromUtf8("-" + std::to_string(i)).native() + file.extension().native());
		if (persistent_)
			std::filesystem::copy_file(file, copy, ec);
		return copy;
	};
	// A video and its soundtrack dropped together: the sound is kept beside the
	// video (before the video is added, which looks for it).
	std::vector<std::filesystem::path> ordered = files;
	std::stable_partition(ordered.begin(), ordered.end(), isAudioFile);
	const auto soundtrackOf = [&files](const std::filesystem::path& audio) {
		return std::any_of(files.begin(), files.end(), [&audio](const std::filesystem::path& file) {
			return isVideoFile(file) && file.stem() == audio.stem();
		});
	};
	for (const std::filesystem::path& file : ordered)
	{
		if (isAudioFile(file) && soundtrackOf(file))
		{
			keep(file, "videos");
			++added;
			continue;
		}
		if (isAudioFile(file))
		{
			std::string error;
			if (!music_.load(file, error))
			{
				toast("打不開這個音樂檔：" + pathToUtf8(file.filename()));
				continue;
			}
			std::snprintf(musicPath_, sizeof(musicPath_), "%s", pathToUtf8(file).c_str());
			music_.setVolume(volume_);
			music_.setLooping(loop_);
			++added;
			toast("音樂換成：" + pathToUtf8(file.filename()));
			continue;
		}
		if (isVideoFile(file))
		{
			const std::filesystem::path copy = keep(file, "videos");
			std::error_code ec;
			const std::string name = pathToUtf8(copy.filename());
			if (!addClip(name, persistent_ && std::filesystem::exists(copy, ec) ? copy : file))
			{
				toast("打不開這部影片：" + pathToUtf8(file.filename()));
				continue;
			}
			++added;
			drawer_ = Drawer::Movie;
			startMovie(static_cast<int>(clips_.size()) - 1);
			toast("已加入影片：" + name);
			continue;
		}
		const cv::Mat image = decodeImage(file);
		if (image.empty())
		{
			toast("不是圖片或影片：" + pathToUtf8(file.filename()));
			continue;
		}
		const bool sticker = hasTransparency(image);
		if (drawer_ == Drawer::Prank && !sticker)
		{
			// A screenshot of the other person, where their window cannot be watched.
			grabber_.show(toBgr8(image));
			windowIndex_ = -1;
			++added;
			toast("改用這張圖裡的臉");
			continue;
		}
		const std::filesystem::path copy = keep(file, sticker ? "stickers" : "backgrounds");
		const std::string name = pathToUtf8(copy.filename());
		if (sticker ? addSticker(name, image) : addBackground(name, image))
		{
			++added;
			if (sticker)
				drawer_ = Drawer::Stickers;
			else
			{
				backgroundIndex_ = static_cast<int>(backgrounds_.size()) - 1;
				if (backgroundMode_ == BackgroundMode::Off)
					backgroundMode_ = BackgroundMode::Replace;
				drawer_ = Drawer::Background;
			}
			toast(std::string(sticker ? "已加入貼圖：" : "已加入背景：") + name);
		}
	}
	if (added == 0 && !files.empty())
		toast("沒有可以用的圖片或影片");
}

bool App::aiTouch() const
{
	return ai_.segmenterReady();
}

bool App::needsLearnedBackground() const
{
	const bool cutOutNeeds = backgroundMode_ == BackgroundMode::Replace && cutOut_ == CutOut::Difference;
	const bool touchNeeds = !aiTouch() && (wavePhoto_ || game_ == GameState::Playing);
	return cutOutNeeds || touchNeeds;
}

bool App::backgroundReady() const
{
	const webcam::ChangeDetector& detector = effects_.detector();
	return detector.hasBackground() && !detector.isLearning() && detector.backgroundWidth() == frameWidth_ &&
		detector.backgroundHeight() == frameHeight_;
}

void App::openCamera()
{
	const std::vector<CameraBackend>& backends = cameraBackends();
	backendIndex_ = std::min(backendIndex_, static_cast<int>(backends.size()) - 1);
	request_.api = backends[backendIndex_].api;
	request_.width = kVideoSizes[sizeIndex_].width;
	request_.height = kVideoSizes[sizeIndex_].height;
	request_.fps = kCameraRates[std::clamp(fpsIndex_, 0, 3)].fps;
	if (recording_)
		toggleRecording();
	frozen_ = false;
	camera_.open(request_);
	log("Opening camera " + std::to_string(request_.index) + "...");
}

void App::closeCamera()
{
	if (recording_)
		toggleRecording();
	frozen_ = false;
	camera_.close();
	video_ = Texture();
	log("Camera closed.");
}

void App::stageScreenshot(int scene, const cv::Mat& room, const cv::Mat& person, const cv::Mat& personMask)
{
	staged_ = true;
	persistent_ = false;
	stagedFrame_ = person;
	// The mask and face as they appear after mirroring.
	cv::flip(personMask, stagedMask_, 1);
	Face face;
	face.box = cv::Rect2f(static_cast<float>(person.cols - 1 - 630), 160.0f, 140.0f, 180.0f);
	face.eyeLeft = cv::Point2f(static_cast<float>(person.cols - 1 - 590), 235.0f);
	face.eyeRight = cv::Point2f(static_cast<float>(person.cols - 1 - 530), 235.0f);
	face.nose = cv::Point2f(static_cast<float>(person.cols - 1 - 560), 270.0f);
	face.mouth = cv::Point2f(static_cast<float>(person.cols - 1 - 560), 300.0f);
	stagedFaces_.push_back(face);
	(void)room;
	processFrame(stagedFrame_); // sets the frame size the scenes below rely on

	if (scene >= 1 && scene != 6 && scene != 5)
	{
		backgroundMode_ = BackgroundMode::Replace;
		backgroundIndex_ = 2;
		faceSticker_ = FaceStickers::kSunglasses;
		placed_[0].on = true;
		drawer_ = Drawer::Background;
		frameStyle_ = FrameStyle::Polaroid;
		dateStamp_ = true;
		Photo photo;
		uploadThumbnail(photo.thumbnail, person, 160);
		photos_.push_front(std::move(photo));
		toast("已存檔：photo-20261027-153012.png");
	}
	if (scene == 2)
		advanced_ = true;
	if (scene == 3)
	{
		drawer_ = Drawer::Stickers;
		faceSticker_ = FaceStickers::kPartyHat;
		placed_[1].on = true;
		frameStyle_ = FrameStyle::None;
		dateStamp_ = false;
	}
	if (scene == 4)
	{
		drawer_ = Drawer::Filters;
		look_ = Look::Cartoon;
		adjust_.vignette = true;
		frameStyle_ = FrameStyle::Film;
	}
	if (scene == 5 || scene == 6)
	{
		drawer_ = Drawer::Game;
		difficulty_ = 1;
		best_[1] = 12;
		startGame();
		score_ = 7;
		if (scene == 6)
		{
			game_ = GameState::Over;
			newBest_ = false;
		}
	}
	if (scene == 7)
	{
		startFourShot();
		fourShot_ = 1;
		fourShots_.push_back(person);
		shootAt_ = Clock::now() + std::chrono::seconds(2);
	}
	if (scene == 8)
	{
		frameStyle_ = FrameStyle::None;
		dateStamp_ = false;
		backgroundMode_ = BackgroundMode::Off;
		placed_[0].on = false;
		faceSticker_ = FaceStickers::kCrown;
		stageClip(room, person, personMask);
		drawer_ = Drawer::Movie;
		processFrame(stagedFrame_);
	}
	if (scene == 9 || scene == 10)
	{
		// Their face, mid-song with big eyes, on the actor of a video. The
		// "other person" is the same figure, unmirrored, given eyes to show the warp.
		frameStyle_ = FrameStyle::None;
		dateStamp_ = false;
		backgroundMode_ = BackgroundMode::Off;
		placed_[0].on = false;
		faceSticker_ = -1;
		stageClip(room, person, personMask);
		stagedTheirs_.bgr = person.clone();
		Face theirs;
		theirs.eyeLeft = cv::Point2f(530.0f, 235.0f);
		theirs.eyeRight = cv::Point2f(590.0f, 235.0f);
		theirs.nose = cv::Point2f(560.0f, 270.0f);
		theirs.mouth = cv::Point2f(560.0f, 300.0f);
		for (const cv::Point2f eye : { theirs.eyeLeft, theirs.eyeRight })
		{
			cv::circle(stagedTheirs_.bgr, eye, 9, cv::Scalar(255, 255, 255), cv::FILLED, cv::LINE_AA);
			cv::circle(stagedTheirs_.bgr, eye, 5, cv::Scalar(40, 30, 20), cv::FILLED, cv::LINE_AA);
		}
		stagedTheirs_.faces = { theirs };
		ScreenWindow call;
		call.handle = 1;
		call.title = "LINE - 視訊通話";
		windows_ = { ScreenWindow(), call };
		windows_[0].title = "整個主螢幕";
		windowIndex_ = 1;
		prankMode_ = PrankMode::TheirFaceOnVideo;
		singAlong_ = true;
		warp_ = Warp::BigEyes;
		prankOn_ = true;
		drawer_ = Drawer::Prank;
		toast_.clear();
		processFrame(stagedFrame_);
	}
	if (scene == 10)
	{
		// A call window: them large, me small in the corner; the box round them.
		cv::Mat call(720, 1280, CV_8UC3, cv::Scalar(40, 36, 34));
		cv::resize(stagedTheirs_.bgr, call(cv::Rect(40, 40, 800, 600)), cv::Size(800, 600));
		cv::resize(person, call(cv::Rect(900, 440, 320, 240)), cv::Size(320, 240));
		regionShot_.upload(call);
		regionWindow_ = 1;
		regionFrom_ = cv::Point2f(260.0f / 1280.0f, 60.0f / 720.0f);
		regionTo_ = cv::Point2f(640.0f / 1280.0f, 420.0f / 720.0f);
		openRegion_ = true;
	}
}

// A "video" for the screenshots: the room with the figure standing at the
// side, larger, as its actor.
void App::stageClip(const cv::Mat& room, const cv::Mat& person, const cv::Mat& personMask)
{
	cv::resize(room, stagedMovie_, cv::Size(1280, 720));
	const float scale = 1.3f;
	const cv::Point2f shift(-300.0f, -120.0f);
	cv::Mat figure, mask;
	cv::resize(person, figure, cv::Size(), scale, scale);
	cv::resize(personMask, mask, cv::Size(), scale, scale);
	const cv::Rect into = cv::Rect(static_cast<int>(shift.x), static_cast<int>(shift.y), figure.cols, figure.rows) &
		cv::Rect(0, 0, stagedMovie_.cols, stagedMovie_.rows);
	const cv::Rect from = into - cv::Point(static_cast<int>(shift.x), static_cast<int>(shift.y));
	figure(from).copyTo(stagedMovie_(into), mask(from) > 128);
	const auto place = [&](float x, float y) { return cv::Point2f(x * scale + shift.x, y * scale + shift.y); };
	Face actor;
	actor.eyeLeft = place(530.0f, 235.0f);
	actor.eyeRight = place(590.0f, 235.0f);
	actor.nose = place(560.0f, 270.0f);
	actor.mouth = place(560.0f, 300.0f);
	actor.box = cv::Rect2f(place(490.0f, 160.0f), cv::Size2f(140.0f * scale, 180.0f * scale));
	stagedMovieFaces_ = { actor };
	Clip clip;
	clip.name = "示範影片.mp4";
	clip.audio = pathFromUtf8("示範影片.mp3");
	uploadThumbnail(clip.thumbnail, stagedMovie_, 160);
	clips_.push_back(std::move(clip));
	clipIndex_ = 0;
}

// ---------------------------------------------------------------------------
// Frames

void App::syncEffects()
{
	const auto set = [this](const std::shared_ptr<webcam::Effect>& effect, bool on) {
		if (on)
			effects_.add(effect);
		else
			effects_.remove(effect);
	};
	const bool touchReady = aiTouch() || backgroundReady();
	const bool replace = backgroundMode_ == BackgroundMode::Replace;

	// Mirroring is done before the core, in OpenCV.
	if (effects_.mirror())
		effects_.setMirror(false);
	set(cutOutByDifference_, replace && cutOut_ == CutOut::Difference && backgroundReady());
	set(cutOutByColor_, replace && cutOut_ == CutOut::GreenScreen);
	set(grayEffect_, gray_);
	tintEffect_->setColor(yuvOf(tintColor_));
	set(tintEffect_, tint_);
	set(skinEffect_, skinMark_);
	set(colorMarkEffect_, colorMark_);
	const int geckos = difficulty_ + 1;
	for (int i = 0; i < static_cast<int>(geckos_.size()); ++i)
	{
		geckos_[i].effect->setShowBlocks(showBlocks_ && i == 0);
		set(geckos_[i].effect, game_ == GameState::Playing && touchReady && i < geckos);
	}
	// Not while the picture goes out to a video call: the icon would give the booth away.
	set(photoTrigger_, wavePhoto_ && touchReady && fourShot_ < 0 && !outputOpen_);
	if (debugView_ > 0)
		difference_->setMode(static_cast<webcam::DifferenceViewEffect::Mode>(debugView_ - 1));
	set(difference_, debugView_ > 0);
}

void App::processFrame(const cv::Mat& frame)
{
	cv::Mat input = frame;
	if (input.type() == CV_8UC1)
		cv::cvtColor(input, input, cv::COLOR_GRAY2BGR);
	else if (input.type() == CV_8UC4)
		cv::cvtColor(input, input, cv::COLOR_BGRA2BGR);
	if (input.type() != CV_8UC3 || input.cols < 2)
		return;
	if (input.cols % 2 != 0) // YUY2 pairs pixels
		input = input.colRange(0, input.cols - 1);

	if (input.cols != frameWidth_ || input.rows != frameHeight_)
	{
		frameWidth_ = input.cols;
		frameHeight_ = input.rows;
		cameraButton_->moveTo(std::max(0, frameWidth_ - cameraButton_->width() - 15),
			std::max(0, frameHeight_ - cameraButton_->height() - 5));
		if (recording_)
			toggleRecording();
	}

	const Clock::time_point start = Clock::now();
	lastProcessed_ = start;
	if (mirror_)
		cv::flip(input, mirrored_, 1);
	else
		input.copyTo(mirrored_);
	cv::Mat picture = mirrored_.clone();

	// 1. Where the person is, from the AI. A video needs only the faces.
	const bool movie = movieOn();
	const bool aiCutOut = backgroundMode_ != BackgroundMode::Off && cutOut_ == CutOut::Ai;
	const bool wantTouch = (wavePhoto_ && fourShot_ < 0) || game_ == GameState::Playing || debugView_ > 0;
	const bool wantMask = ai_.segmenterReady() && (aiCutOut || wantTouch) && !movie;
	const bool wantFaces = (faceSticker_ >= 0 || movie || prankOn_) && ai_.faceTrackerReady();
	bool haveMask = false;
	bool newMask = false;
	if (staged_)
	{
		if (wantMask && !stagedMask_.empty())
		{
			personAlpha_ = stagedMask_;
			haveMask = newMask = true;
		}
		faces_ = stagedFaces_;
		facePicture_ = mirrored_;
	}
	else if (wantMask || wantFaces)
	{
		// The AI runs on its own thread; use whatever it finished last.
		ai_.submit(mirrored_, wantMask, wantFaces);
		const AiWorker::Result result = ai_.latest();
		newMask = result.sequence != aiSeen_;
		aiSeen_ = result.sequence;
		if (wantMask && !result.alpha.empty() && result.alpha.size() == mirrored_.size())
		{
			personAlpha_ = result.alpha;
			haveMask = true;
		}
		if (wantFaces)
		{
			faces_ = result.faces;
			facePicture_ = result.picture;
		}
		aiMs_ = result.segmentMs + result.facesMs;
	}
	// Touching means moving into a place: person now, not a moment ago. A body
	// standing still next to the camera icon or a gecko does not count.
	if (haveMask && aiTouch())
	{
		if (newMask)
		{
			cv::Mat person = personAlpha_ >= 128;
			if (!maskHistory_.empty() && maskHistory_.back().size() != person.size())
				maskHistory_.clear();
			maskHistory_.push_back(person);
			while (static_cast<int>(maskHistory_.size()) > kMotionFrames + 1)
				maskHistory_.pop_front();
		}
		if (maskHistory_.empty())
			effects_.setExternalDetection(webcam::Detection());
		else
		{
			cv::Mat moved = maskHistory_.back() & ~maskHistory_.front();
			effects_.setExternalDetection(webcam::Detection::fromMask(moved.data, moved.cols, moved.rows, moved.step));
		}
	}
	else
	{
		maskHistory_.clear();
		effects_.setExternalDetection(webcam::Detection());
	}

	// The music's loudness drives the singing jaw: open at once on a beat,
	// closing a little slower.
	const float level = prankOn_ && singAlong_ ? (staged_ ? 0.9f : music_.level()) : 0.0f;
	jaw_ = level > jaw_ ? level : jaw_ * 0.75f + level * 0.25f;

	const bool shoot = shootNextFrame_;
	cv::Mat shown;
	if (movie)
	{
		const FaceFrame clip = staged_ ? FaceFrame{ 0, stagedMovie_, stagedMovieFaces_ } : movie_.latest();
		movieFrame_ = !clip.bgr.empty();
		if (prankOn_ && prankMode_ == PrankMode::TheirFaceOnVideo)
		{
			// Their face on the actors; until it is found, the video as it is
			// (never the face in front of the camera, which would give it away).
			const FaceFrame theirs = theirFrame();
			const int face = biggestFace(theirs.faces);
			theirFace_ = face >= 0;
			if (theirFace_)
				shown = composeOnto(clip, prankStyle(), &theirs.bgr, &theirs.faces[face]);
			else if (movieFrame_)
				shown = clip.bgr.clone();
		}
		else
			shown = composeOnto(clip, prankOn_ ? prankStyle() : PasteStyle());
	}
	else if (prankOn_ && prankMode_ == PrankMode::MyFaceOnThem)
	{
		const FaceFrame theirs = theirFrame();
		theirFace_ = !theirs.faces.empty();
		shown = composeOnto(theirs, prankStyle());
	}
	if (shown.empty()) // the camera picture, or the video or window has not sent a picture yet
	{
		// 2. A new or blurred background.
		if (haveMask && aiCutOut)
		{
			if (backgroundMode_ == BackgroundMode::Blur)
				styler_.blurBackground(picture, personAlpha_);
			else
				styler_.replaceBackground(picture, personAlpha_,
					backgrounds_[std::min<std::size_t>(backgroundIndex_, backgrounds_.size() - 1)].bgr);
		}

		// 3. The core: per-pixel filters, the game, the camera icon.
		syncEffects();
		yuy2_.resize(webcam::Yuy2Frame::bytesFor(frameWidth_, frameHeight_));
		webcam::Yuy2Frame yuy2Frame;
		yuy2Frame.wrap(yuy2_.data(), yuy2_.size(), frameWidth_, frameHeight_);
		// Same result as Yuy2Frame::copyFromBgr24 (checked by --self-test), ten times faster.
		cv::Mat yuy2(frameHeight_, frameWidth_, CV_8UC2, yuy2_.data());
		cv::cvtColor(picture, yuy2, cv::COLOR_BGR2YUV_YUY2);
		const bool iconVisible = cameraButton_->visible();
		if (shoot)
			cameraButton_->hide(); // pictures from the button leave the icon out
		effects_.process(yuy2Frame);
		if (shoot && iconVisible)
			cameraButton_->show();
		shown = toBgr(yuy2Frame);

		// 4. Looks, then stickers on top.
		styler_.applyLook(shown, look_);
		if (adjust_.any())
			styler_.applyAdjustments(shown, adjust_);
		if (prankOn_ && prankMode_ != PrankMode::TheirFaceOnVideo)
			prankFaces(shown);
		if (faceSticker_ >= 0)
			faceArt_.draw(shown, faces_, static_cast<FaceStickers::Kind>(faceSticker_));
		for (const Placed& sticker : placed_)
			if (sticker.on)
				overlayBgra(shown, sticker.bgra, cv::Point2f(sticker.center.x * frameWidth_, sticker.center.y * frameHeight_),
					sticker.width * frameWidth_);
	}
	// Then the frame, on either.
	if (frameStyle_ != FrameStyle::None || dateStamp_)
		styler_.applyFrame(shown, frameStyle_, dateStamp_);
	shown_ = shown;
	processingMs_ = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
	++framesProcessed_;
	++processedInWindow_;
	const double window = std::chrono::duration<double>(Clock::now() - fpsWindow_).count();
	if (window >= 1.0)
	{
		processingFps_ = processedInWindow_ / window;
		processedInWindow_ = 0;
		fpsWindow_ = Clock::now();
	}

	// 5. Pictures, recording, display.
	if (shoot)
	{
		shootNextFrame_ = false;
		if (fourShot_ >= 0)
		{
			fourShots_.push_back(shown_.clone());
			fourShot_ = static_cast<int>(fourShots_.size());
			sound_.play(Sound::kShutter);
			flashAt_ = Clock::now();
			if (fourShot_ < 4)
				startCountdown(kFourShotGapSeconds);
			else
			{
				takePicture(makeCollage(fourShots_, dateStamp_));
				fourShots_.clear();
				fourShot_ = -1;
			}
		}
		else
			takePicture(shown_);
	}
	if (waveShot_)
	{
		waveShot_ = false;
		takePicture(shown_);
	}
	if (recording_ && shown_.size() != recordSize_)
		toggleRecording(); // a video and the camera differ in size; one file holds one size
	if (recording_)
	{
		writer_.write(shown_);
		++recordedFrames_;
	}
	video_.upload(shown_);

	// The game: count each catch, then bring that gecko back elsewhere.
	if (game_ == GameState::Playing)
		for (Gecko& gecko : geckos_)
		{
			const bool visible = gecko.sprite->visible();
			if (gecko.wasVisible && !visible && effects_.contains(gecko.effect))
			{
				++score_;
				sound_.play(Sound::kPop);
				gecko.respawnPending = true;
				gecko.respawnAt = Clock::now() + std::chrono::milliseconds(static_cast<int>(kRespawnSeconds * 1000));
			}
			gecko.wasVisible = visible;
		}

	if (backgroundJustLearned_)
	{
		backgroundJustLearned_ = false;
		const webcam::ChangeDetector& detector = effects_.detector();
		cv::Mat learned(detector.backgroundHeight(), detector.backgroundWidth(), CV_8UC3);
		for (int y = 0; y < learned.rows; ++y)
			for (int x = 0; x < learned.cols; ++x)
			{
				const webcam::Rgb c = webcam::yuvToRgb(detector.backgroundAt(x, y));
				learned.at<cv::Vec3b>(y, x) = cv::Vec3b(c.b, c.g, c.r);
			}
		uploadThumbnail(backgroundPreview_, learned, 480);
		toast("背景記住了，可以開始玩了！");
	}
}

cv::Mat App::composeOnto(const FaceFrame& frame, const PasteStyle& style, const cv::Mat* from, const Face* fromFace)
{
	movieFaces_ = static_cast<int>(frame.faces.size());
	pastedFaces_ = 0;
	if (frame.bgr.empty())
		return cv::Mat();

	cv::Mat shown = frame.bgr.clone();
	std::vector<Face> covered;
	if (from != nullptr && fromFace != nullptr)
	{
		// One face on everybody: a whole cast of the same person.
		for (const Face& face : frame.faces)
		{
			pasteFace(*from, *fromFace, shown, face, style);
			covered.push_back(face);
		}
	}
	else
		// Biggest face in front of the camera onto the biggest in the picture, and so on.
		for (const std::pair<int, int>& pair : pairFaces(faces_, frame.faces))
		{
			pasteFace(facePicture_, faces_[pair.first], shown, frame.faces[pair.second], style);
			covered.push_back(frame.faces[pair.second]);
		}
	pastedFaces_ = static_cast<int>(covered.size());
	if (faceSticker_ >= 0)
		faceArt_.draw(shown, covered, static_cast<FaceStickers::Kind>(faceSticker_));
	return shown;
}

void App::prankFaces(cv::Mat& picture)
{
	const PasteStyle style = prankStyle();
	if (prankMode_ == PrankMode::TheirFaceOnMe)
	{
		const FaceFrame theirs = theirFrame();
		const int face = biggestFace(theirs.faces);
		theirFace_ = face >= 0;
		if (theirFace_)
		{
			for (const Face& mine : faces_)
				pasteFace(theirs.bgr, theirs.faces[face], picture, mine, style);
			return;
		}
	}
	// Just me, or no face to borrow yet: pull faces at my own.
	if (style.jaw <= 0.0f && style.warp == Warp::None)
		return;
	PasteStyle own = style;
	own.recolor = false;
	const cv::Mat copy = picture.clone();
	for (const Face& mine : faces_)
		pasteFace(copy, mine, picture, mine, own);
}

PasteStyle App::prankStyle() const
{
	PasteStyle style;
	// Quiet passages keep the mouth shut; the loud ones throw it open.
	style.jaw = singAlong_ ? std::clamp((jaw_ - 0.3f) / 0.7f, 0.0f, 1.0f) : 0.0f;
	style.warp = warp_;
	return style;
}

void App::togglePrank()
{
	prankOn_ = !prankOn_;
	jaw_ = 0.0f;
	if (prankMode_ == PrankMode::TheirFaceOnVideo)
	{
		// The video and its soundtrack start from the top, and end with the prank.
		if (!prankOn_)
			stopMovie();
		else if (movieOn())
			restartMovie();
		else if (!clips_.empty())
			startMovie(std::clamp(lastClip_, 0, static_cast<int>(clips_.size()) - 1));
		else
			toast("先把影片拖進視窗");
	}
	if (!clipAudio_ && singAlong_ && music_.loaded())
	{
		music_.stop();
		if (prankOn_)
			music_.play();
	}
	if (!prankOn_ || !clips_.empty() || prankMode_ != PrankMode::TheirFaceOnVideo)
		toast(prankOn_ ? "惡搞開始！按 F9 結束" : "惡搞結束");
}

FaceFrame App::theirFrame() const
{
	return staged_ ? stagedTheirs_ : grabber_.latest();
}

void App::refreshWindows()
{
	const std::uintptr_t watched = windowIndex_ >= 0 ? windows_[windowIndex_].handle : ~std::uintptr_t(0);
	windows_.clear();
	ScreenWindow screen;
	screen.title = "整個主螢幕";
	windows_.push_back(screen);
	for (const ScreenWindow& window : listScreenWindows())
		windows_.push_back(window);
	windowIndex_ = -1;
	for (std::size_t i = 0; i < windows_.size(); ++i)
		if (windows_[i].handle == watched)
			windowIndex_ = static_cast<int>(i);
}

void App::watchWindow(int index, const cv::Rect2f& part)
{
	windowIndex_ = index;
	grabPart_ = part;
	if (!staged_)
		grabber_.watch(windows_[index].handle, part);
}

void App::startRegionPick()
{
	if (windows_.empty())
		refreshWindows();
	const int index = windowIndex_ >= 0 ? windowIndex_ : 0; // nothing picked yet: the whole main screen
	cv::Mat shot;
	if (!staged_ && !grabScreenWindow(windows_[index].handle, shot))
	{
		toast("抓不到這個視窗：它被縮小或關掉了嗎？");
		return;
	}
	if (!staged_)
		regionShot_.upload(shot);
	regionWindow_ = index;
	// Start from the box in use, if this is the window it belongs to.
	const bool same = index == windowIndex_;
	regionFrom_ = same ? cv::Point2f(grabPart_.x, grabPart_.y) : cv::Point2f(0.0f, 0.0f);
	regionTo_ = same ? cv::Point2f(grabPart_.x + grabPart_.width, grabPart_.y + grabPart_.height) : cv::Point2f(1.0f, 1.0f);
	openRegion_ = true;
}

void App::toggleOutputWindow()
{
	outputOpen_ = !outputOpen_;
	if (setOutput_)
		setOutput_(outputOpen_);
	if (outputOpen_)
		toast("輸出視窗開好了：在 OBS 用「視窗擷取」選它");
}

void App::startMovie(int index)
{
	if (index < 0 || index >= static_cast<int>(clips_.size()))
		return;
	if (game_ == GameState::Playing)
		game_ = GameState::Idle; // the geckos live in the camera picture
	if (clipAudio_)
		stopMovie(); // gives the music player back its own track first
	clipIndex_ = index;
	lastClip_ = index;
	movieFrame_ = false;
	clipSeconds_ = 0.0;
	if (staged_)
		return;
	movie_.open(clips_[index].file);
	std::string error;
	if (!clips_[index].audio.empty() && music_.load(clips_[index].audio, error))
	{
		// Started over with the video, not on its own.
		clipAudio_ = true;
		music_.setVolume(volume_);
		music_.setLooping(false);
		music_.play();
	}
}

void App::stopMovie()
{
	movie_.close();
	clipIndex_ = -1;
	movieFrame_ = false;
	if (clipAudio_)
	{
		clipAudio_ = false;
		music_.stop();
		std::string error;
		std::error_code ec;
		if (std::filesystem::exists(pathFromUtf8(musicPath_), ec) && music_.load(pathFromUtf8(musicPath_), error))
			music_.setLooping(loop_);
	}
}

void App::pauseMovie(bool paused)
{
	movie_.setPaused(paused);
	if (clipAudio_)
		paused ? music_.pause() : music_.play();
}

void App::restartMovie()
{
	movie_.restart();
	clipSeconds_ = 0.0;
	if (clipAudio_)
	{
		music_.stop();
		music_.play();
	}
}

void App::followMovieAudio()
{
	if (!clipAudio_ || !movieOn())
		return;
	// The video went round again: so does its sound.
	const double seconds = movie_.seconds();
	if (seconds + 0.5 < clipSeconds_)
	{
		music_.stop();
		music_.play();
	}
	clipSeconds_ = seconds;
}

void App::moveGeckos(double seconds)
{
	if (game_ != GameState::Playing || frameWidth_ == 0)
		return;
	for (Gecko& gecko : geckos_)
	{
		if (gecko.velocity.x == 0.0f && gecko.velocity.y == 0.0f)
			continue;
		gecko.position += gecko.velocity * static_cast<float>(seconds);
		const float maxX = static_cast<float>(frameWidth_ - gecko.sprite->width());
		const float maxY = static_cast<float>(frameHeight_ - gecko.sprite->height());
		if (gecko.position.x < 0 || gecko.position.x > maxX)
			gecko.velocity.x = -gecko.velocity.x;
		if (gecko.position.y < 0 || gecko.position.y > maxY)
			gecko.velocity.y = -gecko.velocity.y;
		gecko.position.x = std::min(std::max(gecko.position.x, 0.0f), std::max(maxX, 0.0f));
		gecko.position.y = std::min(std::max(gecko.position.y, 0.0f), std::max(maxY, 0.0f));
		gecko.sprite->moveTo(static_cast<int>(gecko.position.x), static_cast<int>(gecko.position.y));
	}
}

void App::runTimers()
{
	const Clock::time_point now = Clock::now();
	const double elapsed = std::chrono::duration<double>(now - lastUpdate_).count();
	lastUpdate_ = now;

	if (learnPending_ && now >= learnAt_)
	{
		learnPending_ = false;
		effects_.startBackgroundLearning(kBackgroundFrames, [this]() { backgroundJustLearned_ = true; });
	}
	if (countdown_)
	{
		const int left = static_cast<int>(std::ceil(std::chrono::duration<double>(shootAt_ - now).count()));
		if (left != lastBeep_ && left > 0)
		{
			lastBeep_ = left;
			sound_.play(left == 1 ? Sound::kGo : Sound::kBeep);
		}
		if (now >= shootAt_)
		{
			countdown_ = false;
			shootNextFrame_ = true;
			if (frozen_ && fourShot_ < 0)
			{
				shootNextFrame_ = false;
				takePicture(shown_);
			}
		}
	}
	if (game_ == GameState::Playing)
	{
		moveGeckos(elapsed);
		for (Gecko& gecko : geckos_)
			if (gecko.respawnPending && now >= gecko.respawnAt)
			{
				gecko.respawnPending = false;
				respawnGecko(gecko);
			}
		if (now >= gameEnds_)
		{
			game_ = GameState::Over;
			newBest_ = score_ > best_[difficulty_];
			best_[difficulty_] = std::max(best_[difficulty_], score_);
			sound_.play(Sound::kFanfare);
		}
	}
}

void App::startLearning()
{
	learnPending_ = true;
	learnAt_ = Clock::now() + std::chrono::seconds(kLearnDelaySeconds);
}

void App::startCountdown(int seconds)
{
	countdown_ = true;
	lastBeep_ = 0;
	shootAt_ = Clock::now() + std::chrono::seconds(seconds);
}

void App::startFourShot()
{
	frozen_ = false;
	fourShots_.clear();
	fourShot_ = 0;
	startCountdown(kShootDelaySeconds);
}

void App::startGame()
{
	stopMovie();
	score_ = 0;
	newBest_ = false;
	game_ = GameState::Playing;
	gameEnds_ = Clock::now() + std::chrono::milliseconds(static_cast<int>(kGameSeconds * 1000));
	const float speed = difficulty_ == 0 ? 0.0f : (difficulty_ == 1 ? 0.08f : 0.16f) * std::max(frameWidth_, 640);
	for (int i = 0; i < static_cast<int>(geckos_.size()); ++i)
	{
		Gecko& gecko = geckos_[i];
		const double angle = (std::rand() % 360) * CV_PI / 180.0;
		gecko.velocity = cv::Point2f(static_cast<float>(std::cos(angle) * speed), static_cast<float>(std::sin(angle) * speed));
		gecko.respawnPending = false;
		respawnGecko(gecko);
	}
}

bool App::onPerson(const cv::Rect& area) const
{
	const cv::Rect inside = area & cv::Rect(0, 0, personAlpha_.cols, personAlpha_.rows);
	if (personAlpha_.empty() || inside.area() == 0)
		return false;
	return cv::mean(personAlpha_(inside))[0] > 20.0;
}

void App::respawnGecko(Gecko& gecko)
{
	// Somewhere the player has to reach for, not on top of them.
	const int roomX = std::max(1, frameWidth_ - gecko.sprite->width());
	const int roomY = std::max(1, frameHeight_ - gecko.sprite->height());
	for (int attempt = 0; attempt < 30; ++attempt)
	{
		gecko.position = cv::Point2f(static_cast<float>(std::rand() % roomX), static_cast<float>(std::rand() % roomY));
		if (!onPerson(cv::Rect(static_cast<int>(gecko.position.x), static_cast<int>(gecko.position.y),
			gecko.sprite->width(), gecko.sprite->height())))
			break;
	}
	gecko.sprite->moveTo(static_cast<int>(gecko.position.x), static_cast<int>(gecko.position.y));
	gecko.sprite->show();
	gecko.wasVisible = true;
}

void App::takePicture(const cv::Mat& bgr)
{
	if (bgr.empty())
		return;
	std::error_code ec;
	std::filesystem::create_directories(outputDir_, ec);
	std::filesystem::path file = outputDir_ / ("photo-" + timestamp() + ".png");
	for (int i = 2; std::filesystem::exists(file, ec); ++i) // two in the same second
		file = outputDir_ / ("photo-" + timestamp() + "-" + std::to_string(i) + ".png");
	std::vector<unsigned char> png;
	if (!cv::imencode(".png", bgr, png) || !writeFile(file, png))
	{
		toast("照片存不進 " + pathToUtf8(outputDir_));
		return;
	}
	if (saveCameraCopy_ && !mirrored_.empty())
	{
		std::vector<unsigned char> raw;
		if (cv::imencode(".png", mirrored_, raw))
			writeFile(file.parent_path() / (file.stem().string() + "-camera.png"), raw);
	}
	Photo photo;
	photo.file = file;
	uploadThumbnail(photo.thumbnail, bgr, 160);
	photos_.push_front(std::move(photo));
	while (photos_.size() > kMaxPhotos)
		photos_.pop_back();
	sound_.play(Sound::kShutter);
	flashAt_ = Clock::now();
	toast("已存檔：" + pathToUtf8(file.filename()));
}

void App::toggleRecording()
{
	if (recording_)
	{
		writer_.release();
		recording_ = false;
		toast("影片已存檔：" + pathToUtf8(pathFromUtf8(recordingFile_).filename()));
		return;
	}
	if (shown_.empty())
		return;
	std::error_code ec;
	std::filesystem::create_directories(outputDir_, ec);
	const Codec& codec = kCodecs[codecIndex_];
	const std::filesystem::path file = outputDir_ / ("video-" + timestamp() + codec.extension);
	double fps = camera_.measuredFps() > 1.0 ? camera_.measuredFps() : 30.0;
	if (previewInterval_ > 0.0)
		fps = std::min(fps, 1.0 / previewInterval_);
	writer_.open(pathToUtf8(file), cv::VideoWriter::fourcc(codec.fourcc[0], codec.fourcc[1], codec.fourcc[2],
		codec.fourcc[3]), fps, shown_.size());
	if (!writer_.isOpened())
	{
		toast(std::string("無法用 ") + codec.label + " 錄影，換成 Motion JPEG 試試");
		return;
	}
	recording_ = true;
	recordSize_ = shown_.size();
	recordedFrames_ = 0;
	recordingStart_ = Clock::now();
	recordingFile_ = pathToUtf8(file);
	log("Recording to " + recordingFile_);
}

void App::toggleFullscreen()
{
	fullscreen_ = !fullscreen_;
	if (setFullscreen_)
		setFullscreen_(fullscreen_);
	if (fullscreen_)
		toast("全螢幕：空白鍵拍照，Esc 離開");
}

void App::resetEffects()
{
	// Back to how the booth starts: mirrored, nothing else on.
	mirror_ = true;
	backgroundMode_ = BackgroundMode::Off;
	faceSticker_ = -1;
	for (Placed& sticker : placed_)
		sticker.on = false;
	look_ = Look::None;
	adjust_ = Adjustments();
	frameStyle_ = FrameStyle::None;
	dateStamp_ = false;
	gray_ = tint_ = skinMark_ = colorMark_ = false;
	game_ = GameState::Idle;
	stopMovie();
	if (prankOn_)
		togglePrank();
	warp_ = Warp::None;
	showBlocks_ = false;
	debugView_ = 0;
	frozen_ = false;
	drawer_ = Drawer::None;
	toast("已恢復預設");
}

void App::copyToClipboard(const cv::Mat& bgr)
{
	if (!bgr.empty() && bgr.type() == CV_8UC3 && copyBgrToClipboard(bgr.data, bgr.cols, bgr.rows, bgr.step))
		toast("已複製到剪貼簿");
	else
		toast("無法複製到剪貼簿");
}

// ---------------------------------------------------------------------------
// Settings

void App::loadSettings()
{
	if (!settings_.load(home_ / "settings.ini"))
		return;
	const Settings& s = settings_;
	request_.index = std::max(0, s.getInt("camera.index", request_.index));
	sizeIndex_ = std::min(std::max(s.getInt("camera.size", sizeIndex_), 0), 2);
	backendIndex_ = std::max(0, s.getInt("camera.backend", backendIndex_));
	request_.preferMjpg = s.getBool("camera.mjpg", request_.preferMjpg);
	fpsIndex_ = std::clamp(s.getInt("camera.fps", fpsIndex_), 0, 3);
	prankMode_ = static_cast<PrankMode>(std::clamp(s.getInt("prank.mode", 0), 0, 3));
	singAlong_ = s.getBool("prank.sing", singAlong_);
	warp_ = static_cast<Warp>(std::clamp(s.getInt("prank.warp", 0), 0, static_cast<int>(Warp::Count) - 1));
	styler_.setUseGpu(s.getBool("gpu", styler_.useGpu()));
	previewInterval_ = s.getFloat("camera.interval", static_cast<float>(previewInterval_));
	mirror_ = s.getBool("mirror", mirror_);
	backgroundMode_ = static_cast<BackgroundMode>(std::min(std::max(s.getInt("background.mode", 0), 0), 2));
	const int cutOut = s.getInt("background.cutout", static_cast<int>(cutOut_));
	if (cutOut != static_cast<int>(CutOut::Ai) || ai_.segmenterReady())
		cutOut_ = static_cast<CutOut>(std::min(std::max(cutOut, 0), 2));
	const std::string backgroundName = s.getString("background.picture", "");
	for (std::size_t i = 0; i < backgrounds_.size(); ++i)
		if (backgrounds_[i].name == backgroundName)
			backgroundIndex_ = static_cast<int>(i);
	faceSticker_ = std::min(std::max(s.getInt("sticker.face", -1), -1), FaceStickers::kCount - 1);
	dragonBallIndex_ = std::min(std::max(s.getInt("sticker.dragonball.star", dragonBallIndex_), 0), 6);
	placed_[1].bgra = dragonBalls_[dragonBallIndex_];
	uploadThumbnail(placed_[1].thumbnail, placed_[1].bgra, 96);
	boxColor_[0] = s.getFloat("sticker.box.r", boxColor_[0]);
	boxColor_[1] = s.getFloat("sticker.box.g", boxColor_[1]);
	boxColor_[2] = s.getFloat("sticker.box.b", boxColor_[2]);
	refreshBoxSticker();
	for (Placed& sticker : placed_)
	{
		const std::string key = "sticker." + sticker.name + ".";
		sticker.on = s.getBool(key + "on", sticker.on);
		sticker.center.x = s.getFloat(key + "x", sticker.center.x);
		sticker.center.y = s.getFloat(key + "y", sticker.center.y);
		sticker.width = s.getFloat(key + "width", sticker.width);
	}
	look_ = static_cast<Look>(std::min(std::max(s.getInt("look", 0), 0), 4));
	adjust_.brightness = s.getInt("adjust.brightness", 0);
	adjust_.contrast = s.getFloat("adjust.contrast", 1.0f);
	adjust_.saturation = s.getFloat("adjust.saturation", 1.0f);
	adjust_.vignette = s.getBool("adjust.vignette", false);
	frameStyle_ = static_cast<FrameStyle>(std::min(std::max(s.getInt("frame", 0), 0), 3));
	dateStamp_ = s.getBool("frame.date", false);
	gray_ = s.getBool("filter.gray", false);
	tint_ = s.getBool("filter.tint", false);
	tintColor_[0] = s.getFloat("filter.tint.r", tintColor_[0]);
	tintColor_[1] = s.getFloat("filter.tint.g", tintColor_[1]);
	tintColor_[2] = s.getFloat("filter.tint.b", tintColor_[2]);
	colorRange_->yMin = s.getInt("range.ymin", colorRange_->yMin);
	colorRange_->yMax = s.getInt("range.ymax", colorRange_->yMax);
	colorRange_->uMin = s.getInt("range.umin", colorRange_->uMin);
	colorRange_->uMax = s.getInt("range.umax", colorRange_->uMax);
	colorRange_->vMin = s.getInt("range.vmin", colorRange_->vMin);
	colorRange_->vMax = s.getInt("range.vmax", colorRange_->vMax);
	wavePhoto_ = s.getBool("photo.wave", wavePhoto_);
	saveCameraCopy_ = s.getBool("photo.cameraCopy", saveCameraCopy_);
	codecIndex_ = std::min(std::max(s.getInt("record.codec", 0), 0), 2);
	difficulty_ = std::min(std::max(s.getInt("game.difficulty", difficulty_), 0), 2);
	for (int i = 0; i < 3; ++i)
		best_[i] = s.getInt("game.best." + std::to_string(i), 0);
	threshold_ = s.getInt("detect.threshold", threshold_);
	adaptation_ = s.getFloat("detect.adaptation", adaptation_);
	effects_.detector().setThreshold(threshold_);
	effects_.detector().setAdaptationRate(adaptation_);
	sound_.setEnabled(s.getBool("sound", true));
	std::snprintf(musicPath_, sizeof(musicPath_), "%s", s.getString("music.file", musicPath_).c_str());
	volume_ = s.getFloat("music.volume", volume_);
	loop_ = s.getBool("music.loop", loop_);
}

void App::saveSettings()
{
	Settings& s = settings_;
	s.set("camera.index", request_.index);
	s.set("camera.size", sizeIndex_);
	s.set("camera.backend", backendIndex_);
	s.set("camera.mjpg", request_.preferMjpg);
	s.set("camera.fps", fpsIndex_);
	s.set("prank.mode", static_cast<int>(prankMode_));
	s.set("prank.sing", singAlong_);
	s.set("prank.warp", static_cast<int>(warp_));
	s.set("gpu", styler_.useGpu());
	s.set("camera.interval", static_cast<float>(previewInterval_));
	s.set("mirror", mirror_);
	s.set("background.mode", static_cast<int>(backgroundMode_));
	s.set("background.cutout", static_cast<int>(cutOut_));
	if (!backgrounds_.empty())
		s.set("background.picture", backgrounds_[std::min<std::size_t>(backgroundIndex_, backgrounds_.size() - 1)].name);
	s.set("sticker.face", faceSticker_);
	s.set("sticker.dragonball.star", dragonBallIndex_);
	s.set("sticker.box.r", boxColor_[0]);
	s.set("sticker.box.g", boxColor_[1]);
	s.set("sticker.box.b", boxColor_[2]);
	for (const Placed& sticker : placed_)
	{
		const std::string key = "sticker." + sticker.name + ".";
		s.set(key + "on", sticker.on);
		s.set(key + "x", sticker.center.x);
		s.set(key + "y", sticker.center.y);
		s.set(key + "width", sticker.width);
	}
	s.set("look", static_cast<int>(look_));
	s.set("adjust.brightness", adjust_.brightness);
	s.set("adjust.contrast", adjust_.contrast);
	s.set("adjust.saturation", adjust_.saturation);
	s.set("adjust.vignette", adjust_.vignette);
	s.set("frame", static_cast<int>(frameStyle_));
	s.set("frame.date", dateStamp_);
	s.set("filter.gray", gray_);
	s.set("filter.tint", tint_);
	s.set("filter.tint.r", tintColor_[0]);
	s.set("filter.tint.g", tintColor_[1]);
	s.set("filter.tint.b", tintColor_[2]);
	s.set("range.ymin", colorRange_->yMin);
	s.set("range.ymax", colorRange_->yMax);
	s.set("range.umin", colorRange_->uMin);
	s.set("range.umax", colorRange_->uMax);
	s.set("range.vmin", colorRange_->vMin);
	s.set("range.vmax", colorRange_->vMax);
	s.set("photo.wave", wavePhoto_);
	s.set("photo.cameraCopy", saveCameraCopy_);
	s.set("record.codec", codecIndex_);
	s.set("game.difficulty", difficulty_);
	for (int i = 0; i < 3; ++i)
		s.set("game.best." + std::to_string(i), best_[i]);
	s.set("detect.threshold", threshold_);
	s.set("detect.adaptation", adaptation_);
	s.set("sound", sound_.enabled());
	s.set("music.file", std::string(musicPath_));
	s.set("music.volume", volume_);
	s.set("music.loop", loop_);
	if (!s.save(home_ / "settings.ini"))
		std::cerr << "Could not save " << pathToUtf8(home_ / "settings.ini") << std::endl;
}

// ---------------------------------------------------------------------------
// UI

void App::update()
{
	scale_ = ImGui::GetStyle().FontScaleDpi;
	runTimers();
	if (movieOn() && movie_.failed())
	{
		toast("打不開這部影片：" + clips_[clipIndex_].name);
		stopMovie();
	}
	followMovieAudio();

	if (!staged_ && !cameraStarted_)
	{
		cameraStarted_ = true;
		openCamera(); // a photo booth starts with the camera on
	}
	// Frozen: keep showing the last picture. Otherwise honour the preview rate.
	const bool due = previewInterval_ <= 0.0 ||
		std::chrono::duration<double>(Clock::now() - lastProcessed_).count() >= previewInterval_;
	if (!frozen_ && due)
	{
		if (staged_)
			processFrame(stagedFrame_);
		else if (camera_.state() == Camera::State::Running && camera_.latest(captured_, seenFrame_))
			processFrame(captured_);
	}
	else if (frozen_ && shootNextFrame_)
	{
		shootNextFrame_ = false;
		takePicture(shown_); // the frozen picture, as it is
	}

	const Camera::State state = camera_.state();
	if (state != lastCameraState_)
	{
		if (state == Camera::State::Failed)
			log(camera_.error());
		lastCameraState_ = state;
	}

	// Keyboard: Space shoots, F11 toggles full screen, Esc leaves it.
	const ImGuiIO& io = ImGui::GetIO();
	if (!io.WantTextInput)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && !countdown_ && (staged_ || state == Camera::State::Running))
		{
			if (frozen_)
				shootNextFrame_ = true;
			else
				startCountdown(kShootDelaySeconds);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_F11, false))
			toggleFullscreen();
		if (ImGui::IsKeyPressed(ImGuiKey_F9, false))
			togglePrank();
		if (ImGui::IsKeyPressed(ImGuiKey_F10, false))
			warp_ = static_cast<Warp>((static_cast<int>(warp_) + 1) % static_cast<int>(Warp::Count));
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && fullscreen_)
			toggleFullscreen();
	}

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::Begin("booth", nullptr,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::PushFont(fonts_.text, kTextSize);

	if (!fullscreen_)
		drawTopBar();

	// The picture takes whatever the controls below leave (heights from the last frame).
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const float below = (drawer_ != Drawer::None ? drawerHeight_ + ImGui::GetStyle().ItemSpacing.y : 0.0f) +
		(bottomHeight_ > 0.0f ? bottomHeight_ : 160.0f * scale_);
	drawVideo(ImVec2(avail.x, std::max(120.0f * scale_, avail.y - below)));

	const float bottomStart = ImGui::GetCursorPosY();
	drawDrawer();
	const float barStart = ImGui::GetCursorPosY();
	drawBottomBar();
	if (!fullscreen_)
		drawPhotoStrip();
	bottomHeight_ = ImGui::GetCursorPosY() - barStart;
	(void)bottomStart;
	drawRegionPicker();

	ImGui::PopFont();
	ImGui::End();

	if (advanced_ && !fullscreen_)
		drawAdvanced();
}

void App::drawTopBar()
{
	ImGui::PushFont(fonts_.bold, kTitleSize);
	ImGui::TextUnformatted("視訊互動拍照亭");
	ImGui::PopFont();
	ImGui::SameLine(0, 24.0f * scale_);

	const Camera::State state = staged_ ? Camera::State::Running : camera_.state();
	const bool cameraOk = state == Camera::State::Running;
	const webcam::ChangeDetector& detector = effects_.detector();
	const bool learnStep = !ai_.segmenterReady(); // with the AI there is nothing to learn
	const bool ready = !learnStep || backgroundReady();

	// 1. Camera
	stepBadge(1, cameraOk, !cameraOk, scale_);
	ImGui::AlignTextToFramePadding();
	if (state == Camera::State::Opening)
		ImGui::TextUnformatted("相機開啟中...");
	else if (cameraOk)
		ImGui::TextUnformatted("相機");
	else
	{
		const bool closed = state == Camera::State::Closed;
		ImGui::TextUnformatted(closed ? "相機已關閉" : "相機打不開");
		ImGui::SameLine();
		if (ImGui::Button(closed ? "開啟相機" : "重試"))
			openCamera();
		if (!closed)
			hint("可能被其他程式占用，或要選別台相機：到「進階設定 > 相機」。");
	}
	ImGui::SameLine(0, 20.0f * scale_);

	// 2. Background, only without the AI.
	int step = 2;
	if (learnStep)
	{
		stepBadge(step++, ready, cameraOk && !ready, scale_);
		if (learnPending_ || detector.isLearning())
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("記住背景中，請留在畫面外...");
		}
		else if (ready)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("背景");
			ImGui::SameLine();
			if (ImGui::SmallButton("重新記住"))
				startLearning();
		}
		else
		{
			ImGui::BeginDisabled(!cameraOk);
			pushPrimary();
			if (ImGui::Button("記住背景"))
				startLearning();
			popPrimary();
			ImGui::EndDisabled();
			hint("按下後 3 秒內離開畫面，讓程式記住空的場景。揮手拍照、抓壁虎都需要它。");
		}
		ImGui::SameLine(0, 20.0f * scale_);
	}

	// 3. Play
	stepBadge(step, false, cameraOk && ready, scale_);
	ImGui::AlignTextToFramePadding();
	if (cameraOk && ready)
		ImGui::TextUnformatted("開始玩：選下方玩法，或揮手碰右下角的相機拍照");
	else
		ImGui::TextDisabled("開始玩");

	// Right side
	const ImGuiStyle& style = ImGui::GetStyle();
	const auto width = [&style](const char* text) { return ImGui::CalcTextSize(text).x + style.FramePadding.x * 2.0f; };
	float right = ImGui::GetWindowWidth() - style.WindowPadding.x - width("進階設定") - width("全螢幕") -
		width(sound_.enabled() ? "音效：開" : "音效：關") - style.ItemSpacing.x * 2.0f;
	if (music_.loaded())
		right -= width("暫停音樂") + style.ItemSpacing.x;
	ImGui::SameLine(std::max(ImGui::GetCursorPosX(), right));
	if (music_.loaded())
	{
		if (ImGui::Button(music_.playing() ? "暫停音樂" : "播放音樂"))
			music_.playing() ? music_.pause() : music_.play();
		ImGui::SameLine();
	}
	if (ImGui::Button(sound_.enabled() ? "音效：開" : "音效：關"))
		sound_.setEnabled(!sound_.enabled());
	ImGui::SameLine();
	if (ImGui::Button("全螢幕"))
		toggleFullscreen();
	ImGui::SetItemTooltip("F11 切換，Esc 離開；全螢幕時按空白鍵拍照");
	ImGui::SameLine();
	if (ImGui::Button("進階設定"))
		advanced_ = !advanced_;
	ImGui::Separator();
}

void App::drawVideo(ImVec2 size)
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(12, 12, 12, 255));
	ImGui::BeginChild("video", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 min = ImGui::GetWindowPos();
	const ImVec2 max(min.x + size.x, min.y + size.y);
	ImDrawList* draw = ImGui::GetWindowDrawList();

	const bool cameraOk = staged_ || camera_.state() == Camera::State::Running;
	if (video_.valid() && cameraOk)
	{
		const ImVec2 fitted = video_.fit(size);
		const ImVec2 corner(min.x + (size.x - fitted.x) * 0.5f, min.y + (size.y - fitted.y) * 0.5f);
		const ImVec2 far(corner.x + fitted.x, corner.y + fitted.y);
		draw->AddImage(video_.id(), corner, far);
		handleVideoMouse(corner, fitted);
		drawOverlay(corner, far);
	}
	else
	{
		const char* text = camera_.state() == Camera::State::Opening ? "相機開啟中..." : "沒有相機畫面";
		drawCentered(draw, fonts_.text, 28.0f * scale_, ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f),
			IM_COL32(200, 200, 200, 255), text);
		if (camera_.state() == Camera::State::Failed)
			drawCentered(draw, fonts_.text, kTextSize * scale_,
				ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f + 40.0f * scale_), IM_COL32(230, 120, 100, 255),
				camera_.error().c_str());
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void App::handleVideoMouse(ImVec2 corner, ImVec2 fitted)
{
	// Drag placed stickers to move them; the wheel resizes the one under the mouse.
	ImGui::SetCursorScreenPos(corner);
	ImGui::InvisibleButton("picture", fitted);
	if (frameWidth_ == 0 || fitted.x <= 0 || movieOn()) // placed stickers are not on a video
		return;
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const cv::Point2f at((mouse.x - corner.x) / fitted.x, (mouse.y - corner.y) / fitted.y);
	const float aspect = static_cast<float>(frameWidth_) / frameHeight_;
	const auto hit = [&]() -> int {
		for (int i = static_cast<int>(placed_.size()) - 1; i >= 0; --i)
		{
			const Placed& s = placed_[i];
			if (!s.on || s.bgra.empty())
				continue;
			const float halfW = s.width * 0.5f;
			const float halfH = s.width * s.bgra.rows / s.bgra.cols * aspect * 0.5f;
			if (std::fabs(at.x - s.center.x) <= halfW && std::fabs(at.y - s.center.y) <= halfH)
				return i;
		}
		return -1;
	};

	if (ImGui::IsItemActivated())
	{
		dragging_ = hit();
		if (dragging_ >= 0)
			dragOffset_ = at - placed_[dragging_].center;
	}
	if (ImGui::IsItemActive() && dragging_ >= 0)
	{
		Placed& s = placed_[dragging_];
		s.center.x = std::min(std::max(at.x - dragOffset_.x, 0.0f), 1.0f);
		s.center.y = std::min(std::max(at.y - dragOffset_.y, 0.0f), 1.0f);
		if (frozen_ && !mirrored_.empty())
			processFrame(captured_.empty() ? stagedFrame_ : captured_);
	}
	if (!ImGui::IsItemActive())
		dragging_ = -1;
	if (ImGui::IsItemHovered())
	{
		const int over = hit();
		if (over >= 0)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			const float wheel = ImGui::GetIO().MouseWheel;
			if (wheel != 0.0f)
				placed_[over].width = std::min(std::max(placed_[over].width * std::pow(1.1f, wheel), 0.02f), 1.0f);
		}
	}
}

void App::drawOverlay(ImVec2 min, ImVec2 max)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
	const Clock::time_point now = Clock::now();
	const ImU32 white = IM_COL32(255, 255, 255, 255);
	const ImU32 shade = IM_COL32(0, 0, 0, 150);
	const float pad = 10.0f * scale_;

	const auto banner = [&](float y, const std::string& text, ImU32 color, float size) {
		const ImVec2 extent = fonts_.text->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
		const ImVec2 a(center.x - extent.x * 0.5f - pad, y);
		const ImVec2 b(center.x + extent.x * 0.5f + pad, y + extent.y + pad);
		draw->AddRectFilled(a, b, shade, 8.0f * scale_);
		draw->AddText(fonts_.text, size, ImVec2(a.x + pad, a.y + pad * 0.5f), color, text.c_str());
	};
	const float bannerSize = kTextSize * 1.2f * scale_;

	// Learning the background.
	const webcam::ChangeDetector& detector = effects_.detector();
	if (learnPending_)
	{
		const int left = static_cast<int>(std::ceil(std::chrono::duration<double>(learnAt_ - now).count()));
		draw->AddRectFilled(min, max, IM_COL32(0, 0, 0, 90));
		drawCentered(draw, fonts_.bold, kHugeSize * scale_, center, white, std::to_string(std::max(1, left)).c_str());
		banner(center.y + kHugeSize * 0.6f * scale_, "請離開畫面，讓程式記住空的場景", white, bannerSize);
	}
	else if (detector.isLearning())
		banner(center.y, "記住背景中...", white, bannerSize);
	else if (needsLearnedBackground() && !backgroundReady())
		banner(min.y + pad, "這個玩法要先「記住背景」：按上方按鈕後離開畫面", IM_COL32(255, 210, 120, 255), bannerSize);

	// Pranks: a reminder that it is on, and what it still needs. Only here,
	// never in the output window the other person sees.
	if (prankOn_)
	{
		const char* text = "惡搞中（F9 結束）";
		const float size = kTextSize * scale_;
		const ImVec2 extent = fonts_.bold->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
		const ImVec2 a(min.x + pad, max.y - extent.y - pad * 3);
		draw->AddRectFilled(a, ImVec2(a.x + extent.x + pad * 2, max.y - pad), IM_COL32(200, 40, 120, 220), 8.0f * scale_);
		draw->AddText(fonts_.bold, size, ImVec2(a.x + pad, a.y + pad * 0.5f), white, text);
		const bool video = prankMode_ == PrankMode::TheirFaceOnVideo;
		if (video && !movieOn())
			banner(min.y + pad, "先在「惡搞」選一部影片（把影片拖進視窗）", IM_COL32(255, 210, 120, 255), bannerSize);
		else if (!video && faces_.empty())
			banner(min.y + pad, "看著鏡頭，讓程式找到你的臉", IM_COL32(255, 210, 120, 255), bannerSize);
		else if (prankMode_ != PrankMode::JustMe && !theirFace_)
			banner(min.y + pad,
				grabber_.lost() ? "對方的視窗被縮小或關掉了" : "還沒找到對方的臉：選對方的視窗，或把截圖拖進來",
				IM_COL32(255, 210, 120, 255), bannerSize);
		else if (singAlong_ && !music_.loaded())
			banner(min.y + pad, "把 mp3 拖進視窗，下巴就會跟著音樂動", IM_COL32(200, 200, 200, 255), bannerSize);
	}

	// Starring in a video: what is missing for the paste.
	if (movieOn())
	{
		if (!movieFrame_)
			banner(center.y, "影片載入中...", white, bannerSize);
		else if (prankOn_)
		{
			// The prank's own hints, above, take this place.
		}
		else if (faces_.empty())
			banner(min.y + pad, "看著鏡頭，讓程式找到你的臉", IM_COL32(255, 210, 120, 255), bannerSize);
		else if (movieFaces_ == 0)
			banner(min.y + pad, "這一段影片裡沒有看得到的臉", IM_COL32(200, 200, 200, 255), bannerSize);
	}

	// Countdown, with the four-shot progress.
	if (countdown_)
	{
		const int left = static_cast<int>(std::ceil(std::chrono::duration<double>(shootAt_ - now).count()));
		drawCentered(draw, fonts_.bold, kHugeSize * 1.3f * scale_, center, white, std::to_string(std::max(1, left)).c_str());
		if (fourShot_ >= 0)
			banner(center.y + kHugeSize * 0.75f * scale_, "四格拍貼：第 " + std::to_string(fourShot_ + 1) + " / 4 張", white,
				bannerSize);
	}

	// Flash after a picture.
	const double sinceFlash = std::chrono::duration<double>(now - flashAt_).count();
	if (sinceFlash < kFlashSeconds)
		draw->AddRectFilled(min, max, IM_COL32(255, 255, 255, static_cast<int>(220 * (1.0 - sinceFlash / kFlashSeconds))));

	// The game.
	if (game_ == GameState::Playing)
	{
		const int left = std::max(0, static_cast<int>(std::ceil(std::chrono::duration<double>(gameEnds_ - now).count())));
		const std::string text = "抓到 " + std::to_string(score_) + " 隻    剩 " + std::to_string(left) + " 秒";
		const float size = kTitleSize * scale_;
		const ImVec2 extent = fonts_.bold->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
		draw->AddRectFilled(ImVec2(min.x + pad, min.y + pad), ImVec2(min.x + extent.x + pad * 3, min.y + extent.y + pad * 2),
			shade, 8.0f * scale_);
		draw->AddText(fonts_.bold, size, ImVec2(min.x + pad * 2, min.y + pad * 1.5f),
			left <= 5 ? IM_COL32(255, 120, 100, 255) : IM_COL32(255, 220, 120, 255), text.c_str());
	}
	else if (game_ == GameState::Over)
	{
		draw->AddRectFilled(min, max, IM_COL32(0, 0, 0, 120));
		drawCentered(draw, fonts_.bold, kHugeSize * 0.6f * scale_, ImVec2(center.x, center.y - 50.0f * scale_), white, "時間到！");
		const std::string result = "抓到 " + std::to_string(score_) + " 隻" +
			(newBest_ ? "，新紀錄！" : "（最高 " + std::to_string(best_[difficulty_]) + " 隻）");
		banner(center.y + 10.0f * scale_, result, newBest_ ? IM_COL32(255, 220, 120, 255) : white, kTitleSize * scale_);
		banner(center.y + 60.0f * scale_, "到「抓壁虎」按「再玩一次」", IM_COL32(200, 200, 200, 255), bannerSize * 0.85f);
	}

	if (frozen_)
	{
		const char* text = "已暫停";
		const float size = kTitleSize * scale_;
		const ImVec2 extent = fonts_.bold->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
		const ImVec2 a(max.x - extent.x - pad * 3, min.y + pad);
		draw->AddRectFilled(a, ImVec2(max.x - pad, a.y + extent.y + pad), shade, 8.0f * scale_);
		draw->AddText(fonts_.bold, size, ImVec2(a.x + pad, a.y + pad * 0.5f), IM_COL32(120, 200, 255, 255), text);
	}
	if (recording_)
	{
		const ImVec2 dot(max.x - 70.0f * scale_, min.y + 22.0f * scale_ + (frozen_ ? 40.0f * scale_ : 0.0f));
		draw->AddCircleFilled(dot, 8.0f * scale_, IM_COL32(230, 40, 40, 255));
		draw->AddText(fonts_.bold, kTextSize * scale_, ImVec2(dot.x + 14.0f * scale_, dot.y - 10.0f * scale_), white, "REC");
	}

	if (!toast_.empty() && std::chrono::duration<double>(now - toastAt_).count() < kToastSeconds)
		banner(max.y - 60.0f * scale_, toast_, white, bannerSize);
}

bool App::bigToggle(const char* label, bool active, bool marked, ImVec2 size)
{
	if (active)
		pushPrimary();
	const bool clicked = ImGui::Button(label, size);
	if (active)
		popPrimary();
	if (marked)
	{
		const ImVec2 corner = ImGui::GetItemRectMax();
		ImGui::GetWindowDrawList()->AddCircleFilled(
			ImVec2(corner.x - 9.0f * scale_, ImGui::GetItemRectMin().y + 9.0f * scale_), 4.5f * scale_,
			ImGui::GetColorU32(active ? ImVec4(0.1f, 0.1f, 0.1f, 1.0f) : kAccent));
	}
	return clicked;
}

bool App::thumbnailButton(const char* id, const Texture& texture, float height, bool selected)
{
	if (!texture.valid())
		return false;
	// Wide pictures shrink to fit, so a row of thumbnails stays compact.
	const ImVec2 size = texture.fit(ImVec2(height * 1.5f, height));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f * scale_, 3.0f * scale_));
	if (selected)
		ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
	const bool clicked = ImGui::ImageButton(id, texture.id(), size);
	if (selected)
		ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	return clicked;
}

// A snapshot of the other person's window, to drag a box round them on.
void App::drawRegionPicker()
{
	const char* title = "框選對方";
	if (openRegion_)
	{
		ImGui::OpenPopup(title);
		openRegion_ = false;
	}
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;
	ImGui::TextUnformatted("在畫面上拖出一個框，框住對方的臉（例如視訊裡對方那一格）。框越小越快，也越不會抓到你自己。");

	const ImVec2 size = regionShot_.fit(ImVec2(viewport->WorkSize.x * 0.7f, viewport->WorkSize.y * 0.6f));
	const ImVec2 corner = ImGui::GetCursorScreenPos();
	ImGui::Image(regionShot_.id(), size);
	ImGui::SetCursorScreenPos(corner);
	ImGui::InvisibleButton("##region", size);
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const cv::Point2f at(std::clamp((mouse.x - corner.x) / std::max(1.0f, size.x), 0.0f, 1.0f),
		std::clamp((mouse.y - corner.y) / std::max(1.0f, size.y), 0.0f, 1.0f));
	if (ImGui::IsItemActivated())
	{
		regionFrom_ = regionTo_ = at;
		regionDragging_ = true;
	}
	if (regionDragging_ && ImGui::IsItemActive())
		regionTo_ = at;
	if (ImGui::IsItemDeactivated())
		regionDragging_ = false;

	// Everything outside the box dimmed, the box outlined.
	const cv::Rect2f box(std::min(regionFrom_.x, regionTo_.x), std::min(regionFrom_.y, regionTo_.y),
		std::fabs(regionTo_.x - regionFrom_.x), std::fabs(regionTo_.y - regionFrom_.y));
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const ImVec2 end(corner.x + size.x, corner.y + size.y);
	const ImVec2 a(corner.x + box.x * size.x, corner.y + box.y * size.y);
	const ImVec2 b(corner.x + (box.x + box.width) * size.x, corner.y + (box.y + box.height) * size.y);
	const ImU32 dim = IM_COL32(0, 0, 0, 140);
	draw->AddRectFilled(corner, ImVec2(end.x, a.y), dim);
	draw->AddRectFilled(ImVec2(corner.x, b.y), end, dim);
	draw->AddRectFilled(ImVec2(corner.x, a.y), ImVec2(a.x, b.y), dim);
	draw->AddRectFilled(ImVec2(b.x, a.y), ImVec2(end.x, b.y), dim);
	draw->AddRect(a, b, ImGui::GetColorU32(kAccent), 0.0f, 0, 2.0f * scale_);

	// Too small a box cannot hold a face the detector would find.
	const bool usable = box.width * size.x >= 24.0f * scale_ && box.height * size.y >= 24.0f * scale_;
	ImGui::BeginDisabled(!usable);
	pushPrimary();
	if (ImGui::Button("用這個範圍"))
	{
		watchWindow(regionWindow_, box);
		ImGui::CloseCurrentPopup();
	}
	popPrimary();
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("整個視窗"))
	{
		watchWindow(regionWindow_, cv::Rect2f(0.0f, 0.0f, 1.0f, 1.0f));
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("取消"))
		ImGui::CloseCurrentPopup();
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("（%s）", windows_[regionWindow_].title.c_str());
	ImGui::EndPopup();
}

// The videos to pick from, and the controls of the one playing.
void App::drawClips(float thumb)
{
	for (std::size_t i = 0; i < clips_.size(); ++i)
	{
		if (i > 0)
			ImGui::SameLine();
		const std::string id = "clip" + std::to_string(i);
		if (thumbnailButton(id.c_str(), clips_[i].thumbnail, thumb, clipIndex_ == static_cast<int>(i)))
		{
			if (clipIndex_ == static_cast<int>(i))
				stopMovie();
			else
				startMovie(static_cast<int>(i));
		}
		ImGui::SetItemTooltip("%s%s", clips_[i].name.c_str(), clips_[i].audio.empty() ? "（沒有配樂）" : "（有配樂）");
	}
	if (clips_.empty())
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("還沒有影片");
	}
	if (!movieOn())
		return;
	ImGui::SameLine(0, 24.0f * scale_);
	if (ImGui::Button(movie_.paused() ? "繼續播放" : "暫停影片"))
		pauseMovie(!movie_.paused());
	ImGui::SameLine();
	if (ImGui::Button("從頭開始"))
		restartMovie();
	ImGui::SameLine();
	if (ImGui::Button("停止"))
		stopMovie();
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	const int at = static_cast<int>(movie_.seconds());
	const int length = static_cast<int>(movie_.duration());
	if (length > 0)
		ImGui::Text("%d:%02d / %d:%02d", at / 60, at % 60, length / 60, length % 60);
	ImGui::SameLine(0, 16.0f * scale_);
	ImGui::Text("貼上 %d 張臉%s", pastedFaces_, clipAudio_ ? "，播放配樂中" : "");
}

void App::drawDrawer()
{
	if (drawer_ == Drawer::None)
	{
		drawerHeight_ = 0.0f;
		return;
	}
	// Controls inside get a lighter frame color so they stand out from the drawer.
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.30f, 0.31f, 0.35f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.36f, 0.37f, 0.42f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale_, 8.0f * scale_));
	ImGui::BeginChild("drawer", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
		ImGuiWindowFlags_NoScrollbar);
	const float thumb = 60.0f * scale_;
	const auto label = [](const char* text) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", text);
		ImGui::SameLine();
	};

	switch (drawer_)
	{
	case Drawer::Background:
	{
		label("背景");
		const char* modes[] = { "不換", "換背景", "模糊背景" };
		for (int i = 0; i < 3; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			const bool blurNeedsAi = i == 2 && !ai_.segmenterReady();
			ImGui::BeginDisabled(blurNeedsAi);
			if (ImGui::RadioButton(modes[i], static_cast<int>(backgroundMode_) == i))
				backgroundMode_ = static_cast<BackgroundMode>(i);
			ImGui::EndDisabled();
		}
		ImGui::SameLine(0, 30.0f * scale_);
		label("去背方式");
		const char* cutOuts[] = { "AI 自動", "比較背景", "綠幕" };
		for (int i = 0; i < 3; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			ImGui::BeginDisabled(i == 0 && !ai_.segmenterReady());
			if (ImGui::RadioButton(cutOuts[i], static_cast<int>(cutOut_) == i))
				cutOut_ = static_cast<CutOut>(i);
			ImGui::EndDisabled();
		}
		hint("AI 自動：直接找出畫面裡的人，不必先記住背景。\n比較背景：跟記住的空場景比較（要先按「記住背景」）。\n"
			"綠幕：把指定顏色（進階設定 > 顏色範圍）的地方換掉，適合身後有單色布幕時。");
		if (cutOut_ == CutOut::Difference && backgroundMode_ == BackgroundMode::Replace && !backgroundReady())
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("記住背景"))
				startLearning();
		}
		for (std::size_t i = 0; i < backgrounds_.size(); ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			const std::string id = "bg" + std::to_string(i);
			if (thumbnailButton(id.c_str(), backgrounds_[i].thumbnail, thumb,
				backgroundMode_ == BackgroundMode::Replace && backgroundIndex_ == static_cast<int>(i)))
			{
				backgroundIndex_ = static_cast<int>(i);
				backgroundMode_ = BackgroundMode::Replace;
				cutOutByDifference_->setPicture(backgrounds_[i].image);
				cutOutByColor_->setPicture(backgrounds_[i].image);
			}
			ImGui::SetItemTooltip("%s", backgrounds_[i].name.c_str());
		}
		ImGui::SameLine(0, 16.0f * scale_);
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("把圖片拖進視窗就能加入新背景");
		break;
	}

	case Drawer::Stickers:
	{
		label("跟著臉");
		if (!ai_.faceTrackerReady())
			ImGui::TextColored(kWarn, "找不到人臉偵測模型");
		else
		{
			if (ImGui::Button("無", ImVec2(thumb * 0.8f, thumb + 6.0f * scale_)))
				faceSticker_ = -1;
			for (int i = 0; i < FaceStickers::kCount; ++i)
			{
				ImGui::SameLine();
				const std::string id = "face" + std::to_string(i);
				if (thumbnailButton(id.c_str(), faceThumbs_[i], thumb, faceSticker_ == i))
					faceSticker_ = faceSticker_ == i ? -1 : i;
				ImGui::SetItemTooltip("%s", FaceStickers::name(static_cast<FaceStickers::Kind>(i)));
			}
		}
		label("放在畫面上");
		for (std::size_t i = 0; i < placed_.size(); ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			const std::string id = "placed" + std::to_string(i);
			if (thumbnailButton(id.c_str(), placed_[i].thumbnail, thumb * 0.8f, placed_[i].on))
				placed_[i].on = !placed_[i].on;
			if (i == 0)
				ImGui::SetItemTooltip("壁虎");
			else if (i == 1)
				ImGui::SetItemTooltip("龍珠");
			else if (i == 2)
			{
				ImGui::SetItemTooltip("色塊");
				ImGui::SameLine();
				if (ImGui::ColorEdit3("##box", boxColor_, ImGuiColorEditFlags_NoInputs))
					refreshBoxSticker();
			}
			else
				ImGui::SetItemTooltip("%s", placed_[i].name.c_str());
		}
		if (placed_[1].on)
		{
			ImGui::SameLine(0, 16.0f * scale_);
			label("星數");
			for (int i = 0; i < 7; ++i)
			{
				if (i > 0)
					ImGui::SameLine(0, 2.0f * scale_);
				if (ImGui::RadioButton(std::to_string(i + 1).c_str(), dragonBallIndex_ == i))
				{
					dragonBallIndex_ = i;
					placed_[1].bgra = dragonBalls_[i];
					uploadThumbnail(placed_[1].thumbnail, placed_[1].bgra, 96);
				}
			}
		}
		ImGui::TextDisabled("在畫面上拖曳貼圖可以移動，滾輪可以縮放；把有透明背景的 PNG 拖進視窗就能加入新貼圖");
		break;
	}

	case Drawer::Filters:
	{
		label("風格");
		const char* looks[] = { "無", "美肌", "卡通", "素描", "復古" };
		for (int i = 0; i < 5; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			if (ImGui::RadioButton(looks[i], static_cast<int>(look_) == i))
				look_ = static_cast<Look>(i);
		}
		ImGui::SameLine(0, 30.0f * scale_);
		ImGui::Checkbox("鏡像", &mirror_);
		ImGui::SameLine();
		ImGui::Checkbox("黑白", &gray_);
		ImGui::SameLine();
		ImGui::Checkbox("單色調", &tint_);
		ImGui::SameLine();
		ImGui::ColorEdit3("##tint", tintColor_, ImGuiColorEditFlags_NoInputs);
		ImGui::SameLine();
		ImGui::Checkbox("標出膚色", &skinMark_);
		ImGui::SameLine();
		ImGui::Checkbox("標出指定顏色", &colorMark_);
		hint("把落在顏色範圍裡的地方染上顏色。範圍在「進階設定 > 顏色範圍」。");

		const float slider = 130.0f * scale_;
		label("調整");
		ImGui::SetNextItemWidth(slider);
		ImGui::SliderInt("亮度", &adjust_.brightness, -100, 100);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(slider);
		ImGui::SliderFloat("對比", &adjust_.contrast, 0.5f, 2.0f, "%.2f");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(slider);
		ImGui::SliderFloat("飽和度", &adjust_.saturation, 0.0f, 2.0f, "%.2f");
		ImGui::SameLine();
		ImGui::Checkbox("暗角", &adjust_.vignette);
		ImGui::SameLine(0, 30.0f * scale_);
		label("相框");
		const char* frames[] = { "無", "簡單", "拍立得", "底片" };
		ImGui::SetNextItemWidth(110.0f * scale_);
		int frame = static_cast<int>(frameStyle_);
		if (ImGui::Combo("##frame", &frame, frames, 4))
			frameStyle_ = static_cast<FrameStyle>(frame);
		ImGui::SameLine();
		ImGui::Checkbox("日期", &dateStamp_);
		break;
	}

	case Drawer::Game:
	{
		label("難度");
		for (int i = 0; i < 3; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			ImGui::BeginDisabled(game_ == GameState::Playing);
			if (ImGui::RadioButton(kDifficulties[i], difficulty_ == i))
				difficulty_ = i;
			ImGui::EndDisabled();
		}
		hint("簡單：1 隻不動。普通：2 隻慢慢跑。困難：3 隻跑得快。");
		ImGui::SameLine(0, 24.0f * scale_);
		if (game_ == GameState::Playing)
		{
			if (ImGui::Button("結束遊戲"))
				game_ = GameState::Idle;
		}
		else
		{
			pushPrimary();
			if (ImGui::Button(game_ == GameState::Over ? "再玩一次" : "開始（30 秒）"))
				startGame();
			popPrimary();
		}
		ImGui::SameLine(0, 24.0f * scale_);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("最高紀錄：%d 隻", best_[difficulty_]);
		ImGui::TextDisabled("用手去碰壁虎就抓到了，30 秒內抓越多越好。");
		ImGui::SameLine();
		if (!aiTouch() && !backgroundReady())
			ImGui::TextColored(kWarn, "（需要先記住背景）");
		else
			ImGui::Checkbox("顯示偵測到的動作（黑白方塊）", &showBlocks_);
		break;
	}

	case Drawer::Movie:
	{
		label("影片");
		if (!ai_.faceTrackerReady() || !movie_.faceTrackerReady())
		{
			ImGui::TextColored(kWarn, "找不到人臉偵測模型，這個玩法不能用");
			break;
		}
		drawClips(thumb);
		ImGui::TextDisabled("把影片檔（mp4、mov、avi…）拖進視窗就能加入，同名的 mp3 一起拖進來就是配樂。"
			"你的臉會貼到影片裡最大的那張臉上，兩個人一起就貼兩張。");
		hint("點縮圖開始播放，再點一次停止。\n影片裡的人轉成側臉、被擋住或太小時，那幾格會露出原本的臉。\n"
			"拍照、四格拍貼、錄影都照常可以用；貼圖區的「跟著臉」也會戴在你貼上去的臉上。");
		break;
	}

	case Drawer::Prank:
	{
		if (!ai_.faceTrackerReady() || !grabber_.faceTrackerReady())
		{
			ImGui::TextColored(kWarn, "找不到人臉偵測模型，這個玩法不能用");
			break;
		}
		label("玩法");
		const char* modes[] = { "對方的臉貼到影片上", "對方的臉貼到我臉上", "我的臉貼到對方臉上", "只惡搞我自己的臉" };
		for (int i = 0; i < 4; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			if (ImGui::RadioButton(modes[i], static_cast<int>(prankMode_) == i) && static_cast<int>(prankMode_) != i)
			{
				if (prankOn_)
					togglePrank(); // switching while on: stop cleanly first
				prankMode_ = static_cast<PrankMode>(i);
			}
		}
		hint("對方的臉貼到影片上：送出去的畫面換成你選的影片，影片裡每個人都長著對方的臉，跟著配樂對嘴。\n"
			"對方的臉貼到我臉上：對方會在視訊裡看到自己的臉長在你頭上，跟著你動。\n"
			"我的臉貼到對方臉上：送出去的畫面換成對方的視窗，對方臉上是你的臉。\n"
			"只惡搞我自己的臉：不換臉，只有下巴和哈哈鏡。");
		ImGui::SameLine(0, 24.0f * scale_);
		pushPrimary();
		if (ImGui::Button(prankOn_ ? "結束惡搞 (F9)" : "開始惡搞 (F9)"))
			togglePrank();
		popPrimary();

		if (prankMode_ != PrankMode::JustMe)
		{
			label("對方");
			if (screenGrabSupported())
			{
				const char* preview = windowIndex_ >= 0 ? windows_[windowIndex_].title.c_str()
					: (grabber_.showing() ? "（用拖進來的截圖）" : "選對方的視訊視窗...");
				ImGui::SetNextItemWidth(320.0f * scale_);
				if (ImGui::BeginCombo("##window", preview))
				{
					if (ImGui::IsWindowAppearing())
						refreshWindows();
					for (std::size_t i = 0; i < windows_.size(); ++i)
					{
						const std::string id = windows_[i].title + "##w" + std::to_string(i);
						if (ImGui::Selectable(id.c_str(), windowIndex_ == static_cast<int>(i)))
							watchWindow(static_cast<int>(i), cv::Rect2f(0.0f, 0.0f, 1.0f, 1.0f));
					}
					ImGui::EndCombo();
				}
				hint("選 LINE、Meet、Zoom 這類視訊的視窗。程式抓的是螢幕上看得到的樣子，"
					"所以那個視窗要露出來，不能縮小或被擋住；裡面最大的那張臉就是對方。\n也可以把對方的截圖拖進視窗。");
				ImGui::SameLine();
				if (ImGui::Button("框選"))
					startRegionPick();
				hint("在視窗的畫面上框出對方那一格：只抓框起來的地方，比較快，也不會抓到你自己的小視窗。"
					"視窗移動或縮放時，框會跟著。");
				ImGui::SameLine();
			}
			ImGui::AlignTextToFramePadding();
			const bool watching = staged_ || grabber_.watching() || grabber_.showing();
			if (!watching)
				ImGui::TextDisabled(screenGrabSupported() ? "或把截圖拖進視窗" : "把對方的截圖拖進視窗（這個系統還不能直接抓視窗）");
			else if (grabber_.lost())
				ImGui::TextColored(kWarn, "視窗被縮小或關掉了");
			else if ((staged_ ? stagedTheirs_ : grabber_.latest()).faces.empty())
				ImGui::TextColored(kWarn, "還沒找到臉");
			else
				ImGui::TextColored(kDone, "%s", grabber_.showing() ? "找到截圖裡的臉了"
					: grabPart_.area() < 0.999f ? "找到對方的臉了（即時，框選範圍）" : "找到對方的臉了（即時）");
		}

		if (prankMode_ == PrankMode::TheirFaceOnVideo)
		{
			label("影片");
			drawClips(thumb * 0.8f);
			hint("點一部影片，F9 開始時就播它。影片和同名的 mp3 一起拖進視窗，mp3 就是它的配樂，"
				"下巴會跟著配樂開合。");
		}

		label("效果");
		ImGui::Checkbox("下巴跟著音樂開合", &singAlong_);
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		if (clipAudio_)
			ImGui::TextDisabled("（影片的配樂）");
		else if (music_.loaded())
			ImGui::TextDisabled("（%s）", pathToUtf8(pathFromUtf8(musicPath_).filename()).c_str());
		else
			ImGui::TextColored(kWarn, "（把 mp3 拖進視窗）");
		ImGui::SameLine(0, 24.0f * scale_);
		label("哈哈鏡");
		const char* warps[] = { "無", "大眼", "大嘴", "扁臉", "長臉" };
		for (int i = 0; i < static_cast<int>(Warp::Count); ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			if (ImGui::RadioButton(warps[i], static_cast<int>(warp_) == i))
				warp_ = static_cast<Warp>(i);
		}
		hint("F10 輪流切換。");
		ImGui::SameLine(0, 24.0f * scale_);
		if (ImGui::Button(outputOpen_ ? "關閉輸出視窗" : "開啟輸出視窗（給 OBS）"))
			toggleOutputWindow();
		hint("讓視訊軟體看到惡搞畫面：\n"
			"1. 按這個按鈕，開出只有畫面的「輸出視窗」。\n"
			"2. 在 OBS 新增「視窗擷取」，選這個輸出視窗，再按「啟動虛擬攝影機」。\n"
			"3. 在 LINE / Meet / Zoom 的攝影機選「OBS Virtual Camera」。\n"
			"輸出視窗不會出現按鈕和提示，對方只看得到畫面。");
		break;
	}

	case Drawer::None:
		break;
	}
	ImGui::EndChild();
	drawerHeight_ = ImGui::GetItemRectSize().y;
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
}

void App::drawBottomBar()
{
	const float height = ImGui::GetFrameHeight() * 1.6f;
	const ImVec2 size(86.0f * scale_, height); // six of them, and the picture-taking buttons, in 1280 pixels
	const bool stickersOn = faceSticker_ >= 0 ||
		std::any_of(placed_.begin(), placed_.end(), [](const Placed& s) { return s.on; });
	const bool filtersOn = look_ != Look::None || adjust_.any() || frameStyle_ != FrameStyle::None || dateStamp_ ||
		gray_ || tint_ || skinMark_ || colorMark_;
	const struct
	{
		const char* label;
		Drawer drawer;
		bool marked;
	} categories[] = {
		{ "虛擬背景", Drawer::Background, backgroundMode_ != BackgroundMode::Off },
		{ "貼圖", Drawer::Stickers, stickersOn },
		{ "濾鏡", Drawer::Filters, filtersOn },
		{ "抓壁虎", Drawer::Game, game_ != GameState::Idle },
		{ "當主角", Drawer::Movie, movieOn() },
		{ "惡搞", Drawer::Prank, prankOn_ },
	};
	for (const auto& category : categories)
	{
		if (bigToggle(category.label, drawer_ == category.drawer, category.marked, size))
			drawer_ = drawer_ == category.drawer ? Drawer::None : category.drawer;
		ImGui::SameLine();
	}
	if (ImGui::Button("恢復預設", ImVec2(0, height)))
		resetEffects();
	ImGui::SetItemTooltip("關掉所有玩法、貼圖和濾鏡（保留鏡像）");
	ImGui::SameLine();

	// Right side: freezing, taking pictures, and recording.
	const float shootWidth = 110.0f * scale_;
	const float buttonWidth = 96.0f * scale_;
	const float waveWidth = ImGui::CalcTextSize("揮手拍照").x + ImGui::GetFrameHeight() + 40.0f * scale_;
	const float right = (ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x) - shootWidth -
		buttonWidth * 3.0f - waveWidth - ImGui::GetStyle().ItemSpacing.x * 4.0f;
	ImGui::SameLine(std::max(ImGui::GetCursorPosX(), right));

	const float checkTop = ImGui::GetCursorPosY() + (height - ImGui::GetFrameHeight()) * 0.5f;
	const float top = ImGui::GetCursorPosY();
	ImGui::SetCursorPosY(checkTop);
	ImGui::Checkbox("揮手拍照", &wavePhoto_);
	hint(aiTouch() ? "揮手碰一下畫面右下角的相機圖示就會拍照。"
		: "揮手碰一下畫面右下角的相機圖示就會拍照（要先記住背景）。");
	ImGui::SameLine();
	ImGui::SetCursorPosY(top);

	const bool cameraOk = staged_ || camera_.state() == Camera::State::Running;
	const bool busy = countdown_ || fourShot_ >= 0;
	ImGui::BeginDisabled(!cameraOk);
	if (frozen_)
		pushPrimary();
	if (ImGui::Button(frozen_ ? "繼續" : "暫停畫面", ImVec2(buttonWidth, height)))
		frozen_ = !frozen_;
	if (frozen_)
		popPrimary();
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("先把畫面停住，擺好再拍：暫停時按「拍照」會直接存下這個畫面");
	ImGui::SameLine();

	ImGui::BeginDisabled(!cameraOk || busy);
	pushPrimary();
	if (ImGui::Button(countdown_ ? "準備..." : "拍照", ImVec2(shootWidth, height)))
	{
		if (frozen_)
			shootNextFrame_ = true;
		else
			startCountdown(kShootDelaySeconds);
	}
	popPrimary();
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("倒數 3 秒後拍照（空白鍵也可以）。按右鍵有更多選項。");
	if (ImGui::BeginPopupContextItem("shootMenu"))
	{
		if (ImGui::MenuItem("立即拍照（不倒數）", nullptr, false, cameraOk && !busy))
			shootNextFrame_ = true;
		if (ImGui::MenuItem("複製目前畫面到剪貼簿", nullptr, false, cameraOk && kImageClipboard))
			copyToClipboard(shown_);
		if (!kImageClipboard)
			ImGui::TextDisabled("（這個系統不支援複製圖片）");
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!cameraOk || busy || frozen_);
	if (ImGui::Button("四格拍貼", ImVec2(buttonWidth, height)))
		startFourShot();
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("連拍 4 張，排成一張 2 x 2 的照片");
	ImGui::SameLine();
	ImGui::BeginDisabled(!cameraOk);
	if (recording_)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
	if (ImGui::Button(recording_ ? "停止錄影" : "錄影", ImVec2(buttonWidth, height)))
		toggleRecording();
	if (recording_)
		ImGui::PopStyleColor();
	ImGui::EndDisabled();
}

void App::drawPhotoStrip()
{
	ImGui::Separator();
	const float height = 64.0f * scale_;
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled(photos_.empty() ? "拍的照片會出現在這裡" : "最近的照片");
	const float folderWidth = ImGui::CalcTextSize("開啟資料夾").x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::SameLine((ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x) - folderWidth);
	if (ImGui::Button("開啟資料夾"))
	{
		std::error_code ec;
		std::filesystem::create_directories(outputDir_, ec);
		openWithDefaultApp(outputDir_);
	}
	if (photos_.empty())
	{
		ImGui::Dummy(ImVec2(1.0f, height + 6.0f * scale_));
		return;
	}
	for (std::size_t i = 0; i < photos_.size(); ++i)
	{
		if (i > 0)
			ImGui::SameLine();
		const std::string id = "photo" + std::to_string(i);
		const Photo& photo = photos_[i];
		if (thumbnailButton(id.c_str(), photo.thumbnail, height, false) && !photo.file.empty())
			openWithDefaultApp(photo.file);
		if (photo.file.empty())
			continue;
		ImGui::SetItemTooltip("%s（右鍵有更多選項）", pathToUtf8(photo.file.filename()).c_str());
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("開啟"))
				openWithDefaultApp(photo.file);
			if (ImGui::MenuItem("複製到剪貼簿", nullptr, false, kImageClipboard))
			{
				std::vector<unsigned char> bytes;
				copyToClipboard(readFile(photo.file, bytes) ? cv::imdecode(bytes, cv::IMREAD_COLOR) : cv::Mat());
			}
			if (ImGui::MenuItem("開啟所在資料夾"))
				openWithDefaultApp(photo.file.parent_path());
			ImGui::EndPopup();
		}
	}
}

void App::drawAdvanced()
{
	ImGui::SetNextWindowSize(ImVec2(460.0f * scale_, 600.0f * scale_), ImGuiCond_FirstUseEver);
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 480.0f * scale_,
		viewport->WorkPos.y + 50.0f * scale_), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("進階設定", &advanced_))
	{
		ImGui::End();
		return;
	}
	ImGui::PushFont(fonts_.text, kTextSize * 0.9f);

	if (ImGui::CollapsingHeader("相機", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const std::vector<CameraBackend>& backends = cameraBackends();
		ImGui::InputInt("第幾台", &request_.index);
		request_.index = std::max(0, request_.index);
		if (ImGui::BeginCombo("解析度", kVideoSizes[sizeIndex_].label))
		{
			for (int i = 0; i < 3; ++i)
				if (ImGui::Selectable(kVideoSizes[i].label, i == sizeIndex_))
					sizeIndex_ = i;
			ImGui::EndCombo();
		}
		backendIndex_ = std::min(backendIndex_, static_cast<int>(backends.size()) - 1);
		if (ImGui::BeginCombo("擷取方式", backends[backendIndex_].name))
		{
			for (int i = 0; i < static_cast<int>(backends.size()); ++i)
				if (ImGui::Selectable(backends[i].name, i == backendIndex_))
					backendIndex_ = i;
			ImGui::EndCombo();
		}
		if (ImGui::BeginCombo("相機 fps", kCameraRates[fpsIndex_].label))
		{
			for (int i = 0; i < 4; ++i)
				if (ImGui::Selectable(kCameraRates[i].label, i == fpsIndex_))
					fpsIndex_ = i;
			ImGui::EndCombo();
		}
		hint("向相機要求的速率；相機做不到時會給最接近的。\n"
			"多數網攝最高 30 fps，要 60 / 120 fps 通常得降到 640x480 並開 MJPG。\n"
			"換了之後按「套用並重新開啟相機」。");
		ImGui::Checkbox("要求 MJPG", &request_.preferMjpg);
		hint("壓縮格式讓多數 USB 相機在 1280x720 以上也能跑 30 fps。");
		const std::string gpu = Styler::gpuName();
		bool useGpu = styler_.useGpu();
		ImGui::BeginDisabled(gpu.empty());
		if (ImGui::Checkbox("用 GPU 加速濾鏡", &useGpu))
			styler_.setUseGpu(useGpu);
		ImGui::EndDisabled();
		hint(gpu.empty() ? "這台電腦沒有可用的 OpenCL 裝置，濾鏡都在 CPU 上算。"
			: "美肌和卡通的雙邊濾波交給顯示卡（OpenCL），約快 7 倍。\n"
			  "AI 去背和人臉偵測留在 CPU，但在另一條執行緒跑，不會拖慢畫面。");
		if (ImGui::Button("套用並重新開啟相機"))
			openCamera();
		ImGui::SameLine();
		const bool open = camera_.state() == Camera::State::Running || camera_.state() == Camera::State::Opening;
		ImGui::BeginDisabled(!open || staged_);
		if (ImGui::Button("關閉相機"))
			closeCamera();
		ImGui::EndDisabled();

		ImGui::TextUnformatted("畫面更新");
		const struct
		{
			const char* label;
			double interval;
		} rates[] = { { "自動（最高）", 0.0 }, { "30 fps", 1.0 / 30.0 }, { "15 fps", 1.0 / 15.0 } };
		for (const auto& rate : rates)
		{
			ImGui::SameLine();
			if (ImGui::RadioButton(rate.label, std::fabs(previewInterval_ - rate.interval) < 1e-4))
				previewInterval_ = rate.interval;
		}
		hint("自動：相機送來幾張就處理幾張；電腦忙不過來時只處理最新的一張，畫面不會延遲。\n"
			"30 / 15 fps：固定上限，可以減輕電腦負擔。錄影也會用這個速率。");

		if (camera_.state() == Camera::State::Running)
		{
			if (ImGui::TreeNode("曝光、白平衡等"))
			{
				ImGui::TextDisabled("數值的意義和範圍依驅動程式而定。");
				for (const auto& prop : Camera::adjustableProperties())
				{
					const double current = camera_.property(prop.first);
					if (std::isnan(current))
						continue;
					float value = static_cast<float>(current);
					if (ImGui::DragFloat(prop.second, &value, 0.5f))
						camera_.setProperty(prop.first, value);
				}
#if defined(_WIN32)
				const bool directShow = request_.api == cv::CAP_DSHOW;
				ImGui::BeginDisabled(!directShow);
				if (ImGui::Button("驅動程式的設定視窗"))
					camera_.openSettingsDialog();
				ImGui::EndDisabled();
				if (!directShow)
					hint("擷取方式選 DirectShow 才有。");
#endif
				ImGui::TreePop();
			}
		}
		else if (camera_.state() == Camera::State::Failed)
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", camera_.error().c_str());
	}

	if (ImGui::CollapsingHeader("背景偵測"))
	{
		ImGui::TextWrapped(ai_.segmenterReady()
			? "AI 人像分割已啟用：去背、揮手拍照、抓壁虎都直接用 AI 找人，不必記住背景。下面的設定只影響「比較背景」去背和除錯畫面。"
			: "沒有 AI 模型，改用「記住背景再比較」的方式找人。");
		if (ImGui::Button(backgroundReady() ? "重新記住背景" : "記住背景"))
			startLearning();
		if (ImGui::SliderInt("靈敏度門檻", &threshold_, 1, 20))
			effects_.detector().setThreshold(threshold_);
		hint("像素要變化到自身雜訊的幾倍才算「有東西」。數字越小越敏感。");
		if (ImGui::SliderFloat("背景更新速度", &adaptation_, 0.0f, 0.05f, "%.3f"))
			effects_.detector().setAdaptationRate(adaptation_);
		hint("讓背景慢慢跟上光線變化（遠離人的地方才更新）。0 表示不更新。");
		ImGui::TextUnformatted("畫面顯示");
		const char* views[] = { "一般", "差異", "前景遮罩", "8x8 區塊" };
		for (int i = 0; i < 4; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			if (ImGui::RadioButton(views[i], debugView_ == i))
				debugView_ = i;
		}
		hint("前景遮罩：白色是人或物體，灰色是影子，黑色是背景。有 AI 時顯示的是 AI 找到的人。");
		if (backgroundPreview_.valid())
		{
			ImGui::TextUnformatted("記住的背景");
			const float width = ImGui::GetContentRegionAvail().x;
			ImGui::Image(backgroundPreview_.id(), backgroundPreview_.fit(ImVec2(width, width)));
		}
	}

	if (ImGui::CollapsingHeader("顏色範圍（綠幕、標出指定顏色）"))
	{
		ImGui::DragIntRange2("Y 亮度", &colorRange_->yMin, &colorRange_->yMax, 1.0f, 0, 255);
		ImGui::DragIntRange2("U 藍色差", &colorRange_->uMin, &colorRange_->uMax, 1.0f, 0, 255);
		ImGui::DragIntRange2("V 紅色差", &colorRange_->vMin, &colorRange_->vMax, 1.0f, 0, 255);
	}

	if (ImGui::CollapsingHeader("照片與影片"))
	{
		ImGui::Checkbox("同時保存未加效果的相機畫面", &saveCameraCopy_);
		ImGui::BeginDisabled(recording_);
		if (ImGui::BeginCombo("錄影格式", kCodecs[codecIndex_].label))
		{
			for (int i = 0; i < 3; ++i)
				if (ImGui::Selectable(kCodecs[i].label, i == codecIndex_))
					codecIndex_ = i;
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		hint("Motion JPEG 在每個系統都能錄；其他格式要看 OpenCV 有沒有對應的編碼器。");
		ImGui::TextWrapped("存放位置：%s", pathToUtf8(outputDir_).c_str());
		if (ImGui::Button("開啟資料夾##advanced"))
		{
			std::error_code ec;
			std::filesystem::create_directories(outputDir_, ec);
			openWithDefaultApp(outputDir_);
		}
	}

	if (ImGui::CollapsingHeader("音樂與音效"))
	{
		bool soundOn = sound_.enabled();
		if (ImGui::Checkbox("音效（倒數、快門、遊戲）", &soundOn))
			sound_.setEnabled(soundOn);
		ImGui::InputText("音樂檔", musicPath_, sizeof(musicPath_));
		if (ImGui::Button("載入"))
		{
			std::string error;
			if (music_.load(pathFromUtf8(musicPath_), error))
			{
				music_.setVolume(volume_);
				music_.setLooping(loop_);
				music_.play();
			}
			else
				log(error);
		}
		ImGui::BeginDisabled(!music_.loaded());
		ImGui::SameLine();
		if (ImGui::Button(music_.playing() ? "暫停" : "播放"))
			music_.playing() ? music_.pause() : music_.play();
		ImGui::SameLine();
		if (ImGui::Button("停止"))
			music_.stop();
		ImGui::SameLine();
		if (ImGui::Button("從頭播放"))
		{
			music_.stop();
			music_.play();
		}
		if (ImGui::Checkbox("重複播放", &loop_))
			music_.setLooping(loop_);
		if (ImGui::SliderFloat("音量", &volume_, 0.0f, 1.0f))
			music_.setVolume(volume_);
		if (music_.loaded())
		{
			const float length = music_.length();
			const float position = music_.position();
			ImGui::ProgressBar(length > 0.0f ? position / length : 0.0f, ImVec2(-1.0f, 0.0f),
				(std::to_string(static_cast<int>(position)) + " / " + std::to_string(static_cast<int>(length)) + " 秒").c_str());
		}
		ImGui::EndDisabled();
	}

	if (staged_)
		ImGui::SetNextItemOpen(true, ImGuiCond_Once); // so --screenshot shows it
	if (ImGui::CollapsingHeader("資訊"))
	{
		// What the original showed in its Information, Video Format, and
		// Capture Parameter tabs, as far as OpenCV can tell, plus the AI.
		const bool running = camera_.state() == Camera::State::Running;
		const Camera::Info info = camera_.info();
		const webcam::ChangeDetector& detector = effects_.detector();
		if (ImGui::BeginTable("info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
		{
			const auto row = [](const char* name, const std::string& value) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(name);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(value.c_str());
			};
			char text[160];
			row("擷取程式", running ? info.backend : "-");
			row("OpenCV 版本", CV_VERSION);
			row("第幾台相機", std::to_string(request_.index));
			std::snprintf(text, sizeof(text), "%d x %d", running ? info.width : frameWidth_, running ? info.height : frameHeight_);
			row("相機解析度", text);
			row("相機傳來的格式", running ? info.fourcc : "-");
			std::snprintf(text, sizeof(text), "%.1f（驅動回報）/ %.1f（實際）", info.fps, camera_.measuredFps());
			row("相機 fps", text);
			if (previewInterval_ > 0.0)
				std::snprintf(text, sizeof(text), "上限 %.0f fps", 1.0 / previewInterval_);
			else
				std::snprintf(text, sizeof(text), "自動（目前 %.1f fps）", camera_.measuredFps());
			row("畫面更新", text);
			std::snprintf(text, sizeof(text), "%.1f fps", processingFps_);
			row("處理 fps", text);
			std::snprintf(text, sizeof(text), "%.1f fps", ImGui::GetIO().Framerate);
			row("螢幕更新 fps", text);
			std::snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(framesProcessed_));
			row("已處理的畫面", text);
			std::snprintf(text, sizeof(text), "%.1f ms", processingMs_);
			row("每張處理時間", text);
			std::snprintf(text, sizeof(text), "%.1f ms（另一條執行緒，不佔上面的時間）", aiMs_);
			row("AI 耗時", text);
			const std::string gpu = Styler::gpuName();
			row("GPU", gpu.empty() ? "沒有 OpenCL 裝置" : gpu + (styler_.useGpu() ? "（使用中）" : "（已關閉）"));
			row("AI 人像分割", ai_.segmenterReady() ? "已載入 PP-HumanSeg" : "沒有模型");
			row("人臉偵測", ai_.faceTrackerReady() ? "已載入 YuNet" : "沒有模型");
			if (faceSticker_ >= 0)
				row("偵測到的臉", std::to_string(faces_.size()));
			if (detector.hasBackground())
				std::snprintf(text, sizeof(text), "%d x %d", detector.backgroundWidth(), detector.backgroundHeight());
			row("記住的背景", detector.hasBackground() ? text : "還沒有");
			if (recording_)
			{
				const double seconds = std::chrono::duration<double>(Clock::now() - recordingStart_).count();
				std::snprintf(text, sizeof(text), "%llu 張，%.0f 秒", static_cast<unsigned long long>(recordedFrames_), seconds);
				row("錄影中", text);
				row("錄影檔", pathToUtf8(pathFromUtf8(recordingFile_).filename()));
			}
			row("錄影格式", kCodecs[codecIndex_].label);
			row("設定檔", persistent_ ? pathToUtf8(home_ / "settings.ini") : "（這次不儲存）");
			ImGui::EndTable();
		}
	}

	if (ImGui::CollapsingHeader("紀錄"))
	{
		ImGui::BeginChild("log", ImVec2(0, 160.0f * scale_), ImGuiChildFlags_Borders);
		for (const std::string& line : log_)
			ImGui::TextWrapped("%s", line.c_str());
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
	}

	ImGui::PopFont();
	ImGui::End();
}

} // namespace app
