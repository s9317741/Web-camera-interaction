// ---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "homework1.h"
#include <vfw.h>

long dwDriverIndex; // 電腦的驅動程式代碼。
wchar_t szDeviceName[80];
wchar_t szDeviceVersion[80];

// 32-bit 電腦請改成以下形式之宣告:
// char szDeviceName[80];	// 驅動程式名稱 Driver Name。
// char szDeviceVersion[80];	// 驅動程式 Driver Version。

HWND hwndVideo; // 儲存所建立的視訊擷取視窗之代碼
HWND frameVideo;
wchar_t *szWindowName;
wchar_t wstrCaptureWindowName[20] = L"My Capture Window";
int iCapWindowX1; // 視訊擷取視窗左上角座標 ( iCapWindowX1, iCapWindowY1 ) 。
int iCapWindowY1;
int iCapWindowWidth;
int iCapWindowHeight;

// 公共變數宣告。
tagCapDriverCaps cdcCapDriverCapability; // tagCapDriverCaps 為結構資料型態。
tagCapStatus capWindowStatus;

// 公用變數宣告。
wchar_t wstrCaptureFileName[80]; // 錄影儲存之檔名。
CAPTUREPARMS cpCaptureParms; // CAPTUREPARMS 為錄影參數之結構資料型態。
long dwFileAlloc; // 錄影要預先配置的硬碟記憶體空間, 以位元組 (byte) 為單位。

// 公共變數宣告
BITMAPINFO bmiBitmapInfo;
int iVideoWidth, iVideoHeight;
BYTE *ptrCapture;
int iVideoFormatSize;

// 預覽回呼函式必須事先宣告於標頭檔。
LRESULT CALLBACK FrameCallbackCounter(HWND hwndVideo, PVIDEOHDR lpvhdr);
// 公共變數宣告
int i, j, k;
int iFrameCounter;

LRESULT CALLBACK FrameCallbackGrayMode(HWND hwndVideo, PVIDEOHDR lpvhdr);

LRESULT CALLBACK FrameCallbackColor(HWND hwndVideo, PVIDEOHDR lpvhdr);

// 預覽回呼函式必須事先宣告於標頭檔。
LRESULT CALLBACK FrameCallbackMirror(HWND hwndCapture, PVIDEOHDR lpvhdr);
// 公共變數宣告。
int iVideoMacroWidth; // iVideoMacroWidth: 表示一列有多少個宏像素 ( 視訊寬度的一半 )。
int iRowBufferSize; // iRowBufferSize: 為一列所需的記憶體大小, 宏像素個數乘以 4 。
int iPartner; // 用來儲存要交換對象的索引值。
unsigned char ucTempY1, ucTempU, ucTempY2, ucTempV; // 暫存待交換的色彩值。

// 回呼函數宣告。
LRESULT CALLBACK FrameCallbackSingleColorObject(HWND hwndCapture,
	PVIDEOHDR lpvhdr);
// 公共變數宣告。
int iObjectX1, iObjectY1;
int iObjectX2, iObjectY2;
int iObjectWidth, iObjectHeight;
int iR, iG, iB;
int iY, iU, iV;

unsigned char ucGeckoRGB[125][155][3];
unsigned char ucGeckoYUV[125][155][3];

unsigned char ucFrame1RGB[3][480][640][3];
unsigned char ucFrame1YUV[3][480][640][3];

unsigned char ucCameraRGB[125][125][3];
unsigned char ucCameraYUV[125][125][3];

int frameID=0, frameName = 1;

BYTE * ptrImage;

int iMinY = 125, iMaxY = 255;
int iMinU = 125, iMaxU = 255;
int iMinV = 120, iMaxV = 255;

LRESULT CALLBACK FrameCallbackSpecificColorFiltering(HWND hwndVideo,
	PVIDEOHDR lpvhdr);

typedef struct stVirtualObject // 虛擬物件結構資料型態名稱。
{
	int iID; // 虛擬物件編號
	int iX1; // 顯示位置左上角座標 ( X1, Y1 )
	int iY1;
	int iX2; // 顯示位置右下點座標 ( X2, Y2 )
	int iY2;
	int iHeight; // 虛擬物件之高
	int iWidth; // 虛擬物件之寬
	int iBlockX1; // 顯示位置左上角區塊座標 ( BlockX1, BlockY1 )
	int iBlockY1;
	int iBlockX2; // 顯示位置右下點區塊座標 ( BlockX2, BlockY2 )
	int iBlockY2;
	bool bVisible; // 是否顯示在視訊擷取視窗之中
	unsigned char ucBackgroundRed; // 虛擬物件背景色 ( 紅 )
	unsigned char ucBackgroundGreen; // 虛擬物件背景色 ( 綠 )
	unsigned char ucBackgroundBlue; // 虛擬物件背景色 ( 藍 )
} stObject; // 虛擬物件結構資料型態名稱。

stVirtualObject voGecko; // 宣告虛擬物件。
LRESULT CALLBACK FrameCallbackGecko(HWND hwndVideo, PVIDEOHDR lpvhdr);

unsigned char ucDragonBallRGB[7][50][50][3];
stVirtualObject voDragonBall;
int iDragonBallID;
unsigned char ucDragonBallYUV[7][50][50][3];

LRESULT CALLBACK FrameCallbackDragonBall(HWND hwndVideo, PVIDEOHDR lpvhdr);

int iTemp[480][640][3];
unsigned char ucBackground[480][640][3];

int iTempYUV[480][640][3];
unsigned char ucBackgroundYUV[480][640][3];
LRESULT CALLBACK FrameCallbackBackgroundRGB(HWND hwndVideo, PVIDEOHDR lpvhdr);
LRESULT CALLBACK FrameCallbackBackgroundYUY2(HWND hwndVideo, PVIDEOHDR lpvhdr);

int t, iC1, iD, iThreshold = 1600, iVDifference, iUDifference, iY1, iY2,
	iY1Difference, iY2Difference, iSquareDifferenceSum1, iSquareDifferenceSum2;
int iBlockChangeCounter[60][80];
LRESULT CALLBACK FrameCallbackDifferenceColorYUY2(HWND hwndCapture,
	PVIDEOHDR lpvhdr);
LRESULT CALLBACK FrameCallbackDifferenceBinaryYUY2(HWND hwndCapture,
	PVIDEOHDR lpvhdr);
LRESULT CALLBACK FrameCallbackDifferenceBlockYUY2(HWND hwndCapture,
	PVIDEOHDR lpvhdr);

#include <time.h>                // 使用clock(); 函式需要加入 time.h標頭檔
int m, n;

// 公共變數宣告
int iVideoBlockWidth, iVideoBlockHeight; // 視訊擷取視窗的區塊寬與區塊高

LRESULT CALLBACK FrameCallbackPlayWithGecko6A(HWND hwndCapture,
	PVIDEOHDR lpvhdr);
LRESULT CALLBACK FrameCallbackPlayWithGecko6B(HWND hwndCapture,
	PVIDEOHDR lpvhdr);

stVirtualObject vFrame1; // 宣告虛擬物件。
stVirtualObject vCamera; // 宣告虛擬物件。

// ---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm1 *Form1;

// ---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner) : TForm(Owner)
{
	Memo1->Clear();
	MediaPlayer1->FileName = "僕が死のうと思ったのは.mp3";
}

