// ---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "MainForm.h"
#include <vfw.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

using namespace webcam;

long dwDriverIndex; // Index of the capture driver on this computer.
wchar_t szDeviceName[80];
wchar_t szDeviceVersion[80];

// On a 32-bit machine, use the following declarations instead:
// char szDeviceName[80];	// Driver name.
// char szDeviceVersion[80];	// Driver version.

HWND hwndVideo = NULL; // Handle of the created video capture window
wchar_t wstrCaptureWindowName[20] = L"My Capture Window";
int iCapWindowX1; // Top-left corner of the capture window ( iCapWindowX1, iCapWindowY1 ).
int iCapWindowY1;
int iCapWindowWidth;
int iCapWindowHeight;

// Global variable declarations.
tagCapDriverCaps cdcCapDriverCapability; // tagCapDriverCaps is a struct type.
tagCapStatus capWindowStatus;

// Global variable declarations.
CAPTUREPARMS cpCaptureParms; // CAPTUREPARMS is the struct type for capture parameters.
DWORD dwFileAlloc; // Disk space to pre-allocate for recording, in bytes.

// The one preview callback; it hands every frame to Form1->ProcessFrame.
static LRESULT CALLBACK FrameCallback(HWND hwndCapture, PVIDEOHDR lpvhdr);

// ---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm1 *Form1;

// ---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner) : TForm(Owner)
{
	Memo1->Clear();

	// The song is not part of the repository; put it next to the executable.
	// Opening a missing file throws, so check first and disable the player instead.
	const String music = ExtractFilePath(Application->ExeName) + L"僕が死のうと思ったのは.mp3";
	MediaPlayer1->Enabled = false;
	if (FileExists(music))
	{
		MediaPlayer1->FileName = music;
		try
		{
			MediaPlayer1->Open();
			MediaPlayer1->Enabled = true;
		}
		catch (Exception &e)
		{
			Memo1->Lines->Add(String("Cannot open the music: ") + e.Message);
		}
	}

	// Starting color range for color filtering and the virtual background,
	// used until the Y/U/V edits change it.
	const YuvRange range = { 125, 255, 125, 255, 120, 255 };
	FColorRange = std::make_shared<YuvRange>(range);
	const Rgb green = { 0, 255, 0 };
	FFillColor = rgbToYuv(green);
	FDragonBallIndex = 3;
	FUseColorKey = false;
	FBackgroundIndex = 0;
	FFrameCounter = 0;
	FVideoWidth = 0;
	FVideoHeight = 0;
	FVideoIsYuy2 = false;
	std::srand(static_cast<unsigned>(std::time(0)));
}