// ---------------------------------------------------------------------------
void __fastcall TForm1::On1Click(TObject *Sender)
{
	dwDriverIndex = 0;
	// 用來放TLabel元件的panel的Property Visible設為true,看得到了!
	pnlDriverDescription->Visible = true;
	if (capGetDriverDescription(dwDriverIndex, szDeviceName,
		sizeof(szDeviceName), szDeviceVersion, sizeof(szDeviceVersion)))
	{ // 當成功取得相關資訊會傳回true
		// 並將該字串變數內容顯示在lblCapDriverName, lblCapDriverVer
		lblCapDriverName->Caption = "Name: " + AnsiString(szDeviceName);
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
	iCapWindowWidth = 320; // 視訊擷取視窗的寬設定為 320。
	iCapWindowHeight = 240; // 視訊擷取視窗的高設定為 240。

	// 建立視訊擷取視窗。
	pnlCaptureWindow->Visible = true;
	// pnlCaptureWindow 為 Panel 物件的 Name , 用來放 capture window。
	hwndVideo = capCreateCaptureWindow(szWindowName, WS_CHILD | WS_VISIBLE,
		iCapWindowX1, iCapWindowY1, iCapWindowWidth, iCapWindowHeight,
		Form1->pnlCaptureWindow->Handle, 1);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Destroy1Click(TObject *Sender)
{
	// 要關閉視訊擷取視窗的方法, 如同要關閉一般視窗一樣,
	// 只要利用 Win32 API 的 DestroyWindow 函數,
	// 並告訴電腦你要關閉的視窗代碼 ( handle ) 即可。
	DestroyWindow(hwndVideo);
	pnlCaptureWindow->Visible = false;

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Connect1Click(TObject *Sender)
{
	// 將所建立的視訊擷取視窗與視訊裝置的驅動程式連結起來,
	// 連起來後, 視訊裝置所擷取到的畫面, 將來就會直接被送到視訊擷取視窗中顯示出來。
	capDriverConnect(hwndVideo, dwDriverIndex);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Disconnect1Click(TObject *Sender)
{
	// 將視訊裝置驅動程式與視訊擷取視窗之間的連結取消。
	capDriverDisconnect(hwndVideo);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Enable1Click(TObject *Sender)
{
	// 設定顯示模式為 Preview 模式。
	capPreview(hwndVideo, true);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Disable1Click(TObject *Sender)
{
	// 將 Preview 顯示模式取消。
	capPreview(hwndVideo, false);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::N30framesec1Click(TObject *Sender)
{
	// 設定顯示速率 33.3 miliseconds, or 30 FPS。
	capPreviewRate(hwndVideo, 33);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::N15framesec1Click(TObject *Sender)
{
	// 設定顯示速率 66 miliseconds, or 15 FPS。
	capPreviewRate(hwndVideo, 66);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::On2Click(TObject *Sender)
{
	// 將視訊畫面設定成可以隨著視訊擷取視窗大小而調整。
	capPreviewScale(hwndVideo, true);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Off2Click(TObject *Sender)
{
	// 取消設定視訊畫面隨著視訊擷取視窗大小而調整。
	capPreviewScale(hwndVideo, false);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::N3202401Click(TObject *Sender)
{
	// 調整視訊擷取視窗的大小為 320*240。
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
	// 調整視訊擷取視窗的大小為 640 * 480。
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
	// 取得視訊裝置驅動程式的效能。
	if (capDriverGetCaps(hwndVideo, &cdcCapDriverCapability,
		sizeof(cdcCapDriverCapability)))
	{
		lblDriverIndex->Caption = AnsiString("Driver Index:") +
			AnsiString(cdcCapDriverCapability.wDeviceIndex);
		if (cdcCapDriverCapability.fHasOverlay)
			lblOverlay->Caption = AnsiString(" Can device overlay ?  Yes! ");
		else
			lblOverlay->Caption = AnsiString(" Can device overlay ? No! ");
		// lblOverlay 為 Label 物件的 Name。

		if (cdcCapDriverCapability.fHasDlgVideoSource)
		{
			// miVideoSource 為 MainMenu 物件的 MenuItem 之名稱。
			// 將 MenuItem miVideoSource元件設定成可以使用的狀態。
			miVideoSource->Enabled = true;
			lblVideoSourceDialog->Caption =
				AnsiString(" Has video source dialog ? Yes! ");
		}
		else
		{
			// 將 MenuItem miVideoSource元件設定成失效, 無法使用的狀態。
			miVideoSource->Enabled = false;
			lblVideoSourceDialog->Caption =
				AnsiString(" Has video source dialog? No! ");
		}

		if (cdcCapDriverCapability.fHasDlgVideoFormat)
		{
			// miVideoFormat 為 MainMenu 物件的 MenuItem 之名稱。
			// 將 MenuItem miVideoFormat元件設定成可以使用的狀態。
			miVideoFormat->Enabled = true;
			lblVideoFormatDialog->Caption =
				AnsiString(" Has video format dialog? Yes! ");
		}
		else
		{
			// 將 MenuItem miVideoFormat元件設定成失效, 無法使用的狀態。
			miVideoFormat->Enabled = false;
			lblVideoFormatDialog->Caption =
				AnsiString(" Has video format dialog ? No ");
		}

		if (cdcCapDriverCapability.fHasDlgVideoDisplay)
		{
			// miVideoDisplay 為 MainMenu 物件下的 MenuItem 之名稱。
			// 將 MenuItem miVideoDisplay 元件設定成可以使用的狀態。
			miVideoDisplay->Enabled = true;
			lblVideoDisplayDialog->Caption =
				AnsiString(" Has video display dialog ? Yes ");
		}
		else
		{
			// 將 MenuItem miVideoDisplay 元件設定成失效, 無法使用的狀態。
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

		if (cdcCapDriverCapability.fCaptureInitialized)
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
	// 開啟視訊來源對話盒, 如果無法開啟就秀出訊息通知。
	if (!capDlgVideoSource(hwndVideo))
		ShowMessage(" Can't open video source dialog !! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miVideoFormatClick(TObject *Sender)
{
	// 開啟視訊格式對話盒, 如果無法開啟就秀出訊息通知。
	if (!capDlgVideoFormat(hwndVideo))
		ShowMessage(" Can't open video format dialog ! ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miVideoDisplayClick(TObject *Sender)
{
	// 開啟視訊顯示對話盒, 如果無法開啟就秀出訊息通知。
	if (!capDlgVideoDisplay(hwndVideo))
		ShowMessage(" Can't open video display dialog ! ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miVideoCompressionClick(TObject *Sender)
{
	// 開啟視訊壓縮對話盒, 如果無法開啟就秀出訊息通知。
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

	// 建立視訊擷取視窗。
	pnlCaptureWindow->Visible = true;
	// pnlCaptureWindow 為 Panel 物件的 Name , 用來放 capture window。
	hwndVideo = capCreateCaptureWindow(szWindowName, WS_CHILD | WS_VISIBLE,
		iCapWindowX1, iCapWindowY1, iCapWindowWidth, iCapWindowHeight,
		Form1->pnlCaptureWindow->Handle, 1);
	capDriverConnect(hwndVideo, dwDriverIndex);
	capPreview(hwndVideo, true);
	capPreviewRate(hwndVideo, 33);
	capPreviewScale(hwndVideo, true);
	pcWebcam->TabIndex = 0;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnGrabFrameClick(TObject *Sender)
{
	// 擷取單張畫面到記憶體緩衝區之中, 視訊擷取視窗的畫面會暫時靜止,
	// 直到重新啟動 Preview or Overlay 顯示模式。
	// 靜止的好處是可以觀察取得的畫面是否滿意!
	if (!capGrabFrame(hwndVideo))
		ShowMessage(" Can't Grab Frame to the video frame buffer ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnGrabFrameNoStopClick(TObject *Sender)
{
	// 擷取單張畫面到記憶體緩衝區之中, 視訊擷取視窗的畫面不會暫時靜止。
	if (!capGrabFrameNoStop(hwndVideo))
		ShowMessage(" Can't Grab Frame to the Video Frame Buffre ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnEditCopyClick(TObject *Sender)
{
	// 將記憶體緩衝區中的影像資料複製到剪貼簿, 可供其他應用程式也能透過剪貼簿取得。
	if (!capEditCopy(hwndVideo))
		ShowMessage(" Can't Copy the Video Frame Buffre to the Clipboard ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnFileSaveDIBClick(TObject *Sender)
{
	// 分成兩個步驟:
	// 1. 先透過 SavePictureDialog1 元件輸入要儲存的 bmp 檔名。
	// 2. 在透過 capFileSaveDIB 函式直接存檔。
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
	// 取得視訊裝置驅動程式的效能。
	if (capGetStatus(hwndVideo, &capWindowStatus, sizeof(capWindowStatus)))
	{
		lblImageWidth->Caption = AnsiString("Image width:") +
			AnsiString(capWindowStatus.uiImageWidth);
		lblImageHeight->Caption = AnsiString("Image width:") +
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
	// 設定所要儲存的錄影檔案名稱。
	if (SaveDialog1->Execute())
	{
		wcscpy(wstrCaptureFileName, SaveDialog1->FileName.t_str());
		ShowMessage(AnsiString(wstrCaptureFileName));
		if (!capFileSetCaptureFile(hwndVideo, SaveDialog1->FileName.t_str()))
		{
			ShowMessage("Can't Set the Capture Filename ! ");
			ShowMessage(SaveDialog1->FileName.t_str());
		}
		if (capFileGetCaptureFile(hwndVideo, wstrCaptureFileName,
			sizeof(wstrCaptureFileName)))
			lblCaptureFilename->Caption = AnsiString(wstrCaptureFileName);
		else
			ShowMessage("Can't Get the Capture Filename ! ");
	}

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::edFileAllocChange(TObject *Sender)
{
	// 設定預先配置視訊檔之磁碟空間大小, 以 Mega Bytes為單位。
	dwFileAlloc = 1024 * 1024 * StrToIntDef(edFileAlloc->Text, 0);

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnFileAllocClick(TObject *Sender)
{
	// 配置預先設定之磁碟空間大小。
	if (!capFileAlloc(hwndVideo, dwFileAlloc))
		ShowMessage("Can't Allocate " + edFileAlloc->Text + "MB ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::btnCaptureSequenceClick(TObject *Sender)
{
	// 開始錄影, 如果錄影失敗則顯示訊息通知。
	if (!capCaptureSequence(hwndVideo))
		ShowMessage("Can't Capture Sequence ! ");

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miFrameCounterClick(TObject *Sender)
{
	// 將iFrameCounter 值重設為0。
	iFrameCounter = 0;
	// 開啟預覽回呼函式 FrameCallbackCounter。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackCounter);

}

void __fastcall TForm1::miOff2Click(TObject *Sender)
{
	// 關閉預覽回呼函式。
	capSetCallbackOnFrame(hwndVideo, NULL);

}

// 預覽回呼函式: 當程式開啟視訊擷取視窗後, 顯示預覽的畫面總數到 TLabel 元件上。
LRESULT CALLBACK FrameCallbackCounter(HWND hwndVideo, PVIDEOHDR lpvhdr)
{
	// 每執行一次預覽回呼函式, 就將 iFrameCounter 加 1。
	iFrameCounter++;
	// 將預覽的畫面總數顯示在 Label lblFrameCounter元件之上。
	Form1->lblFrameCounter->Caption = AnsiString(iFrameCounter);
	return (0);
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::miFormatClick(TObject *Sender)
{
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	lblVideoFormatSize->Caption = "Video Format Size : " +
		AnsiString(iVideoFormatSize);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	lblSize->Caption = "Size : " + AnsiString(bmiBitmapInfo.bmiHeader.biSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	lblWidth->Caption = "Width : " + AnsiString(iVideoWidth);
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	lblHeight->Caption = "Height : " + AnsiString(iVideoHeight);
	lblPlanes->Caption = "Planes : " + AnsiString
		(bmiBitmapInfo.bmiHeader.biPlanes);
	lblBitCount->Caption = "Bit Count: " + AnsiString
		(bmiBitmapInfo.bmiHeader.biBitCount);
	lblCompression->Caption = "Compression : " +
		AnsiString(bmiBitmapInfo.bmiHeader.biCompression);
	lblSizeImage->Caption = "Image Size: " + AnsiString
		(bmiBitmapInfo.bmiHeader.biSizeImage);
	lblXPelsPerMeter->Caption = "X Pels Per Meter : " +
		AnsiString(bmiBitmapInfo.bmiHeader.biXPelsPerMeter);
	lblYPelsPerMeter->Caption = "Y Pels Per Meter : " +
		AnsiString(bmiBitmapInfo.bmiHeader.biYPelsPerMeter);
	lblColorUsed->Caption = "Color Used : " +
		AnsiString(bmiBitmapInfo.bmiHeader.biClrUsed);
	lblColorImportant->Caption = "Color Important: " +
		AnsiString(bmiBitmapInfo.bmiHeader.biClrImportant);

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miGrayModeClick(TObject *Sender)
{
	// 設定 iFrameCounter 變數的初始值。
	iFrameCounter = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight,
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoMacroWidth = iVideoWidth / 2;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	// 開啟預覽回呼函式FrameCallbackGrayMode。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackGrayMode);
}

// Video Format 為 YUY2 的情況。
// 預覽回呼函式: 當程式開啟視訊擷取視窗後, 將預覽畫面變成灰階畫面。
LRESULT CALLBACK FrameCallbackGrayMode(HWND hwndVideo, PVIDEOHDR lpvhdr)
{
	ptrCapture = lpvhdr->lpData;
	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoMacroWidth; i++)
		{
			// 只要將每一個宏像素中的 U、V 色彩値改成 128, 就會呈現灰階畫面。
			ptrCapture[k + 1] = 128;
			ptrCapture[k + 3] = 128;
			k = k + 4;
		}
	}
	return (0);
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::miMirrorClick(TObject *Sender)
{
	// 設定 iFrameCounter 變數的初始值。
	iFrameCounter = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight ,
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	iVideoMacroWidth = iVideoWidth / 2;
	iRowBufferSize = iVideoMacroWidth * 4;
	// 開啟預覽回呼函式FrameCallbackMirror。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackMirror);

}

LRESULT CALLBACK FrameCallbackMirror(HWND hwndCapture, PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	ptrCapture = lpvhdr->lpData;
	// iVideoMacroWidth: 表示一列有多少個宏像素 ( 視訊寬度的一半 )。
	// iRowBufferSize: 為一列所需的記憶體大小, 宏像素個數乘以 4 。
	// 處理鏡射問題。
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoMacroWidth / 2; i++)
		{
			// 第 i 個 macropixel 的交換對象是第 ( iVideoWidth / 2 ) - i 。
			iPartner = iVideoMacroWidth - i - 1;
			// 先將第 i 個 marcopixel 的四個位元組的色彩值暫存起來。
			ucTempY1 = ptrCapture[j * iRowBufferSize + i * 4];
			ucTempU = ptrCapture[j * iRowBufferSize + i * 4 + 1];
			ucTempY2 = ptrCapture[j * iRowBufferSize + i * 4 + 2];
			ucTempV = ptrCapture[j * iRowBufferSize + i * 4 + 3];
			// 把 Partner 的四個位元組的色彩值複製過來, 但 Y1 與 Y2 要互換位置。
			ptrCapture[j * iRowBufferSize + i * 4] =
				ptrCapture[j * iRowBufferSize + iPartner * 4 + 2];
			ptrCapture[j * iRowBufferSize + i * 4 + 1] =
				ptrCapture[j * iRowBufferSize + iPartner * 4 + 1];
			ptrCapture[j * iRowBufferSize + i * 4 + 2] =
				ptrCapture[j * iRowBufferSize + iPartner * 4];
			ptrCapture[j * iRowBufferSize + i * 4 + 3] =
				ptrCapture[j * iRowBufferSize + iPartner * 4 + 3];
			// 將暫存的色彩值放到 Partner 的緩衝區記憶體之中, 但 Y1 與 Y2 要互換位置。
			ptrCapture[j * iRowBufferSize + iPartner * 4] = ucTempY2;
			ptrCapture[j * iRowBufferSize + iPartner * 4 + 1] = ucTempU;
			ptrCapture[j * iRowBufferSize + iPartner * 4 + 2] = ucTempY1;
			ptrCapture[j * iRowBufferSize + iPartner * 4 + 3] = ucTempV;
		}
	}
	return (0);
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::miSingleColorObjectClick(TObject *Sender)
{
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight。
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	// 設定要填入的區域。
	iObjectX1 = 350;
	iObjectY1 = 40;
	iObjectWidth = 200;
	iObjectHeight = 50;
	iObjectX2 = iObjectX1 + iObjectWidth;
	iObjectY2 = iObjectY1 + iObjectHeight;
	// 設定要填入的色彩。
	iR = 0;
	iG = 255;
	iB = 0;
	iY = ((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16;
	iU = ((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128;
	iV = ((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128;
	// 開啟預覽回呼函式。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackSingleColorObject);

}

LRESULT CALLBACK FrameCallbackSingleColorObject(HWND hwndVideo,
	PVIDEOHDR lpvhdr)
{
	// 取得視訊擷取視窗的緩衝區記憶體的起始位址。
	ptrCapture = lpvhdr->lpData;

	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			// 在座標 ( iObjectX1, iObjectY1 ) 與 ( iObjectX2, iObjectY2 ) 之間的區域要將色彩値改掉。
			if ((i > iObjectX1) && (i < iObjectX2) && (j > iObjectY1) && (j <
				iObjectY2))
			{
				ptrCapture[k + 0] = iY;
				ptrCapture[k + 1] = iU;
				ptrCapture[k + 2] = iY;
				ptrCapture[k + 3] = iV;
			}
			k = k + 4;
		}
	}
	Form1->lblFrameCounter->Caption = AnsiString(iFrameCounter);
	return (0);
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::miSelectColorClick(TObject *Sender)
{
	// 透過 TColorDiallog 元件來選定要填入視訊擷取視窗的顏色。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoMacroWidth = iVideoWidth / 2;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	if (ColorDialog1->Execute())
	{
		iR = GetRValue(ColorDialog1->Color);
		iG = GetGValue(ColorDialog1->Color);
		iB = GetBValue(ColorDialog1->Color);
		// 將 RGB 色彩轉換成 YUV。
		iY = ((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16;
		iU = ((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128;
		iV = ((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128;
	}
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackColor);
}

// ---------------------------------------------------------------------------
LRESULT CALLBACK FrameCallbackColor(HWND hwndVideo, PVIDEOHDR lpvhdr)
{
	ptrCapture = lpvhdr->lpData;
	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoMacroWidth; i++)
		{
			ptrCapture[k + 1] = iU;
			ptrCapture[k + 3] = iV;
			k = k + 4;
		}
	}
	return (0);
}

void __fastcall TForm1::miColorClick(TObject *Sender)
{
	if (ColorDialog1->Execute())
	{
		iR = GetRValue(ColorDialog1->Color);
		iG = GetGValue(ColorDialog1->Color);
		iB = GetBValue(ColorDialog1->Color);
		// 將 RGB 色彩轉換成 YUV。
		iY = ((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16;
		iU = ((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128;
		iV = ((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128;

	}
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miGeckoClick(TObject *Sender)
{
	// 因為 FrameCallBackGecko 需要用到 iVideoWidth, iVideoHeight。
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoMacroWidth = iVideoWidth / 2;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	// 設定虛擬物件的各項屬性。
	voGecko.iX1 = 50;
	voGecko.iY1 = 50;
	voGecko.iWidth = 155;
	voGecko.iHeight = 125;
	voGecko.iX2 = voGecko.iX1 + voGecko.iWidth - 1;
	voGecko.iY2 = voGecko.iY1 + voGecko.iHeight - 1;
	voGecko.ucBackgroundRed = 255;
	voGecko.ucBackgroundGreen = 255;
	voGecko.ucBackgroundBlue = 255;
	// 將影像元件中的色彩資料放到陣列中
	for (j = 0; j < voGecko.iHeight; j++)
	{
		ptrImage = (BYTE*) imGecko->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < voGecko.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucGeckoRGB[j][i][0] = (unsigned char) iB;
			ucGeckoRGB[j][i][1] = (unsigned char) iG;
			ucGeckoRGB[j][i][2] = (unsigned char) iR;
			ucGeckoYUV[j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucGeckoYUV[j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucGeckoYUV[j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
	}
	// 開啟預覽回呼函式 FrameCallbackGecko。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackGecko);

}

LRESULT CALLBACK FrameCallbackGecko(HWND hwndVideo, PVIDEOHDR lpvhdr)
{
	// 取得視訊擷取視窗的緩衝區記憶體的起始位址。
	ptrCapture = lpvhdr->lpData;

	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			// 在座標 ( iObjectX1, iObjectY1 ) 與 ( iObjectX2, iObjectY2 ) 之間的區域要置入Gecko。
			if ((i >= voGecko.iX1) && (i < voGecko.iX2) && (j >= voGecko.iY1) &&
				(j < voGecko.iY2))
			{
				// Gecko 圖片的背景區域必須忽略, 無須置入。
				if ((ucGeckoRGB[j - voGecko.iY1][i - voGecko.iX1][0]
					!= voGecko.ucBackgroundBlue) ||
					(ucGeckoRGB[j - voGecko.iY1][i - voGecko.iX1][1]
					!= voGecko.ucBackgroundGreen) ||
					(ucGeckoRGB[j - voGecko.iY1][i - voGecko.iX1][2]
					!= voGecko.ucBackgroundRed))
				{
					ptrCapture[k] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1][0];
					ptrCapture[k + 1] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1][1];
					ptrCapture[k + 2] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1 + 1][0];
					ptrCapture[k + 3] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1][2];
				}
			}
			k = k + 4;
		}
	}
	// 結束預覽回呼函數之執行。
	return (0);
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::miDragonBallClick(TObject *Sender)
{
	iFrameCounter = 0;
	iDragonBallID = 3;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight，
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	// 設定虛擬龍珠出現的初始位置。
	voDragonBall.iX1 = 210;
	voDragonBall.iY1 = 80;
	voDragonBall.iWidth = 50;
	voDragonBall.iHeight = 50;
	voDragonBall.iX2 = voDragonBall.iX1 + voDragonBall.iWidth - 1;
	voDragonBall.iY2 = voDragonBall.iY1 + voDragonBall.iHeight - 1;

	// 將影像元件中的色彩資料放到陣列中
	for (j = 0; j < voDragonBall.iHeight; j++)
	{
		// 從影像元件讀取一星龍珠的色彩資料到影像陣列之中。
		ptrImage = (BYTE*)imDragonBall0->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < voDragonBall.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucDragonBallRGB[0][j][i][0] = (unsigned char) iB;
			ucDragonBallRGB[0][j][i][1] = (unsigned char) iG;
			ucDragonBallRGB[0][j][i][2] = (unsigned char) iR;
			ucDragonBallYUV[0][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucDragonBallYUV[0][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucDragonBallYUV[0][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
		// 從影像元件讀取二星龍珠的色彩資料到影像陣列之中。
		ptrImage = (BYTE*) imDragonBall1->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < voDragonBall.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucDragonBallRGB[1][j][i][0] = (unsigned char) iB;
			ucDragonBallRGB[1][j][i][1] = (unsigned char) iG;
			ucDragonBallRGB[1][j][i][2] = (unsigned char) iR;
			ucDragonBallYUV[1][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucDragonBallYUV[1][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucDragonBallYUV[1][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
		// 從影像元件讀取三星龍珠的色彩資料到影像陣列之中。
		ptrImage = (BYTE*) imDragonBall2->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < voDragonBall.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucDragonBallRGB[2][j][i][0] = (unsigned char) iB;
			ucDragonBallRGB[2][j][i][1] = (unsigned char) iG;
			ucDragonBallRGB[2][j][i][2] = (unsigned char) iR;
			ucDragonBallYUV[2][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucDragonBallYUV[2][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucDragonBallYUV[2][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
		// 從影像元件讀取四星龍珠的色彩資料到影像陣列之中。
		ptrImage = (BYTE*) imDragonBall3->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < voDragonBall.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucDragonBallRGB[3][j][i][0] = (unsigned char) iB;
			ucDragonBallRGB[3][j][i][1] = (unsigned char) iG;
			ucDragonBallRGB[3][j][i][2] = (unsigned char) iR;
			ucDragonBallYUV[3][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucDragonBallYUV[3][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucDragonBallYUV[3][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
		// 從影像元件讀取五星龍珠的色彩資料到影像陣列之中。
		ptrImage = (BYTE*)imDragonBall4->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < voDragonBall.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucDragonBallRGB[4][j][i][0] = (unsigned char) iB;
			ucDragonBallRGB[4][j][i][1] = (unsigned char) iG;
			ucDragonBallRGB[4][j][i][2] = (unsigned char) iR;
			ucDragonBallYUV[4][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucDragonBallYUV[4][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucDragonBallYUV[4][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
		// 從影像元件讀取六星龍珠的色彩資料到影像陣列之中。
		ptrImage = (BYTE*) imDragonBall5->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < voDragonBall.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucDragonBallRGB[5][j][i][0] = (unsigned char) iB;
			ucDragonBallRGB[5][j][i][1] = (unsigned char) iG;
			ucDragonBallRGB[5][j][i][2] = (unsigned char) iR;
			ucDragonBallYUV[5][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucDragonBallYUV[5][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucDragonBallYUV[5][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
		// 從影像元件讀取七星龍珠的色彩資料到影像陣列之中。
		ptrImage = (BYTE*)imDragonBall6->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < voDragonBall.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucDragonBallRGB[6][j][i][0] = (unsigned char) iB;
			ucDragonBallRGB[6][j][i][1] = (unsigned char) iG;
			ucDragonBallRGB[6][j][i][2] = (unsigned char) iR;
			ucDragonBallYUV[6][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucDragonBallYUV[6][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucDragonBallYUV[6][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
	}
	// 開啟預覽回呼函數FrameCallbackDragonBall。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackDragonBall);
}

LRESULT CALLBACK FrameCallbackDragonBall(HWND hwndVideo, PVIDEOHDR lpvhdr)
{
	ptrCapture = lpvhdr->lpData;

	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			// 在座標 ( iX1, iY1 ) 與 ( iX2, iY2 ) 之間的區域要置入龍珠圖片。
			if ((i >= voDragonBall.iX1) && (i < voDragonBall.iX2) && (j >=
				voDragonBall.iY1) && (j < voDragonBall.iY2))
			{
				// 龍珠圖片的背景區域必須忽略, 不須置入。
				if ((ucDragonBallRGB[iDragonBallID][j - voDragonBall.iY1][i -
					voDragonBall.iX1][0] != 0) ||
					(ucDragonBallRGB[iDragonBallID][j - voDragonBall.iY1][i -
					voDragonBall.iX1][1] != 0) ||
					(ucDragonBallRGB[iDragonBallID][j - voDragonBall.iY1][i -
					voDragonBall.iX1][2] != 255))
				{
					ptrCapture[k] =
						ucDragonBallYUV[iDragonBallID][j - voDragonBall.iY1]
						[i - voDragonBall.iX1][0];
					ptrCapture[k + 1] =
						ucDragonBallYUV[iDragonBallID][j - voDragonBall.iY1]
						[i - voDragonBall.iX1][1];
					ptrCapture[k + 2] =
						ucDragonBallYUV[iDragonBallID][j - voDragonBall.iY1]
						[i - voDragonBall.iX1 + 1][0];
					ptrCapture[k + 3] =
						ucDragonBallYUV[iDragonBallID][j - voDragonBall.iY1]
						[i - voDragonBall.iX1][2];
				}
			}
			k = k + 4;
		}
	}
	Form1->lblFrameCounter->Caption = AnsiString(iFrameCounter);
	return (0);
}

// ---------------------------------------------------------------------------
void __fastcall TForm1::imDragonBall0Click(TObject *Sender)
{
	// 點選一星龍珠圖片來選定要出現在是訊擷取視窗的是一星龍珠。
	iDragonBallID = 0;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::imDragonBall1Click(TObject *Sender)
{
	iDragonBallID = 1;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::imDragonBall2Click(TObject *Sender)
{
	iDragonBallID = 2;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::imDragonBall3Click(TObject *Sender)
{
	iDragonBallID = 3;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::imDragonBall4Click(TObject *Sender)
{
	iDragonBallID = 4;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::imDragonBall5Click(TObject *Sender)
{
	iDragonBallID = 5;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::imDragonBall6Click(TObject *Sender)
{
	iDragonBallID = 6;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miSpecificColorClick(TObject *Sender)
{
	iFrameCounter = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	iVideoMacroWidth = iVideoWidth / 2;
	// 因為要將追蹤的顏色套上綠色的光影, 所以需要綠色的 U, V 値
	iR = 0;
	iG = 255; // 綠色。
	iB = 0;
	iY = ((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16;
	iU = ((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128;
	iV = ((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128;
	// 開啟預覽回呼函式 ( 設定為 FrameCallbackSpecificColorFiltering ) 。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackSpecificColorFiltering);

}

LRESULT CALLBACK FrameCallbackSpecificColorFiltering(HWND hwndVideo,
	PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	ptrCapture = lpvhdr->lpData;

	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoMacroWidth; i++)
		{
			if ((ptrCapture[k] > iMinY) && (ptrCapture[k] < iMaxY) &&
				(ptrCapture[k + 1] > iMinU) && (ptrCapture[k + 1] < iMaxU) &&
				(ptrCapture[k + 2] > iMinY) && (ptrCapture[k + 2] < iMaxY) &&
				(ptrCapture[k + 3] > iMinV) && (ptrCapture[k + 3] < iMaxV))
			{
				ptrCapture[k + 1] = iU;
				ptrCapture[k + 3] = iV;
			}
			k = k + 4;
		}
	}

	Form1->lblFrameCounter->Caption = AnsiString(iFrameCounter);
	return (0);
}

// ---------------------------------------------------------------------------
void __fastcall TForm1::miSkinColorClick(TObject *Sender)
{
	iFrameCounter = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	iVideoMacroWidth = iVideoWidth / 2;
	// 因為要將追蹤的顏色套上綠色的光影, 所以需要綠色的 U, V 値
	iR = 95;
	iG = 40; // 綠色。
	iB = 20;
	iY = ((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16;
	iU = ((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128;
	iV = ((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128;
	// 開啟預覽回呼函式 ( 設定為 FrameCallbackSpecificColorFiltering ) 。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackSpecificColorFiltering);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::edMaxYChange(TObject *Sender)
{
	iMaxY = StrToIntDef(edMaxY->Text, 255);
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::edMinYChange(TObject *Sender)
{
	iMinY = StrToIntDef(edMinY->Text, 0);
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::edMinUChange(TObject *Sender)
{
	iMinU = StrToIntDef(edMinU->Text, 90);
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::edMaxUChange(TObject *Sender)
{
	iMaxU = StrToIntDef(edMaxU->Text, 120);
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::edMinVChange(TObject *Sender)
{
	iMinV = StrToIntDef(edMinV->Text, 90);
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::edMaxVChange(TObject *Sender)
{
	iMaxV = StrToIntDef(edMaxV->Text, 120);
}

// ---------------------------------------------------------------------------
LRESULT CALLBACK FrameCallbackBackgroundRGB(HWND hwndVideo, PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	ptrCapture = lpvhdr->lpData;

	if (iFrameCounter > 30)
	{
		// 完成累加 30 張影像, 計算平均影像。
		for (j = 0; j < iVideoHeight; j++)
		{
			// 取得 imBackground 影像元件第 j 個 row 的儲存色彩値的記憶體起始位址。
			ptrImage =
				(BYTE*) Form1->imBackground->Picture->Bitmap->ScanLine[j];
			t = 0;
			for (i = 0; i < iVideoWidth; i++)
			{
				// 將累積陣列 iTemp 中的色彩値除以 30, 即為平均值。
				ucBackground[j][i][0] = (unsigned char)(iTemp[j][i][0] / 30);
				ucBackground[j][i][1] = (unsigned char)(iTemp[j][i][1] / 30);
				ucBackground[j][i][2] = (unsigned char)(iTemp[j][i][2] / 30);
				// 將陣列 ucBackground 的色彩資料,  放進影像元件 imBackgroung 的記憶體中, 準備顯示。
				ptrImage[t + 2] = ucBackground[j][i][0];
				ptrImage[t + 1] = ucBackground[j][i][1];
				ptrImage[t] = ucBackground[j][i][2];
				t = t + 3;
			}
		}
		// 更新影像元件 imBackgroung 的顯示內容, 使其與記憶體中內容一致。
		Form1->imBackground->Refresh();
		// 背景影像建立完成, 關閉預覽回呼函式。
		capSetCallbackOnFrame(hwndVideo, NULL);
	}
	else // ( iFrameCounter <= 30 )
	{
		k = 0;
		for (j = iVideoHeight - 1; j >= 0; j--)
			for (i = 0; i < iVideoWidth; i++)
			{
				// 將視訊擷取視窗的畫面累加到 iTemp 陣列之中。
				iTemp[j][i][0] = iTemp[j][i][0] + ptrCapture[k + 2];
				iTemp[j][i][1] = iTemp[j][i][1] + ptrCapture[k + 1];
				iTemp[j][i][2] = iTemp[j][i][2] + ptrCapture[k];
				k = k + 3;
			}
	}

	return (0);
}

LRESULT CALLBACK FrameCallbackBackgroundYUY2(HWND hwndVideo, PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	ptrCapture = lpvhdr->lpData;

	if (iFrameCounter > 30)
	{
		// 完成累加 30 張影像, 計算平均影像。
		for (j = 0; j < iVideoHeight; j++)
		{
			// 取得 imBackground 影像元件第 j 個 row 的儲存色彩値的記憶體起始位址。
			ptrImage =
				(BYTE*) Form1->imBackground->Picture->Bitmap->ScanLine[j];
			t = 0;
			for (i = 0; i < iVideoWidth; i++)
			{
				ucBackgroundYUV[j][i][0] =
					(unsigned char)(iTempYUV[j][i][0] / 30);
				ucBackgroundYUV[j][i][1] =
					(unsigned char)(iTempYUV[j][i][1] / 30);
				ucBackgroundYUV[j][i][2] =
					(unsigned char)(iTempYUV[j][i][2] / 30);
				// 將 YUV 色彩轉換成 RGB 色彩後, 存到 imBackgroung 中顯示出來。
				iY = ucBackgroundYUV[j][i][0];
				iU = ucBackgroundYUV[j][i][1];
				iV = ucBackgroundYUV[j][i][2];
				iC1 = iY - 16;
				iD = iU - 128;
				int iE = iV - 128;
				// 處理紅色之色彩轉換。
				iR = (298 * iC1 + 409 * iE + 128) >> 8;
				// 檢查 iR 是否發生溢位 ? 如果超過 255 表示發生溢位, 將其值設定為 255。
				if (iR > 255)
					iR = 255;
				else if (iR < 0)
					iR = 0;
				// 處理綠色之色彩轉換
				iG = (298 * iC1 - 100 * iD - 208 * iE + 128) >> 8;
				// 檢查 iG 是否發生溢位 ? 如果超過 255 表示發生溢位, 將其值設定為 255。
				if (iG > 255)
					iG = 255;
				else if (iG < 0)
					iG = 0;
				iB = (298 * iC1 + 516 * iD + 128) >> 8;
				// 檢查 iB 是否發生溢位 ? 如果超過 255 表示發生溢位, 將其值設定為 255。
				if (iB > 255)
					iB = 255;
				else if (iB < 0)
					iB = 0;
				// 存到 imBackgroung 中顯示出來。
				ptrImage[t + 2] = (unsigned char) iR;
				ptrImage[t + 1] = (unsigned char) iG;
				ptrImage[t] = (unsigned char) iB;
				t = t + 3;
			}
		}
		Form1->imBackground->Refresh();
		capSetCallbackOnFrame(hwndVideo, NULL);
	}
	else // ( iFrameCount < 30 )
	{
		k = 0;
		for (j = 0; j < iVideoHeight; j++)
			for (i = 0; i < iVideoWidth; i = i + 2)
			{
				int iY1 = ptrCapture[k];
				iU = ptrCapture[k + 1];
				int iY2 = ptrCapture[k + 2];
				iV = ptrCapture[k + 3];
				// 累加宏像素中的第一個像素 ( Pixel 1 )
				iTempYUV[j][i][0] = iTempYUV[j][i][0] + iY1;
				iTempYUV[j][i][1] = iTempYUV[j][i][1] + iU;
				iTempYUV[j][i][2] = iTempYUV[j][i][2] + iV;
				// 累加宏像素中的第二個像素 ( Pixel 2 )
				iTempYUV[j][i + 1][0] = iTempYUV[j][i + 1][0] + iY2;
				iTempYUV[j][i + 1][1] = iTempYUV[j][i + 1][1] + iU;
				iTempYUV[j][i + 1][2] = iTempYUV[j][i + 1][2] + iV;
				k = k + 4;
			}
	}
	Form1->lblFrameCounter->Caption = AnsiString(iFrameCounter);
	return 0;
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::miBackgroundClick(TObject *Sender)
{
	pcWebcam->TabIndex = 2;
	iFrameCounter = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight,
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	// 針對用來存放背景影像及累加影像的陣列設定初始値。
	for (j = 0; j < iVideoHeight; j++)
		for (i = 0; i < iVideoWidth; i++)
		{
			// RGB 色彩模式背景影像。
			ucBackground[j][i][0] = 0;
			ucBackground[j][i][1] = 0;
			ucBackground[j][i][2] = 0;
			iTemp[j][i][0] = 0;
			iTemp[j][i][1] = 0;
			iTemp[j][i][2] = 0;
			// YUV 色彩模式背景影像。
			ucBackgroundYUV[j][i][0] = 0;
			ucBackgroundYUV[j][i][1] = 0;
			ucBackgroundYUV[j][i][2] = 0;
			iTempYUV[j][i][0] = 0;
			iTempYUV[j][i][1] = 0;
			iTempYUV[j][i][2] = 0;
		}
	// 針對用來顯示背景影像的影像元件設定各項屬性 ( Property ) 値。
	imBackground->Width = iVideoWidth;
	imBackground->Height = iVideoHeight;
	imBackground->Picture->Bitmap->Width = iVideoWidth;
	imBackground->Picture->Bitmap->Height = iVideoHeight;
	imBackground->Picture->Bitmap->PixelFormat = pf24bit;
	// 開啟預覽回呼函式 ( 設定為 FrameCallbackBackgroundYUY2 ) 。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackBackgroundYUY2);

}

// ---------------------------------------------------------------------------
void __fastcall TForm1::miColorModeClick(TObject *Sender)
{
	iFrameCounter = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight,
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	// 開啟預覽回呼函式 ( 設定為 FrameCallbackDifferenceColorYUY2 ) 。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackDifferenceColorYUY2);

}

LRESULT CALLBACK FrameCallbackDifferenceColorYUY2(HWND hwndCapture,
	PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	// 設定 ptrCapture 指向視訊擷取視窗的緩衝區記憶體的起始位址。
	ptrCapture = lpvhdr->lpData;
	// 開始進行色彩差異之計算。
	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			// 每次處理 1 個 macropixel, 共 2 個 pixels, 4 bytes 。
			iY1 = ptrCapture[k];
			iU = ptrCapture[k + 1];
			iY2 = ptrCapture[k + 2];
			iV = ptrCapture[k + 3];
			// 計算差異値。
			iY1Difference = abs(ptrCapture[k] - ucBackgroundYUV[j][i][0]);
			iUDifference = ucBackgroundYUV[j][i][1] - ptrCapture[k + 1];
			iY2Difference =
				abs(ptrCapture[k + 2] - ucBackgroundYUV[j][i + 1][0]);
			iVDifference = ucBackgroundYUV[j][i + 1][2] - ptrCapture[k + 3];
			// 將差異値以彩色模式顯示在視訊擷取視窗。
			ptrCapture[k] = iY1Difference + 16;
			ptrCapture[k + 1] = iUDifference + 128;
			ptrCapture[k + 2] = iY2Difference + 16;
			ptrCapture[k + 3] = iVDifference + 128;
			// 更新 k 値, 準備處理下一個宏像素( macropixel )
			k = k + 4;
		}
	}
	Form1->lblFrameCounter->Caption = AnsiString(iFrameCounter);
	return 0;
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::miBinaryModeClick(TObject *Sender)
{
	iFrameCounter = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight，
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	// 開啟預覽回呼函式 ( 設定為 FrameCallbackDifferenceBinaryYUY2 )
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackDifferenceBinaryYUY2);

}

LRESULT CALLBACK FrameCallbackDifferenceBinaryYUY2(HWND hwndCapture,
	PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	// 設定 ptrCapture 指向是訊擷取視窗的緩衝區記憶體的起始位址。
	ptrCapture = lpvhdr->lpData;

	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			// 每次處理 1 個 macropixel, 共 2 個 pixels, 4 bytes。
			iY1 = ptrCapture[k];
			iU = ptrCapture[k + 1];
			iY2 = ptrCapture[k + 2];
			iV = ptrCapture[k + 3];
			// 計算差異値 。
			iY1Difference = ptrCapture[k] - ucBackgroundYUV[j][i][0];
			iUDifference = ptrCapture[k + 1] - ucBackgroundYUV[j][i][1];
			iY2Difference = ptrCapture[k + 2] - ucBackgroundYUV[j][i + 1][0];
			iVDifference = ptrCapture[k + 3] - ucBackgroundYUV[j][i + 1][2];
			// 分別計算 2 個pixel 差異値的平方和。
			iSquareDifferenceSum1 = iY1Difference * iY1Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			iSquareDifferenceSum2 = iY2Difference * iY2Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			// 不管是黑或白, U, V 都是 128。
			ptrCapture[k + 1] = 128;
			ptrCapture[k + 3] = 128;
			// 設定 pixel 1 的 Y, 即 Y1
			if (iSquareDifferenceSum1 > iThreshold)
				ptrCapture[k] = 255;
			else
				ptrCapture[k] = 16;
			// 設定 pixel 2 的 Y, 即 Y2
			if (iSquareDifferenceSum2 > iThreshold)
				ptrCapture[k + 2] = 255;
			else
				ptrCapture[k + 2] = 16;
			// 更新 k 値, 準備處理下一個 macropixel。
			k = k + 4;
		}
	}
	Form1->lblFrameCounter->Caption = AnsiString(iFrameCounter);
	return 0;
}

// ---------------------------------------------------------------------------

void __fastcall TForm1::edThresholdChange(TObject *Sender)
{
	iThreshold = StrToIntDef(edThreshold->Text, 1600);
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miBlockModeClick(TObject *Sender)
{
	iFrameCounter = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight，
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	// 開啟預覽回呼函式 ( 設定為 FrameCallbackDifferenceBlockBinaryYUY2 )
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackDifferenceBlockYUY2);

}

LRESULT CALLBACK FrameCallbackDifferenceBlockYUY2(HWND hwndCapture,
	PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	ptrCapture = lpvhdr->lpData;
	// 重設區塊累加陣列。
	for (int m = 0; m < 60; m++)
		for (int n = 0; n < 80; n++)
			iBlockChangeCounter[m][n] = 0;

	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			// 計算目前的 j, i 這個像素是屬於哪一個 block 的累加範圍。
			int m = j / 8;
			int n = i / 8;
			// 每次處理 1 個 macropixel, 共 2 個 pixels, 4 bytes。
			iY1 = ptrCapture[k];
			iU = ptrCapture[k + 1];
			iY2 = ptrCapture[k + 2];
			iV = ptrCapture[k + 3];
			// 計算差異値
			iY1Difference = abs(ptrCapture[k] - ucBackgroundYUV[j][i][0]);
			iUDifference = abs(ptrCapture[k + 1] - ucBackgroundYUV[j][i][1]);
			iY2Difference =
				abs(ptrCapture[k + 2] - ucBackgroundYUV[j][i + 1][0]);
			iUDifference =
				abs(ptrCapture[k + 3] - ucBackgroundYUV[j][i + 1][2]);
			// 分別計算 2 個pixel 差異値的平方和
			iSquareDifferenceSum1 = iY1Difference * iY1Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			iSquareDifferenceSum2 = iY2Difference * iY2Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			// 累加每個區塊中的差異像素
			if (iSquareDifferenceSum1 > iThreshold)
				iBlockChangeCounter[m][n]++;
			if (iSquareDifferenceSum2 > iThreshold)
				iBlockChangeCounter[m][n]++;
			// 更新 k 値, 準備處理下一個 macropixel
			k = k + 4;
		}
	}
	// 顯示黑白區塊。
	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			// 計算目前的 j, i 這個像素是屬於哪一個 block 的範圍。
			int m = j / 8;
			int n = i / 8;
			// 不管是黑或白, U, V 都是 128
			ptrCapture[k + 1] = 128;
			ptrCapture[k + 3] = 128;
			// Block 中白點的個數超過一半就整個區塊塗成白色。
			if (iBlockChangeCounter[m][n] > 31)
			{
				ptrCapture[k] = 255;
				ptrCapture[k + 2] = 255;
			}
			else
			{
				ptrCapture[k] = 16;
				ptrCapture[k + 2] = 16;
			}
			k = k + 4;
		}
	}

	Form1->lblFrameCounter->Caption = AnsiString(iFrameCounter);
	return 0;
}

// ---------------------------------------------------------------------------
void __fastcall TForm1::miPlayClick(TObject *Sender)
{
	iFrameCounter = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight,
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	iVideoMacroWidth = iVideoWidth / 2;
	iVideoBlockWidth = iVideoWidth / 8;
	iVideoBlockHeight = iVideoHeight / 8;

	// 重設亂數產生器
	float iTime = clock();
	srand(iTime);

	// 設定虛擬斑點壁虎的相關資料
	voGecko.iX1 = 50;
	voGecko.iY1 = 50;
	voGecko.iWidth = 155;
	voGecko.iHeight = 125;
	voGecko.iX2 = voGecko.iX1 + voGecko.iWidth - 1;
	voGecko.iY2 = voGecko.iY1 + voGecko.iHeight - 1;
	voGecko.iBlockX1 = voGecko.iX1 / 8;
	voGecko.iBlockY1 = voGecko.iY1 / 8;
	voGecko.iBlockX2 = voGecko.iX2 / 8;
	voGecko.iBlockY2 = voGecko.iY2 / 8;
	voGecko.bVisible = true;
	voGecko.ucBackgroundRed = 255;
	voGecko.ucBackgroundGreen = 255;
	voGecko.ucBackgroundBlue = 255;

	// 將影像元件中的色彩資料放到陣列中
	for (j = 0; j < voGecko.iHeight; j++)
	{
		ptrImage = (BYTE*)imGecko->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < voGecko.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucGeckoRGB[j][i][0] = (unsigned char) iB;
			ucGeckoRGB[j][i][1] = (unsigned char) iG;
			ucGeckoRGB[j][i][2] = (unsigned char) iR;
			ucGeckoYUV[j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucGeckoYUV[j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucGeckoYUV[j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
	}

	// 開啟預覽回呼函式 FrameCallbackPlayWithGecko。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackPlayWithGecko6A);

}
// ---------------------------------------------------------------------------

void __fastcall TForm1::miReplayClick(TObject *Sender)
{
	// 注意: 要重新讓虛擬物件出現, 只要將 voGecko.bVisible 重新設回 true 即可。
	// 如果重新出現的虛擬物件要更改位置, 只要將虛擬物件的位置相關屬性再作變更即可!
	voGecko.iX1 = rand() % 485;
	voGecko.iY1 = rand() % 355;
	voGecko.iX2 = voGecko.iX1 + voGecko.iWidth - 1;
	voGecko.iY2 = voGecko.iY1 + voGecko.iHeight - 1;
	voGecko.iBlockX1 = voGecko.iX1 / 8;
	voGecko.iBlockY1 = voGecko.iY1 / 8;
	voGecko.iBlockX2 = voGecko.iX2 / 8;
	voGecko.iBlockY2 = voGecko.iY2 / 8;
	voGecko.bVisible = true;

}

LRESULT CALLBACK FrameCallbackPlayWithGecko6A(HWND hwndCapture,
	PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	ptrCapture = lpvhdr->lpData;

	// 重設儲存區塊中, 差異像素總數的累加陣列
	for (m = 0; m < iVideoBlockHeight; m++)
		for (n = 0; n < iVideoBlockWidth; n++)
			iBlockChangeCounter[m][n] = 0;

	// 迴圈中, 同時處理實境物件偵測 ( 差異偵測 ) 與置入虛擬物件到視訊擷取視窗
	// 先進行實境物件偵測, 再進行置入虛擬物件到視訊擷取視窗
	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			m = j / 8;
			n = i / 8;
			iY1 = ptrCapture[k];
			iU = ptrCapture[k + 1];
			iY2 = ptrCapture[k + 2];
			iV = ptrCapture[k + 3];
			iY1Difference = abs(ptrCapture[k] - ucBackgroundYUV[j][i][0]);
			iUDifference = abs(ptrCapture[k + 1] - ucBackgroundYUV[j][i][1]);
			iY2Difference =
				abs(ptrCapture[k + 2] - ucBackgroundYUV[j][i + 1][0]);
			iVDifference =
				abs(ptrCapture[k + 3] - ucBackgroundYUV[j][i + 1][2]);

			iSquareDifferenceSum1 = iY1Difference * iY1Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			iSquareDifferenceSum2 = iY2Difference * iY2Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			// 累加每個區塊中的差異像素
			if (iSquareDifferenceSum1 > iThreshold)
				iBlockChangeCounter[m][n]++;
			if (iSquareDifferenceSum2 > iThreshold)
				iBlockChangeCounter[m][n]++;

			// 實境物件偵測完畢, 接下來, 開始置入虛擬物件到視訊擷取視窗
			// 如果 bVisible 值為 true
			// 在座標 ( iX1, iY1 ) 與 ( iX2, iY2 ) 之間的區域要置入虛擬物件
			if ((voGecko.bVisible) && (i >= voGecko.iX1) && (i < voGecko.iX2) &&
				(j >= voGecko.iY1) && (j < voGecko.iY2))
			{
				// 虛擬物件的背景區域必須忽略, 無須置入
				if ((ucGeckoRGB[j - voGecko.iY1][i - voGecko.iX1][0]
					!= voGecko.ucBackgroundBlue) ||
					(ucGeckoRGB[j - voGecko.iY1][i - voGecko.iX1][1]
					!= voGecko.ucBackgroundGreen) ||
					(ucGeckoRGB[j - voGecko.iY1][i - voGecko.iX1][2]
					!= voGecko.ucBackgroundRed))
				{
					ptrCapture[k] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1][0];
					ptrCapture[k + 1] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1][1];
					ptrCapture[k + 2] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1 + 1][0];
					ptrCapture[k + 3] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1][2];
				}
			}
			k = k + 4;
		}
	}

	// 實境物件的位置為 iBlockChangeCounter[m][n] > 31 的區塊,
	// 虛擬物件的位置為( iObjectBlockX1, iObjectBlockY1 ) 與 ( iObjectBlockX2, iObjectBlockY2 ) 之間,
	// 檢查實境物件與虛擬物件的位置是否重疊 ?

	k = 0;
	for (m = 0; m < iVideoBlockHeight; m++)
	{
		for (n = 0; n < iVideoBlockWidth; n++)
		{
			// 第 1 個判斷式 ( iBlockChangeCounter[m][n]>31 ) 判斷實境物件的位置,
			// 其餘 4 個判斷式是判斷虛擬物件的位置,
			// 同時成立表示兩個物件在這個 [m, n] 區塊有交集,
			if ((voGecko.bVisible) && (iBlockChangeCounter[m][n] > 31) && (m >
				voGecko.iBlockY1) && (n > voGecko.iBlockX1) && (m <
				voGecko.iBlockY2) && (n < voGecko.iBlockX2))
			{
				// 啟動互動處理程序。
				voGecko.bVisible = false;
				// 已經確定重疊了, 不需檢查其他區塊了! 故可以跳離 for 迴圈了。
				m = iVideoBlockHeight;
				n = iVideoBlockWidth;
			}
		}
	}
	// 離開預覽回呼函式
	return 0;
}

// ---------------------------------------------------------------------------

LRESULT CALLBACK FrameCallbackPlayWithGecko6B(HWND hwndCapture,
	PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	ptrCapture = lpvhdr->lpData;

	// 重設區塊累加陣列
	for (m = 0; m < iVideoBlockHeight; m++)
		for (n = 0; n < iVideoBlockWidth; n++)
			iBlockChangeCounter[m][n] = 0;

	// 迴圈中, 同時處理實境物件偵測 ( 差異偵測 ) 與置入虛擬物件到視訊擷取視窗
	// 先進行實境物件偵測, 再進行置入虛擬物件到視訊擷取視窗
	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			m = j / 8;
			n = i / 8;
			iY1 = ptrCapture[k];
			iU = ptrCapture[k + 1];
			iY2 = ptrCapture[k + 2];
			iV = ptrCapture[k + 3];
			iY1Difference = abs(ptrCapture[k] - ucBackgroundYUV[j][i][0]);
			iUDifference = abs(ptrCapture[k + 1] - ucBackgroundYUV[j][i][1]);
			iY2Difference =
				abs(ptrCapture[k + 2] - ucBackgroundYUV[j][i + 1][0]);
			iVDifference =
				abs(ptrCapture[k + 3] - ucBackgroundYUV[j][i + 1][2]);

			iSquareDifferenceSum1 = iY1Difference * iY1Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			iSquareDifferenceSum2 = iY2Difference * iY2Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			// 累加每個區塊中的差異像素
			if (iSquareDifferenceSum1 > iThreshold)
				iBlockChangeCounter[m][n]++;
			if (iSquareDifferenceSum2 > iThreshold)
				iBlockChangeCounter[m][n]++;

			k = k + 4;
		}
	}

	// 實境物件偵測完畢, 接下來, 開始置入虛擬物件到視訊擷取視窗
	// 當使用者勾選 8 * 8 黑白區塊顯示時, 則將視訊擷取視窗內容依照差異偵測結果顯示
	// 白色 YUV 色彩值的 YUV = ( 255, 128, 128 )
	// 黑色 YUV 色彩值的 YUV = ( 16, 128, 128 )

	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			m = j / 8;
			n = i / 8;
			if (Form1->cbBlockDisplay->Checked)
			{
				ptrCapture[k + 1] = 128;
				ptrCapture[k + 3] = 128;
				if (iBlockChangeCounter[m][n] > 31)
				{
					ptrCapture[k] = 255;
					ptrCapture[k + 2] = 255;
				}
				else
				{
					ptrCapture[k] = 16;
					ptrCapture[k + 2] = 16;
				}
			}

			// 如果 bVisible 值為 true,
			// 在座標 ( iX1, iY1 ) 與 ( iX2, iY2 ) 之間的區域要置入虛擬物件。
			if ((voGecko.bVisible) && (i >= voGecko.iX1) && (i < voGecko.iX2) &&
				(j >= voGecko.iY1) && (j < voGecko.iY2))
			{
				// 虛擬物件的背景區域必須忽略, 無須置入。
				if ((ucGeckoRGB[j - voGecko.iY1][i - voGecko.iX1][0]
					!= voGecko.ucBackgroundBlue) ||
					(ucGeckoRGB[j - voGecko.iY1][i - voGecko.iX1][1]
					!= voGecko.ucBackgroundGreen) ||
					(ucGeckoRGB[j - voGecko.iY1][i - voGecko.iX1][2]
					!= voGecko.ucBackgroundRed))
				{
					ptrCapture[k] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1][0];
					ptrCapture[k + 1] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1][1];
					ptrCapture[k + 2] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1 + 1][0];
					ptrCapture[k + 3] =
						ucGeckoYUV[j - voGecko.iY1][i - voGecko.iX1][2];
				}
			}
			k = k + 4;
		}
	}

	// 實境物件的位置為 iBlockChangeCounter[m][n] > 31 的區塊,
	// 虛擬物件的位置為( iObjectBlockX1, iObjectBlockY1 ) 與 ( iObjectBlockX2, iObjectBlockY2 ) 之間,
	// 檢查實境物件與虛擬物件的位置是否重疊 ?

	k = 0;
	for (m = 0; m < iVideoBlockHeight; m++)
	{
		for (n = 0; n < iVideoBlockWidth; n++)
		{
			// 第 1 個判斷式 ( iBlockChangeCounter[m][n]>31 ) 判斷實境物件的位置,
			// 其餘 4 個判斷式是判斷虛擬物件的位置,
			// 同時成立表示兩個物件在這個 [m, n] 區塊有交集 !
			if ((voGecko.bVisible) && (iBlockChangeCounter[m][n] > 31) && (m >
				voGecko.iBlockY1) && (n > voGecko.iBlockX1) && (m <
				voGecko.iBlockY2) && (n < voGecko.iBlockX2))
			{
				// 啟動互動處理程序。
				voGecko.bVisible = false;
				// 已經確定重疊了, 不需檢查其他區塊了! 故可以跳離 for 迴圈了
				m = iVideoBlockHeight;
				n = iVideoBlockWidth;
			}
		}
	}
	// 離開預覽回呼函式
	return 0;
}

LRESULT CALLBACK FrameCallbackF(HWND hwndVideo, PVIDEOHDR lpvhdr)
{
	iFrameCounter++;
	// 取得視訊擷取視窗的緩衝區記憶體的起始位址。
	ptrCapture = lpvhdr->lpData;

	// 重設儲存區塊中, 差異像素總數的累加陣列
	for (m = 0; m < iVideoBlockHeight; m++)
		for (n = 0; n < iVideoBlockWidth; n++)
			iBlockChangeCounter[m][n] = 0;
	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			m = j / 8;
			n = i / 8;
			iY1 = ptrCapture[k];
			iU = ptrCapture[k + 1];
			iY2 = ptrCapture[k + 2];
			iV = ptrCapture[k + 3];
			iY1Difference = abs(ptrCapture[k] - ucBackgroundYUV[j][i][0]);
			iUDifference = abs(ptrCapture[k + 1] - ucBackgroundYUV[j][i][1]);
			iY2Difference =
				abs(ptrCapture[k + 2] - ucBackgroundYUV[j][i + 1][0]);
			iVDifference =
				abs(ptrCapture[k + 3] - ucBackgroundYUV[j][i + 1][2]);

			iSquareDifferenceSum1 = iY1Difference * iY1Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			iSquareDifferenceSum2 = iY2Difference * iY2Difference +
				iUDifference * iUDifference + iVDifference * iVDifference;
			// 累加每個區塊中的差異像素
			if (iSquareDifferenceSum1 > iThreshold)
				iBlockChangeCounter[m][n]++;
			if (iSquareDifferenceSum2 > iThreshold)
				iBlockChangeCounter[m][n]++;

			// 實境物件偵測完畢, 接下來, 開始置入虛擬物件到視訊擷取視窗
			// 如果 bVisible 值為 true
			// 在座標 ( iX1, iY1 ) 與 ( iX2, iY2 ) 之間的區域要置入虛擬物件
			if ((vCamera.bVisible) && (i >= vCamera.iX1) && (i < vCamera.iX2) &&
				(j >= vCamera.iY1) && (j < vCamera.iY2))
			{
				// 虛擬物件的背景區域必須忽略, 無須置入
				if ((ucCameraRGB[j - vCamera.iY1][i - vCamera.iX1][0]
					!= vCamera.ucBackgroundBlue) ||
					(ucCameraRGB[j - vCamera.iY1][i - vCamera.iX1][1]
					!= vCamera.ucBackgroundGreen) ||
					(ucCameraRGB[j - vCamera.iY1][i - vCamera.iX1][2]
					!= vCamera.ucBackgroundRed))
				{
					ptrCapture[k] =
						ucCameraYUV[j - vCamera.iY1][i - vCamera.iX1][0];
					ptrCapture[k + 1] =
						ucCameraYUV[j - vCamera.iY1][i - vCamera.iX1][1];
					ptrCapture[k + 2] =
						ucCameraYUV[j - vCamera.iY1][i - vCamera.iX1 + 1][0];
					ptrCapture[k + 3] =
						ucCameraYUV[j - vCamera.iY1][i - vCamera.iX1][2];
				}
			}
			k = k + 4;
		}
	}

	k = 0;
	for (m = 0; m < iVideoBlockHeight; m++)
	{
		for (n = 0; n < iVideoBlockWidth; n++)
		{
			// 第 1 個判斷式 ( iBlockChangeCounter[m][n]>31 ) 判斷實境物件的位置,
			// 其餘 4 個判斷式是判斷虛擬物件的位置,
			// 同時成立表示兩個物件在這個 [m, n] 區塊有交集,
			if ((vCamera.bVisible) && (iBlockChangeCounter[m][n] > 31) && (m >
				vCamera.iBlockY1) && (n > vCamera.iBlockX1) && (m <
				vCamera.iBlockY2) && (n < vCamera.iBlockX2))
			{
				// 啟動互動處理程序。
				// AnsiString a = "截圖_"+IntToStr(frameName++)+".bmp";
				Graphics::TBitmap *scrBitmap = new Graphics::TBitmap();

				scrBitmap->Width = Form1->pnlCaptureWindow->Width-10;

				scrBitmap->Height = Form1->pnlCaptureWindow->Height-10;

				HDC hdc = GetDC(hwndVideo);

				BitBlt(scrBitmap->Canvas->Handle, 0, 0, scrBitmap->Width,
					scrBitmap->Height, hdc, 0, 0, SRCCOPY);

				scrBitmap->SaveToFile(".\\截圖(含背景).bmp");

				delete scrBitmap;

				capFileSaveDIB(hwndVideo, _T("截圖(原始).bmp"));
				Form1->Memo1->Lines->Add("截圖成功!");
				vCamera.bVisible = false;

				// 已經確定重疊了, 不需檢查其他區塊了! 故可以跳離 for 迴圈了。
				m = iVideoBlockHeight;
				n = iVideoBlockWidth;
			}
		}
	}
	k = 0;
	for (j = 0; j < iVideoHeight; j++)
	{
		for (i = 0; i < iVideoMacroWidth / 2; i++)
		{
			// 第 i 個 macropixel 的交換對象是第 ( iVideoWidth / 2 ) - i 。
			iPartner = iVideoMacroWidth - i - 1;
			// 先將第 i 個 marcopixel 的四個位元組的色彩值暫存起來。
			ucTempY1 = ptrCapture[j * iRowBufferSize + i * 4];
			ucTempU = ptrCapture[j * iRowBufferSize + i * 4 + 1];
			ucTempY2 = ptrCapture[j * iRowBufferSize + i * 4 + 2];
			ucTempV = ptrCapture[j * iRowBufferSize + i * 4 + 3];
			// 把 Partner 的四個位元組的色彩值複製過來, 但 Y1 與 Y2 要互換位置。
			ptrCapture[j * iRowBufferSize + i * 4] =
				ptrCapture[j * iRowBufferSize + iPartner * 4 + 2];
			ptrCapture[j * iRowBufferSize + i * 4 + 1] =
				ptrCapture[j * iRowBufferSize + iPartner * 4 + 1];
			ptrCapture[j * iRowBufferSize + i * 4 + 2] =
				ptrCapture[j * iRowBufferSize + iPartner * 4];
			ptrCapture[j * iRowBufferSize + i * 4 + 3] =
				ptrCapture[j * iRowBufferSize + iPartner * 4 + 3];
			// 將暫存的色彩值放到 Partner 的緩衝區記憶體之中, 但 Y1 與 Y2 要互換位置。
			ptrCapture[j * iRowBufferSize + iPartner * 4] = ucTempY2;
			ptrCapture[j * iRowBufferSize + iPartner * 4 + 1] = ucTempU;
			ptrCapture[j * iRowBufferSize + iPartner * 4 + 2] = ucTempY1;
			ptrCapture[j * iRowBufferSize + iPartner * 4 + 3] = ucTempV;
		}
		for (i = 0; i < iVideoWidth; i = i + 2)
		{
			if ((i >= vFrame1.iX1) && (i < vFrame1.iX2) && (j >= vFrame1.iY1) &&
				(j < vFrame1.iY2))
			{

				if ((ptrCapture[k] >= iMinY) && (ptrCapture[k] <= iMaxY) &&
					(ptrCapture[k + 1] >= iMinU) && (ptrCapture[k + 1] <= iMaxU)
					&& (ptrCapture[k + 2] >= iMinY) && (ptrCapture[k + 2] <=
					iMaxY) && (ptrCapture[k + 3] >= iMinV) &&
					(ptrCapture[k + 3] <= iMaxV))
				{
					ptrCapture[k] =
						ucFrame1YUV[frameID][j - vFrame1.iY1]
						[i - vFrame1.iX1][0];
					ptrCapture[k + 1] =
						ucFrame1YUV[frameID][j - vFrame1.iY1]
						[i - vFrame1.iX1][1];
					ptrCapture[k + 2] =
						ucFrame1YUV[frameID][j - vFrame1.iY1]
						[i - vFrame1.iX1 + 1][0];
					ptrCapture[k + 3] =
						ucFrame1YUV[frameID][j - vFrame1.iY1]
						[i - vFrame1.iX1][2];
				}
			}
			k = k + 4;
		}
	}

	// 結束預覽回呼函數之執行。
	return (0);
}

void __fastcall TForm1::akePicture1Click(TObject *Sender)
{
	iFrameCounter = 0;
	//frameID = 0;
	// 因為 FrameCallBackGrayMode 需要用到 iVideoWidth, iVideoHeight,
	// 所以必須事先使用 capGetVideoFormat 來取得視訊內容的高與寬。
	iVideoFormatSize = capGetVideoFormatSize(hwndVideo);
	capGetVideoFormat(hwndVideo, &bmiBitmapInfo, iVideoFormatSize);
	iVideoWidth = bmiBitmapInfo.bmiHeader.biWidth;
	iVideoHeight = bmiBitmapInfo.bmiHeader.biHeight;
	iRowBufferSize = iVideoMacroWidth * 4;
	iVideoMacroWidth = iVideoWidth / 2;
	iVideoBlockWidth = iVideoWidth / 8;
	iVideoBlockHeight = iVideoHeight / 8;

	// 設定虛擬背景的相關資料
	vFrame1.iX1 = 0;
	vFrame1.iY1 = 0;
	vFrame1.iWidth = 640;
	vFrame1.iHeight = 480;
	vFrame1.ucBackgroundRed = 255;
	vFrame1.ucBackgroundGreen = 255;
	vFrame1.ucBackgroundBlue = 255;
	vFrame1.iX2 = vFrame1.iX1 + vFrame1.iWidth - 1;
	vFrame1.iY2 = vFrame1.iY1 + vFrame1.iHeight - 1;

	vFrame1.iBlockX1 = vFrame1.iX1 / 8;
	vFrame1.iBlockY1 = vFrame1.iY1 / 8;
	vFrame1.iBlockX2 = vFrame1.iX2 / 8;
	vFrame1.iBlockY2 = vFrame1.iY2 / 8;
	vFrame1.bVisible = true;

	// 設定虛擬拍照的相關資料
	vCamera.iX1 = 500;
	vCamera.iY1 = 350;
	vCamera.iWidth = 125;
	vCamera.iHeight = 125;
	vCamera.ucBackgroundRed = 255;
	vCamera.ucBackgroundGreen = 255;
	vCamera.ucBackgroundBlue = 255;
	vCamera.iX2 = vCamera.iX1 + vCamera.iWidth - 1;
	vCamera.iY2 = vCamera.iY1 + vCamera.iHeight - 1;

	vCamera.iBlockX1 = vCamera.iX1 / 8;
	vCamera.iBlockY1 = vCamera.iY1 / 8;
	vCamera.iBlockX2 = vCamera.iX2 / 8;
	vCamera.iBlockY2 = vCamera.iY2 / 8;
	vCamera.bVisible = true;

	// 將影像元件中的色彩資料放到陣列中
	for (j = 0; j < vFrame1.iHeight; j++)
	{
		ptrImage = (BYTE*)Frame1->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < vFrame1.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucFrame1RGB[0][j][i][0] = (unsigned char) iB;
			ucFrame1RGB[0][j][i][1] = (unsigned char) iG;
			ucFrame1RGB[0][j][i][2] = (unsigned char) iR;
			ucFrame1YUV[0][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucFrame1YUV[0][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucFrame1YUV[0][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}

		ptrImage = (BYTE*)Frame2->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < vFrame1.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucFrame1RGB[1][j][i][0] = (unsigned char) iB;
			ucFrame1RGB[1][j][i][1] = (unsigned char) iG;
			ucFrame1RGB[1][j][i][2] = (unsigned char) iR;
			ucFrame1YUV[1][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucFrame1YUV[1][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucFrame1YUV[1][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}

		ptrImage = (BYTE*)Frame3->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < vFrame1.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucFrame1RGB[2][j][i][0] = (unsigned char) iB;
			ucFrame1RGB[2][j][i][1] = (unsigned char) iG;
			ucFrame1RGB[2][j][i][2] = (unsigned char) iR;
			ucFrame1YUV[2][j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucFrame1YUV[2][j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucFrame1YUV[2][j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}

	}

	// 將影像元件中的色彩資料放到陣列中
	for (j = 0; j < vCamera.iHeight; j++)
	{
		ptrImage = (BYTE*)imCamera->Picture->Bitmap->ScanLine[j];
		for (i = 0; i < vCamera.iWidth; i++)
		{
			iB = ptrImage[i * 3];
			iG = ptrImage[i * 3 + 1];
			iR = ptrImage[i * 3 + 2];
			ucCameraRGB[j][i][0] = (unsigned char) iB;
			ucCameraRGB[j][i][1] = (unsigned char) iG;
			ucCameraRGB[j][i][2] = (unsigned char) iR;
			ucCameraYUV[j][i][0] =
				(unsigned char)
				(((66 * iR + 129 * iG + 25 * iB + 128) >> 8) + 16);
			ucCameraYUV[j][i][1] =
				(unsigned char)
				(((-38 * iR - 74 * iG + 112 * iB + 128) >> 8) + 128);
			ucCameraYUV[j][i][2] =
				(unsigned char)
				(((112 * iR - 94 * iG - 18 * iB + 128) >> 8) + 128);
		}
	}

	// 開啟預覽回呼函式 FrameCallbackPlayWithGecko。
	capSetCallbackOnFrame(hwndVideo, &FrameCallbackF);
}

// ---------------------------------------------------------------------------
void __fastcall TForm1::Button1Click(TObject *Sender)
{
	frameID = 0;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::Button2Click(TObject *Sender)
{
	frameID = 1;
}
// ---------------------------------------------------------------------------

void __fastcall TForm1::Button3Click(TObject *Sender)
{
	frameID = 2;
}
// ---------------------------------------------------------------------------