// ---------------------------------------------------------------------------
void __fastcall TForm1::On1Click(TObject *Sender)
{
	dwDriverIndex = 0;
	// Make the panel holding the TLabels visible.
	pnlDriverDescription->Visible = true;
	// Pass lengths in characters: passing bytes would let a wide-char driver
	// write past the end of the buffers.
	if (capGetDriverDescription(dwDriverIndex, szDeviceName,
		sizeof(szDeviceName) / sizeof(szDeviceName[0]), szDeviceVersion,
		sizeof(szDeviceVersion) / sizeof(szDeviceVersion[0])))
	{ // Returns true when the information was retrieved
		// and shows the strings in lblCapDriverName, lblCapDriverVersion
		lblCapDriverName->Caption = String("Name: ") + szDeviceName;
		// lblCapDriverName
		lblCapDriverVersion->Caption = szDeviceVersion;
	}
}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Off1Click(TObject *Sender)
{
	pnlDriverDescription->Visible = false;
}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Create1Click(TObject *Sender)
{
	iCapWindowX1 = 10;
	iCapWindowY1 = 10;
	iCapWindowWidth = 320; // Capture window width is 320.
	iCapWindowHeight = 240; // Capture window height is 240.

	CreateCaptureWindow();

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Destroy1Click(TObject *Sender)
{
	DestroyCaptureWindow();
	pnlCaptureWindow->Visible = false;

}

// ---------------------------------------------------------------------------
__fastcall TForm1::~TForm1()
{
	// Frame callbacks reach into this form, so stop them before it goes away.
	DestroyCaptureWindow();
}

void TForm1::CreateCaptureWindow()
{
	// Creating again replaces the old window instead of leaking it.
	DestroyCaptureWindow();
	// pnlCaptureWindow is the panel that hosts the capture window.
	pnlCaptureWindow->Visible = true;
	hwndVideo = capCreateCaptureWindow(wstrCaptureWindowName, WS_CHILD | WS_VISIBLE,
		iCapWindowX1, iCapWindowY1, iCapWindowWidth, iCapWindowHeight,
		pnlCaptureWindow->Handle, 1);
}

void TForm1::DestroyCaptureWindow()
{
	if (hwndVideo == NULL)
		return;
	capSetCallbackOnFrame(hwndVideo, NULL);
	capDriverDisconnect(hwndVideo);
	// The capture window is closed like any other window.
	DestroyWindow(hwndVideo);
	hwndVideo = NULL;
}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Connect1Click(TObject *Sender)
{
	// Connect the capture window to the capture device driver.
	// Once connected, frames from the device are shown directly in the capture window.
	capDriverConnect(hwndVideo, dwDriverIndex);
	RefreshVideoFormat();

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Disconnect1Click(TObject *Sender)
{
	// Disconnect the capture driver from the capture window.
	capDriverDisconnect(hwndVideo);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Enable1Click(TObject *Sender)
{
	// Switch to preview mode.
	capPreview(hwndVideo, true);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Disable1Click(TObject *Sender)
{
	// Turn preview mode off.
	capPreview(hwndVideo, false);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::N30framesec1Click(TObject *Sender)
{
	// Set the preview rate to 33.3 milliseconds, or 30 FPS.
	capPreviewRate(hwndVideo, 33);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::N15framesec1Click(TObject *Sender)
{
	// Set the preview rate to 66 milliseconds, or 15 FPS.
	capPreviewRate(hwndVideo, 66);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::On2Click(TObject *Sender)
{
	// Scale the video to fit the capture window.
	capPreviewScale(hwndVideo, true);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Off2Click(TObject *Sender)
{
	// Stop scaling the video to fit the capture window.
	capPreviewScale(hwndVideo, false);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::N3202401Click(TObject *Sender)
{
	// Resize the capture window to 320*240.
	pnlCaptureWindow->Width = 330;
	pnlCaptureWindow->Height = 250;
	iCapWindowWidth = 320;
	iCapWindowHeight = 240;
	MoveWindow(hwndVideo, iCapWindowX1, iCapWindowY1, iCapWindowWidth,
		iCapWindowHeight, true);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::N6404801Click(TObject *Sender)
{
	// Resize the capture window to 640 * 480.
	pnlCaptureWindow->Width = 650;
	pnlCaptureWindow->Height = 490;
	iCapWindowWidth = 640;
	iCapWindowHeight = 480;
	MoveWindow(hwndVideo, iCapWindowX1, iCapWindowY1, iCapWindowWidth,
		iCapWindowHeight, true);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Capabilities1Click(TObject *Sender)
{
	pcWebcam->ActivePage = tsInformation;
	// Query the capture driver capabilities.
	if (capDriverGetCaps(hwndVideo, &cdcCapDriverCapability,
		sizeof(cdcCapDriverCapability)))
	{
		lblDriverIndex->Caption = AnsiString("Driver Index:") +
			AnsiString(cdcCapDriverCapability.wDeviceIndex);
		if (cdcCapDriverCapability.fHasOverlay)
			lblOverlay->Caption = AnsiString(" Can device overlay ?  Yes! ");
		else
			lblOverlay->Caption = AnsiString(" Can device overlay ? No! ");
		// lblOverlay is a TLabel.

		if (cdcCapDriverCapability.fHasDlgVideoSource)
		{
			// miVideoSource is a menu item of the main menu.
			// Enable the miVideoSource menu item.
			miVideoSource->Enabled = true;
			lblVideoSourceDialog->Caption =
				AnsiString(" Has video source dialog ? Yes! ");
		}
		else
		{
			// Disable the miVideoSource menu item.
			miVideoSource->Enabled = false;
			lblVideoSourceDialog->Caption =
				AnsiString(" Has video source dialog? No! ");
		}

		if (cdcCapDriverCapability.fHasDlgVideoFormat)
		{
			// miVideoFormat is a menu item of the main menu.
			// Enable the miVideoFormat menu item.
			miVideoFormat->Enabled = true;
			lblVideoFormatDialog->Caption =
				AnsiString(" Has video format dialog? Yes! ");
		}
		else
		{
			// Disable the miVideoFormat menu item.
			miVideoFormat->Enabled = false;
			lblVideoFormatDialog->Caption =
				AnsiString(" Has video format dialog ? No ");
		}

		if (cdcCapDriverCapability.fHasDlgVideoDisplay)
		{
			// miVideoDisplay is a menu item of the main menu.
			// Enable the miVideoDisplay menu item.
			miVideoDisplay->Enabled = true;
			lblVideoDisplayDialog->Caption =
				AnsiString(" Has video display dialog ? Yes ");
		}
		else
		{
			// Disable the miVideoDisplay menu item.
			miVideoDisplay->Enabled = false;
			lblVideoDisplayDialog->Caption =
				AnsiString(" Has video display dialog ? No ");
		}

		if (cdcCapDriverCapability.fCaptureInitialized)
			lblDriverReady->Caption =
				AnsiString(" Driver ready to capture ? Yes ");
		else
			lblDriverReady->Caption =
				AnsiString(" Driver ready to capture ? No ");

		if (cdcCapDriverCapability.fDriverSuppliesPalettes)
			lblDriverPalette->Caption =
				AnsiString(" Can Driver make palette ? Yes ");
		else
			lblDriverPalette->Caption =
				AnsiString(" Can Driver make palette ? No ");
	}
	else
		ShowMessage(
		" Capture Window does not connect to any Capture Driver ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miVideoSourceClick(TObject *Sender)
{
	// Open the video source dialog; show a message if it cannot be opened.
	if (!capDlgVideoSource(hwndVideo))
		ShowMessage(" Can't open video source dialog !! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miVideoFormatClick(TObject *Sender)
{
	// Open the video format dialog; show a message if it cannot be opened.
	if (!capDlgVideoFormat(hwndVideo))
		ShowMessage(" Can't open video format dialog ! ! ");
	// The user may have picked another size or color format.
	RefreshVideoFormat();

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miVideoDisplayClick(TObject *Sender)
{
	// Open the video display dialog; show a message if it cannot be opened.
	if (!capDlgVideoDisplay(hwndVideo))
		ShowMessage(" Can't open video display dialog ! ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miVideoCompressionClick(TObject *Sender)
{
	// Open the video compression dialog; show a message if it cannot be opened.
	if (!capDlgVideoCompression(hwndVideo))
		ShowMessage(" Can't open video compression dialog ! ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::AutoDisplay1Click(TObject *Sender)
{
	iCapWindowX1 = 10;
	iCapWindowY1 = 10;
	pnlCaptureWindow->Width = 650;
	pnlCaptureWindow->Height = 490;
	iCapWindowWidth = 640;
	iCapWindowHeight = 480;

	CreateCaptureWindow();
	capDriverConnect(hwndVideo, dwDriverIndex);
	capPreview(hwndVideo, true);
	capPreviewRate(hwndVideo, 33);
	capPreviewScale(hwndVideo, true);
	RefreshVideoFormat();
	pcWebcam->TabIndex = 0;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnGrabFrameClick(TObject *Sender)
{
	// Grab a single frame into the frame buffer. The capture window freezes
	// until preview or overlay mode is restarted.
	// Freezing lets you check whether the grabbed frame is acceptable.
	if (!capGrabFrame(hwndVideo))
		ShowMessage(" Can't Grab Frame to the video frame buffer ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnGrabFrameNoStopClick(TObject *Sender)
{
	// Grab a single frame into the frame buffer without freezing the capture window.
	if (!capGrabFrameNoStop(hwndVideo))
		ShowMessage(" Can't Grab Frame to the Video Frame Buffre ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnEditCopyClick(TObject *Sender)
{
	// Copy the frame buffer to the clipboard so other applications can paste it.
	if (!capEditCopy(hwndVideo))
		ShowMessage(" Can't Copy the Video Frame Buffre to the Clipboard ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnFileSaveDIBClick(TObject *Sender)
{
	// Two steps:
	// 1. Ask for the bmp file name with SavePictureDialog1.
	// 2. Save it directly with capFileSaveDIB.
	if (SavePictureDialog1->Execute())
	{
		if (!capFileSaveDIB(hwndVideo, &(SavePictureDialog1->FileName[1])))
			ShowMessage(" Can't Save the Clipboard Image ! ");
	}

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::Status1Click(TObject *Sender)
{
	pcWebcam->ActivePage = tsInformation;
	// Query the capture driver capabilities.
	if (capGetStatus(hwndVideo, &capWindowStatus, sizeof(capWindowStatus)))
	{
		lblImageWidth->Caption = AnsiString("Image width:") +
			AnsiString(capWindowStatus.uiImageWidth);
		lblImageHeight->Caption = AnsiString("Image height:") +
			AnsiString(capWindowStatus.uiImageHeight);
		if (capWindowStatus.fLiveWindow)
			lblLiveWindow->Caption = AnsiString(" Live window ? Yes! ");
		else
			lblLiveWindow->Caption = AnsiString(" Live window ? No! ");

		if (capWindowStatus.fScale)
			lblScale->Caption = AnsiString(" Scale ? Yes! ");
		else
			lblScale->Caption = AnsiString(" Scale ? No! ");

		lblScroll->Caption = AnsiString("Scroll? (") +
			AnsiString(capWindowStatus.ptScroll.x) + AnsiString(", ") +
			AnsiString(capWindowStatus.ptScroll.y) + AnsiString(")");

		if (capWindowStatus.fUsingDefaultPalette)
			lblUsingDefaultPalette->Caption =
				AnsiString(" Using default palette? Yes! ");
		else
			lblUsingDefaultPalette->Caption =
				AnsiString(" Using default palette? No ");

		if (capWindowStatus.fAudioHardware)
			lblAudioHardware->Caption = AnsiString(" Audio hardware? Yes ");
		else
			lblAudioHardware->Caption = AnsiString(" Audio hardware? No ");

		if (capWindowStatus.fCapFileExists)
			lblCaptureFileExist->Caption =
				AnsiString(" Capture file exist? Yes ");
		else
			lblCaptureFileExist->Caption =
				AnsiString(" Capture file exist? No ");

		lblCurrentVideoFrame->Caption = AnsiString("Current video frame:") +
			AnsiString(capWindowStatus.dwCurrentVideoFrame);
		lblCurrentVideoFrameDropped->Caption =
			AnsiString("Current video frame dropped:") +
			AnsiString(capWindowStatus.dwCurrentVideoFramesDropped);
		lblCurrentWaveSamples->Caption = AnsiString("Current wave samples:") +
			AnsiString(capWindowStatus.dwCurrentWaveSamples);
		lblCurrentTimeElapsedMS->Caption =
			AnsiString("Current time elapsed MS:") +
			AnsiString(capWindowStatus.dwCurrentTimeElapsedMS);

		if (capWindowStatus.fCapturingNow)
			lblCaptureNow->Caption = AnsiString(" Capture Now? Yes ");
		else
			lblCaptureNow->Caption = AnsiString(" Capture Now? No ");

		lblReturn->Caption = AnsiString("Return:") +
			AnsiString(capWindowStatus.dwReturn);
		lblNumberOfVideoAllocated->Caption =
			AnsiString("Number of video allocated:") +
			AnsiString(capWindowStatus.wNumVideoAllocated);
		lblNumberOfAudioAllocated->Caption =
			AnsiString("Number of audio allocated:") +
			AnsiString(capWindowStatus.wNumAudioAllocated);
	}
	else
		ShowMessage(
		" Capture Window does not connect to any Capture Driver ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miGetSeClick(TObject *Sender)
{
	pcWebcam->ActivePage = tsCaptureParameter;
	if (capCaptureGetSetup(hwndVideo, &cpCaptureParms, sizeof(cpCaptureParms)))
	{
		lblRequestMicroSecPerFrame->Caption =
			AnsiString("Request Micro Sec Per Frame : ") +
			AnsiString(cpCaptureParms.dwRequestMicroSecPerFrame);
		if (cpCaptureParms.fMakeUserHitOKToCapture)
			lblMakeUserHitOKToCapture->Caption =
				AnsiString("Make User Hit OK to Capture ? Yes ! ");
		else
			lblMakeUserHitOKToCapture->Caption =
				AnsiString("Make User Hit OK to Capture ? No ! ");
		lblPercentDropForError->Caption =
			AnsiString("Percent Drop For Error: ") +
			AnsiString(cpCaptureParms.wPercentDropForError);
		if (cpCaptureParms.fYield)
			lblYield->Caption = AnsiString("Yield ? Yes ! ");
		else
			lblYield->Caption = AnsiString("Yield ? No ! ");
		lblIndexSize->Caption = AnsiString("Index Size: ") +
			AnsiString(cpCaptureParms.dwIndexSize);
		lblChunkGranularity->Caption = AnsiString("Chunk Granularity : ") +
			AnsiString(cpCaptureParms.wChunkGranularity);
		if (cpCaptureParms.fUsingDOSMemory)
			lblUsingDOSMemory->Caption =
				AnsiString("Using DOS Memory ? Yes ! ");
		else
			lblUsingDOSMemory->Caption = AnsiString("Using DOS Memory ? No ! ");
		lblNumVideoRequested->Caption =
			AnsiString("Number of Video Requested : ") +
			AnsiString(cpCaptureParms.wNumVideoRequested);
		if (cpCaptureParms.fCaptureAudio)
			lblCaptureAudio->Caption = AnsiString("Capture Audio ? Yes ! ");
		else
			lblCaptureAudio->Caption = AnsiString("Capture Audio ? No ! ");
		lblNumAudioRequested->Caption =
			AnsiString(" Number of Audio Requested: ") +
			AnsiString(cpCaptureParms.wNumAudioRequested);
		lblKeyAbort->Caption = AnsiString("Key Abort: ") +
			AnsiString(cpCaptureParms.vKeyAbort);
		if (cpCaptureParms.fAbortLeftMouse)
			lblAbortLeftMouse->Caption =
				AnsiString("Abort Left Mouse ? Yes ! ");
		else
			lblAbortLeftMouse->Caption = AnsiString("Abort Left Mouse ? No ! ");
		if (cpCaptureParms.fAbortRightMouse)
			lblAbortRightMouse->Caption =
				AnsiString("Abort Right Mouse ? Yes ! ");
		else
			lblAbortRightMouse->Caption =
				AnsiString("Abort Right Mouse ? No ! ");
		if (cpCaptureParms.fLimitEnabled)
			lblLimitEnabled->Caption = AnsiString(" Limit Enabled ? Yes ! ");
		else
			lblLimitEnabled->Caption = AnsiString("Limit Enabled ? No ! ");
		lblTimeLimit->Caption = AnsiString("Time Limit: ") +
			AnsiString(cpCaptureParms.wTimeLimit);
		if (cpCaptureParms.fMCIControl)
			lblMCIControl->Caption = AnsiString("MCI Control ? Yes ! ");
		else
			lblMCIControl->Caption = AnsiString("MCI Control ? No ! ");
		if (cpCaptureParms.fStepMCIDevice)
			lblStepMCIDevice->Caption = AnsiString("Step MCI Device ? Yes ! ");
		else
			lblStepMCIDevice->Caption = AnsiString("Step MCI Device ? No ! ");
		lblMCIStartTime->Caption = AnsiString("MCI Start Time : ") +
			AnsiString(cpCaptureParms.dwMCIStartTime);
		lblMCIStopTime->Caption = AnsiString("MCI Stop Time : ") +
			AnsiString(cpCaptureParms.dwMCIStopTime);
		if (cpCaptureParms.fStepCaptureAt2x)
			lblStepCaptureAt2x->Caption =
				AnsiString("Step Capture at 2x ? Yes ! ");
		else
			lblStepCaptureAt2x->Caption =
				AnsiString("Step Capture at 2x ? No ! ");
		lblStepCaptureAverageFrames->Caption =
			AnsiString("Step Capture Average Frames : ") +
			AnsiString(cpCaptureParms.wStepCaptureAverageFrames);
		lblAudioBufferSize->Caption = AnsiString("Audio Buffer Size : ") +
			AnsiString(cpCaptureParms.dwAudioBufferSize);
		if (cpCaptureParms.fDisableWriteCache)
			lblDisableWriteCache->Caption =
				AnsiString("Disable Write Cache ? Yes ! ");
		else
			lblDisableWriteCache->Caption =
				AnsiString("Disable Write Cache ? No ! ");
		lblAVStreamMaster->Caption = AnsiString("AV Stream Master : ") +
			AnsiString(cpCaptureParms.AVStreamMaster);
	}

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnSetFilenameClick(TObject *Sender)
{
	// Set the file name for the recording.
	if (SaveDialog1->Execute())
	{
		const String fileName = SaveDialog1->FileName;
		ShowMessage(fileName);
		if (!capFileSetCaptureFile(hwndVideo, fileName.c_str()))
		{
			ShowMessage("Can't Set the Capture Filename ! ");
			ShowMessage(fileName);
		}
		// Read back what the driver will use. The buffer fits any Windows path,
		// and its length is given in characters so it cannot be overrun.
		std::vector<wchar_t> current(32768);
		if (capFileGetCaptureFile(hwndVideo, &current[0], current.size()))
			lblCaptureFilename->Caption = String(&current[0]);
		else
			ShowMessage("Can't Get the Capture Filename ! ");
	}

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::edFileAllocChange(TObject *Sender)
{
	// Disk space to pre-allocate for the video file, in megabytes.
	// capFileAlloc takes a DWORD, so 4095 MB is the most it can express.
	const int megabytes = std::min(std::max(StrToIntDef(edFileAlloc->Text, 0), 0), 4095);
	dwFileAlloc = static_cast<DWORD>(megabytes) * 1024 * 1024;

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnFileAllocClick(TObject *Sender)
{
	// Pre-allocate the configured disk space.
	if (!capFileAlloc(hwndVideo, dwFileAlloc))
		ShowMessage("Can't Allocate " + edFileAlloc->Text + "MB ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnCaptureSequenceClick(TObject *Sender)
{
	// Start recording; show a message if it fails.
	if (!capCaptureSequence(hwndVideo))
		ShowMessage("Can't Capture Sequence ! ");

}
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Frame processing
// ---------------------------------------------------------------------------

namespace {

const Rgb kWhite = { 255, 255, 255 };
const Rgb kPureRed = { 255, 0, 0 }; // background of the dragon ball pictures
const Rgb kGreen = { 0, 255, 0 };
const Rgb kSkinTint = { 95, 40, 20 };

Rgb RgbOf(TColor color)
{
	const COLORREF c = ColorToRGB(color);
	Rgb out = { GetRValue(c), GetGValue(c), GetBValue(c) };
	return out;
}

// Converts the picture held by a TImage, whatever its pixel format.
std::shared_ptr<const Image> PictureOf(TImage *image, bool keyed, Rgb transparent)
{
	std::unique_ptr<Graphics::TBitmap> bitmap(new Graphics::TBitmap());
	bitmap->Assign(image->Picture->Bitmap);
	bitmap->PixelFormat = pf24bit;
	const int width = bitmap->Width;
	const int height = bitmap->Height;
	const std::size_t stride = static_cast<std::size_t>(width) * 3;
	if (width <= 0 || height <= 0)
		return Image::fromBgr24(0, 0, 0, 0);
	std::vector<std::uint8_t> pixels(stride * height);
	for (int y = 0; y < height; ++y)
		std::memcpy(&pixels[y * stride], bitmap->ScanLine[y], stride);
	return keyed
		? Image::fromBgr24Keyed(&pixels[0], width, height, stride, transparent)
		: Image::fromBgr24(&pixels[0], width, height, stride);
}

void SaveFrameAsBitmap(const Yuy2Frame& frame, const String& fileName)
{
	std::unique_ptr<Graphics::TBitmap> bitmap(new Graphics::TBitmap());
	bitmap->PixelFormat = pf24bit;
	bitmap->Width = frame.width();
	bitmap->Height = frame.height();
	const std::size_t stride = static_cast<std::size_t>(frame.width()) * 3;
	std::vector<std::uint8_t> pixels(stride * frame.height());
	frame.copyToBgr24(&pixels[0], stride);
	for (int y = 0; y < frame.height(); ++y)
		std::memcpy(bitmap->ScanLine[y], &pixels[y * stride], stride);
	bitmap->SaveToFile(fileName);
}

} // namespace

static LRESULT CALLBACK FrameCallback(HWND hwndCapture, PVIDEOHDR lpvhdr)
{
	// Some drivers leave dwBytesUsed at 0; the buffer length is the upper bound.
	const DWORD size = lpvhdr->dwBytesUsed != 0 ? lpvhdr->dwBytesUsed : lpvhdr->dwBufferLength;
	Form1->ProcessFrame(lpvhdr->lpData, size);
	return 0;
}

void TForm1::ProcessFrame(unsigned char *data, unsigned long size)
{
	++FFrameCounter;
	String status = IntToStr(FFrameCounter);
	Yuy2Frame frame;
	if (!FVideoIsYuy2)
		status += String("  (video format is not YUY2, effects are off)");
	else if (!frame.wrap(data, size, FVideoWidth, FVideoHeight))
		status += String("  (frame does not match the video format)");
	else
	{
		if (FGeckoGame)
			FGeckoGame->setShowBlocks(cbBlockDisplay->Checked);
		FEffects.process(frame);
	}
	lblFrameCounter->Caption = status;
}

void TForm1::RefreshVideoFormat()
{
	FVideoWidth = 0;
	FVideoHeight = 0;
	FVideoIsYuy2 = false;
	const DWORD size = capGetVideoFormatSize(hwndVideo);
	if (size < sizeof(BITMAPINFOHEADER))
		return;
	// The format can be larger than BITMAPINFO (palettes, extra fields).
	std::vector<BYTE> buffer(size);
	if (!capGetVideoFormat(hwndVideo, &buffer[0], size))
		return;
	const BITMAPINFOHEADER *header = reinterpret_cast<const BITMAPINFOHEADER*>(&buffer[0]);
	FVideoWidth = header->biWidth;
	FVideoHeight = std::abs(header->biHeight);
	FVideoIsYuy2 = header->biCompression == Yuy2Frame::kFourCC;
}

void TForm1::InstallFrameCallback()
{
	RefreshVideoFormat();
	capSetCallbackOnFrame(hwndVideo, &FrameCallback);
}

void TForm1::ToggleEffect(TMenuItem *item, const std::shared_ptr<Effect>& effect)
{
	if (FEffects.contains(effect))
		FEffects.remove(effect);
	else
		FEffects.add(effect);
	item->Checked = FEffects.contains(effect);
	InstallFrameCallback();
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::miFrameCounterClick(TObject *Sender)
{
	FFrameCounter = 0;
	InstallFrameCallback();
}

void __fastcall TForm1::miOff2Click(TObject *Sender)
{
	// Turn every effect off and stop the preview callback.
	FEffects.clear();
	FEffects.setMirror(false);
	TMenuItem *items[] = { miGrayMode, miSelectColor, miMirror, miSingleColorObject,
		miGecko, miDragonBall, miSpecificColor, miSkinColor, miColorMode,
		miBinaryMode, miBlockMode, miPlay, akePicture1 };
	for (unsigned i = 0; i < sizeof(items) / sizeof(items[0]); ++i)
		items[i]->Checked = false;
	capSetCallbackOnFrame(hwndVideo, NULL);
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::miFormatClick(TObject *Sender)
{
	const DWORD formatSize = capGetVideoFormatSize(hwndVideo);
	lblVideoFormatSize->Caption = "Video Format Size : " + AnsiString(formatSize);
	if (formatSize < sizeof(BITMAPINFOHEADER))
		return;
	std::vector<BYTE> buffer(formatSize);
	if (!capGetVideoFormat(hwndVideo, &buffer[0], formatSize))
		return;
	const BITMAPINFOHEADER &header = *reinterpret_cast<const BITMAPINFOHEADER*>(&buffer[0]);
	lblSize->Caption = "Size : " + AnsiString(header.biSize);
	lblWidth->Caption = "Width : " + AnsiString(header.biWidth);
	lblHeight->Caption = "Height : " + AnsiString(header.biHeight);
	lblPlanes->Caption = "Planes : " + AnsiString(header.biPlanes);
	lblBitCount->Caption = "Bit Count: " + AnsiString(header.biBitCount);
	lblCompression->Caption = "Compression : " + AnsiString(header.biCompression);
	lblSizeImage->Caption = "Image Size: " + AnsiString(header.biSizeImage);
	lblXPelsPerMeter->Caption = "X Pels Per Meter : " + AnsiString(header.biXPelsPerMeter);
	lblYPelsPerMeter->Caption = "Y Pels Per Meter : " + AnsiString(header.biYPelsPerMeter);
	lblColorUsed->Caption = "Color Used : " + AnsiString(header.biClrUsed);
	lblColorImportant->Caption = "Color Important: " + AnsiString(header.biClrImportant);
	RefreshVideoFormat();
}

// ---------------------------------------------------------------------------
// Color effects

void __fastcall TForm1::miGrayModeClick(TObject *Sender)
{
	if (!FGray)
	{
		const Yuv gray = { 0, 128, 128 };
		FGray = std::make_shared<TintEffect>(gray);
	}
	ToggleEffect(miGrayMode, FGray);
}

void __fastcall TForm1::miSelectColorClick(TObject *Sender)
{
	// Turning the tint on asks for its color; turning it off does not.
	if (FTint && FEffects.contains(FTint))
	{
		ToggleEffect(miSelectColor, FTint);
		return;
	}
	if (!ColorDialog1->Execute())
		return;
	const Yuv color = rgbToYuv(RgbOf(ColorDialog1->Color));
	if (FTint)
		FTint->setColor(color);
	else
		FTint = std::make_shared<TintEffect>(color);
	ToggleEffect(miSelectColor, FTint);
}

void __fastcall TForm1::miMirrorClick(TObject *Sender)
{
	FEffects.setMirror(!FEffects.mirror());
	miMirror->Checked = FEffects.mirror();
	InstallFrameCallback();
}

void __fastcall TForm1::miSpecificColorClick(TObject *Sender)
{
	if (!FSpecificColor)
		FSpecificColor = std::make_shared<ColorHighlightEffect>(FColorRange, rgbToYuv(kGreen));
	ToggleEffect(miSpecificColor, FSpecificColor);
}

void __fastcall TForm1::miSkinColorClick(TObject *Sender)
{
	if (!FSkinColor)
		FSkinColor = std::make_shared<ColorHighlightEffect>(FColorRange, rgbToYuv(kSkinTint));
	ToggleEffect(miSkinColor, FSkinColor);
}

void __fastcall TForm1::edMaxYChange(TObject *Sender)
{
	FColorRange->yMax = StrToIntDef(edMaxY->Text, 255);
}

void __fastcall TForm1::edMinYChange(TObject *Sender)
{
	FColorRange->yMin = StrToIntDef(edMinY->Text, 0);
}

void __fastcall TForm1::edMinUChange(TObject *Sender)
{
	FColorRange->uMin = StrToIntDef(edMinU->Text, 90);
}

void __fastcall TForm1::edMaxUChange(TObject *Sender)
{
	FColorRange->uMax = StrToIntDef(edMaxU->Text, 120);
}

void __fastcall TForm1::edMinVChange(TObject *Sender)
{
	FColorRange->vMin = StrToIntDef(edMinV->Text, 90);
}

void __fastcall TForm1::edMaxVChange(TObject *Sender)
{
	FColorRange->vMax = StrToIntDef(edMaxV->Text, 120);
}

// ---------------------------------------------------------------------------
// Objects

void __fastcall TForm1::miSingleColorObjectClick(TObject *Sender)
{
	if (!FFillRect)
		FFillRect = std::make_shared<FillRectEffect>(350, 40, 200, 50, FFillColor);
	ToggleEffect(miSingleColorObject, FFillRect);
}

void __fastcall TForm1::miColorClick(TObject *Sender)
{
	// Picks the color of the single color object.
	if (!ColorDialog1->Execute())
		return;
	FFillColor = rgbToYuv(RgbOf(ColorDialog1->Color));
	if (FFillRect)
		FFillRect->setColor(FFillColor);
}

void __fastcall TForm1::miGeckoClick(TObject *Sender)
{
	if (!FGecko)
		FGecko = std::make_shared<SpriteEffect>(
			std::make_shared<Sprite>(PictureOf(imGecko, true, kWhite), 50, 50));
	ToggleEffect(miGecko, FGecko);
}

void TForm1::LoadDragonBalls()
{
	if (FDragonBallImages[0])
		return;
	TImage *images[7] = { imDragonBall0, imDragonBall1, imDragonBall2,
		imDragonBall3, imDragonBall4, imDragonBall5, imDragonBall6 };
	for (int i = 0; i < 7; ++i)
		FDragonBallImages[i] = PictureOf(images[i], true, kPureRed);
}

void TForm1::SelectDragonBall(int index)
{
	FDragonBallIndex = index;
	if (FDragonBall)
		FDragonBall->setImage(FDragonBallImages[index]);
}

void __fastcall TForm1::miDragonBallClick(TObject *Sender)
{
	if (!FDragonBallEffect)
	{
		LoadDragonBalls();
		FDragonBall = std::make_shared<Sprite>(FDragonBallImages[FDragonBallIndex], 210, 80);
		FDragonBallEffect = std::make_shared<SpriteEffect>(FDragonBall);
	}
	ToggleEffect(miDragonBall, FDragonBallEffect);
}

// Clicking a dragon ball picture selects which one is shown.
void __fastcall TForm1::imDragonBall0Click(TObject *Sender) { SelectDragonBall(0); }
void __fastcall TForm1::imDragonBall1Click(TObject *Sender) { SelectDragonBall(1); }
void __fastcall TForm1::imDragonBall2Click(TObject *Sender) { SelectDragonBall(2); }
void __fastcall TForm1::imDragonBall3Click(TObject *Sender) { SelectDragonBall(3); }
void __fastcall TForm1::imDragonBall4Click(TObject *Sender) { SelectDragonBall(4); }
void __fastcall TForm1::imDragonBall5Click(TObject *Sender) { SelectDragonBall(5); }
void __fastcall TForm1::imDragonBall6Click(TObject *Sender) { SelectDragonBall(6); }

// ---------------------------------------------------------------------------
// Background and difference detection

void TForm1::ShowLearnedBackground()
{
	const ChangeDetector &detector = FEffects.detector();
	const int width = detector.backgroundWidth();
	const int height = detector.backgroundHeight();
	imBackground->Width = width;
	imBackground->Height = height;
	imBackground->Picture->Bitmap->PixelFormat = pf24bit;
	imBackground->Picture->Bitmap->Width = width;
	imBackground->Picture->Bitmap->Height = height;
	for (int y = 0; y < height; ++y)
	{
		BYTE *row = static_cast<BYTE*>(imBackground->Picture->Bitmap->ScanLine[y]);
		for (int x = 0; x < width; ++x)
		{
			const Rgb c = yuvToRgb(detector.backgroundAt(x, y));
			row[x * 3 + 0] = c.b;
			row[x * 3 + 1] = c.g;
			row[x * 3 + 2] = c.r;
		}
	}
	imBackground->Refresh();
}

void __fastcall TForm1::miBackgroundClick(TObject *Sender)
{
	// Average the next 30 frames into the background, then show it.
	pcWebcam->TabIndex = 2;
	FEffects.startBackgroundLearning(30, [this]() { ShowLearnedBackground(); });
	InstallFrameCallback();
}

void TForm1::SelectDifferenceView(DifferenceViewEffect::Mode mode)
{
	// The three views are exclusive; choosing the active one turns it off.
	const bool turningOff = FDifferenceView && FEffects.contains(FDifferenceView) &&
		FDifferenceView->mode() == mode;
	if (!FDifferenceView)
		FDifferenceView = std::make_shared<DifferenceViewEffect>(mode);
	FDifferenceView->setMode(mode);
	if (turningOff)
		FEffects.remove(FDifferenceView);
	else
		FEffects.add(FDifferenceView);
	miColorMode->Checked = !turningOff && mode == DifferenceViewEffect::kColor;
	miBinaryMode->Checked = !turningOff && mode == DifferenceViewEffect::kBinary;
	miBlockMode->Checked = !turningOff && mode == DifferenceViewEffect::kBlocks;
	InstallFrameCallback();
}

void __fastcall TForm1::miColorModeClick(TObject *Sender)
{
	SelectDifferenceView(DifferenceViewEffect::kColor);
}

void __fastcall TForm1::miBinaryModeClick(TObject *Sender)
{
	SelectDifferenceView(DifferenceViewEffect::kBinary);
}

void __fastcall TForm1::miBlockModeClick(TObject *Sender)
{
	SelectDifferenceView(DifferenceViewEffect::kBlocks);
}

void __fastcall TForm1::edThresholdChange(TObject *Sender)
{
	FEffects.detector().setThreshold(StrToIntDef(edThreshold->Text, ChangeDetector::kDefaultThreshold));
}

// ---------------------------------------------------------------------------
// Interaction: virtual spotted gecko

void __fastcall TForm1::miPlayClick(TObject *Sender)
{
	if (!FGeckoGame)
	{
		FGameGecko = std::make_shared<Sprite>(PictureOf(imGecko, true, kWhite));
		FGeckoGame = std::make_shared<TouchToHideEffect>(FGameGecko);
	}
	if (!FEffects.contains(FGeckoGame))
	{
		WarnIfNoBackground();
		FGameGecko->moveTo(50, 50);
		FGameGecko->show();
	}
	ToggleEffect(miPlay, FGeckoGame);
}

void __fastcall TForm1::miReplayClick(TObject *Sender)
{
	// Bring the gecko back somewhere random inside the frame.
	if (!FGameGecko)
		return;
	const int roomX = FVideoWidth - FGameGecko->width();
	const int roomY = FVideoHeight - FGameGecko->height();
	FGameGecko->moveTo(roomX > 0 ? std::rand() % (roomX + 1) : 0,
		roomY > 0 ? std::rand() % (roomY + 1) : 0);
	FGameGecko->show();
}

// ---------------------------------------------------------------------------
// Interaction: take a picture with a virtual background

void TForm1::LoadBackgroundPictures()
{
	if (FBackgroundPictures[0])
		return;
	TImage *frames[3] = { Frame1, Frame2, Frame3 };
	for (int i = 0; i < 3; ++i)
		FBackgroundPictures[i] = PictureOf(frames[i], false, kWhite);
}

void TForm1::SelectBackgroundPicture(int index)
{
	FBackgroundIndex = index;
	if (FPhotoTrigger)
	{
		FColorKeyBackground->setPicture(FBackgroundPictures[index]);
		FReplaceBackground->setPicture(FBackgroundPictures[index]);
	}
}

std::shared_ptr<Effect> TForm1::ActiveVirtualBackground() const
{
	if (FUseColorKey)
		return FColorKeyBackground;
	return FReplaceBackground;
}

void TForm1::WarnIfNoBackground()
{
	const ChangeDetector &detector = FEffects.detector();
	if (!detector.hasBackground() && !detector.isLearning())
		ShowMessage("This needs a learned background. Step out of view and choose "
			"Preview Callback Function > Background first.");
}

void TForm1::SavePhoto(const Yuy2Frame& shown, const Yuy2Frame& captured)
{
	try
	{
		SaveFrameAsBitmap(shown, L"截圖(含背景).bmp");
		SaveFrameAsBitmap(captured, L"截圖(原始).bmp");
		Memo1->Lines->Add(L"截圖成功!");
	}
	catch (Exception &e)
	{
		Memo1->Lines->Add(String("Saving the picture failed: ") + e.Message);
	}
}

void __fastcall TForm1::akePicture1Click(TObject *Sender)
{
	if (!FPhotoTrigger)
	{
		LoadBackgroundPictures();
		FColorKeyBackground = std::make_shared<ChromaKeyEffect>(FColorRange,
			FBackgroundPictures[FBackgroundIndex]);
		FReplaceBackground = std::make_shared<BackgroundReplaceEffect>(
			FBackgroundPictures[FBackgroundIndex]);
		FCameraButton = std::make_shared<Sprite>(PictureOf(imCamera, true, kWhite));
		FPhotoTrigger = std::make_shared<TouchTriggerEffect>(FCameraButton,
			[this](const Yuy2Frame& shown, const Yuy2Frame& captured) {
				SavePhoto(shown, captured);
			});
	}

	const bool turningOn = !FEffects.contains(FPhotoTrigger);
	if (turningOn)
	{
		WarnIfNoBackground();
		// The camera button sits in the bottom-right corner.
		RefreshVideoFormat();
		FCameraButton->moveTo(std::max(0, FVideoWidth - FCameraButton->width() - 15),
			std::max(0, FVideoHeight - FCameraButton->height() - 5));
		FCameraButton->show();
		FEffects.add(ActiveVirtualBackground());
		FEffects.add(FPhotoTrigger);
	}
	else
	{
		FEffects.remove(FColorKeyBackground);
		FEffects.remove(FReplaceBackground);
		FEffects.remove(FPhotoTrigger);
	}
	akePicture1->Checked = turningOn;
	InstallFrameCallback();
}

void __fastcall TForm1::miColorKeyBackgroundClick(TObject *Sender)
{
	// Off: the person is cut out by comparing with the learned background.
	// On: pixels inside the Y/U/V range are replaced (needs a plain backdrop).
	const bool photoOn = FPhotoTrigger && FEffects.contains(FPhotoTrigger);
	if (photoOn)
		FEffects.remove(ActiveVirtualBackground());
	FUseColorKey = !FUseColorKey;
	miColorKeyBackground->Checked = FUseColorKey;
	if (photoOn)
		FEffects.add(ActiveVirtualBackground());
}

// Buttons 1-3 choose the virtual background.
void __fastcall TForm1::Button1Click(TObject *Sender) { SelectBackgroundPicture(0); }
void __fastcall TForm1::Button2Click(TObject *Sender) { SelectBackgroundPicture(1); }
void __fastcall TForm1::Button3Click(TObject *Sender) { SelectBackgroundPicture(2); }
// ---------------------------------------------------------------------------
