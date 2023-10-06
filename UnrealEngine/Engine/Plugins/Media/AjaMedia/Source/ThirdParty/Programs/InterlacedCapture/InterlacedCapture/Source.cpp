#define MSWindows 1
#define AJA_WINDOWS 1
#define AJA_NO_AUTOIMPORT 1

const int kAppSignature = 12999;

#include "ajantv2/includes/ntv2card.h"
#include "ajantv2/includes/ntv2enums.h"
#include "ajabase/common/types.h"
#include "ajabase/system/process.h"
#include "ajabase/common/testpatterngen.h"
#include "ajabase/common/options_popt.h"
#include "ajantv2/includes/ntv2utils.h"
#include "ajantv2/includes/ntv2rp188.h"
#include "ajabase/common/timecodeburn.h"

//#include "Bitmap.h"

#include <iostream>
#include <chrono>
#include <type_traits>

namespace std
{
	template <typename _CharT, typename _Traits>
	inline basic_ostream<_CharT, _Traits>& tab(basic_ostream<_CharT, _Traits> &__os)
	{
		return __os.put(__os.widen('\t'));
	}
}

const unsigned int	CIRCULAR_BUFFER_SIZE(10);		///< @brief	Specifies how many AVDataBuffers constitute the circular buffer

AJA_PixelFormat GetAJAPixelFormat(const NTV2FrameBufferFormat inFormat)
{
	switch (inFormat)
	{
	case NTV2_FBF_10BIT_YCBCR:				return AJA_PixelFormat_YCbCr10;
	case NTV2_FBF_8BIT_YCBCR:				return AJA_PixelFormat_YCbCr8;
	case NTV2_FBF_ARGB:						return AJA_PixelFormat_ARGB8;
	case NTV2_FBF_RGBA:						return AJA_PixelFormat_RGBA8;
	case NTV2_FBF_10BIT_RGB:				return AJA_PixelFormat_RGB10;
	case NTV2_FBF_8BIT_YCBCR_YUY2:			return AJA_PixelFormat_YUY28;
	case NTV2_FBF_ABGR:						return AJA_PixelFormat_ABGR8;
	case NTV2_FBF_10BIT_DPX:				return AJA_PixelFormat_RGB_DPX;
	case NTV2_FBF_10BIT_YCBCR_DPX:			return AJA_PixelFormat_YCbCr_DPX;
	case NTV2_FBF_8BIT_DVCPRO:				return AJA_PixelFormat_DVCPRO;
	case NTV2_FBF_8BIT_HDV:					return AJA_PixelFormat_HDV;
	case NTV2_FBF_24BIT_RGB:				return AJA_PixelFormat_RGB8_PACK;
	case NTV2_FBF_24BIT_BGR:				return AJA_PixelFormat_BGR8_PACK;
	case NTV2_FBF_10BIT_YCBCRA:				return AJA_PixelFormat_YCbCrA10;
	case NTV2_FBF_10BIT_DPX_LE:             return AJA_PixelFormat_RGB_DPX_LE;
	case NTV2_FBF_48BIT_RGB:				return AJA_PixelFormat_RGB16;
	//case NTV2_FBF_PRORES:					return AJA_PixelFormat_PRORES;
	case NTV2_FBF_PRORES_DVCPRO:			return AJA_PixelFormat_PRORES_DVPRO;
	case NTV2_FBF_PRORES_HDV:				return AJA_PixelFormat_PRORES_HDV;
	case NTV2_FBF_10BIT_RGB_PACKED:			return AJA_PixelFormat_RGB10_PACK;

	case NTV2_FBF_8BIT_YCBCR_420PL2:		return AJA_PixelFormat_YCBCR8_420PL;
	case NTV2_FBF_8BIT_YCBCR_422PL2:		return AJA_PixelFormat_YCBCR8_422PL;
	case NTV2_FBF_10BIT_YCBCR_420PL2:		return AJA_PixelFormat_YCBCR10_420PL;
	case NTV2_FBF_10BIT_YCBCR_422PL2:		return AJA_PixelFormat_YCBCR10_422PL;

	case NTV2_FBF_8BIT_YCBCR_420PL3:		return AJA_PixelFormat_YCBCR8_420PL3;
	case NTV2_FBF_8BIT_YCBCR_422PL3:		return AJA_PixelFormat_YCBCR8_422PL3;
	case NTV2_FBF_10BIT_YCBCR_420PL3_LE:	return AJA_PixelFormat_YCBCR10_420PL3LE;
	case NTV2_FBF_10BIT_YCBCR_422PL3_LE:	return AJA_PixelFormat_YCBCR10_422PL3LE;

	case NTV2_FBF_10BIT_RAW_RGB:
	case NTV2_FBF_10BIT_RAW_YCBCR:
	case NTV2_FBF_10BIT_ARGB:
	case NTV2_FBF_16BIT_ARGB:
	case NTV2_FBF_INVALID:					break;
	}
	return AJA_PixelFormat_Unknown;
}	//	GetAJAPixelFormat

struct FCapture
{
	CNTV2Card* mDevice;
	int mDeviceIndex;
	NTV2Channel mChannel;
	std::string mFolderPath;
	NTV2VideoFormat mVideoFormat;
	NTV2FrameBufferFormat mFrameBufferFormat;
	NTV2EveryFrameTaskMode mSavedTaskMode;
	bool mbInputFormatIsRGB;

	FCapture()
	{
		mDevice = nullptr;
	}

	bool Initialize()
	{
		if (!AskQuestions())
		{
			return false;
		}

		//	Open the board...
		mDevice = new CNTV2Card(mDeviceIndex);

		if (!mDevice->IsDeviceReady(false))
		{
			std::cerr << "## ERROR:  Device '" << mDeviceIndex << "' not ready" << std::endl;
			return false;
		}

		if (!mDevice->AcquireStreamForApplication(kAppSignature, static_cast <uint32_t>(AJAProcess::GetPid())))
		{
			std::cerr << "## ERROR:  Unable to acquire device because another app owns it" << std::endl;
			return false;		//	Some other app is using the device
		}

		mDevice->GetEveryFrameServices(mSavedTaskMode);		//	Save the current state before changing it
		mDevice->SetEveryFrameServices(NTV2_OEM_TASKS);		//	Since this is an OEM demo, use the OEM service level

		//	This is a "playback" application, so set the board reference to free run...
		if (!mDevice->SetReference(NTV2_REFERENCE_FREERUN))
			return false;

		mDevice->ClearRouting();

		//	Enable the Frame Buffer, just in case it's not currently enabled...
		mDevice->EnableChannel(mChannel);

		mDevice->EnableInputInterrupt(mChannel);
		mDevice->SubscribeInputVerticalEvent(mChannel);
		mDevice->SubscribeOutputVerticalEvent(NTV2_CHANNEL1);

		//	Set the video format for the channel's Frame Store to 8-bit YCbCr...
		if (!mDevice->SetVideoFormat(mVideoFormat, mChannel))
			return false;

		//	Set the video format for the channel's Frame Store to 8-bit YCbCr...
		if (!mDevice->SetFrameBufferFormat(mChannel, mFrameBufferFormat))
			return false;

		//	Set the channel mode to "playout" (not capture)...
		if (!mDevice->SetMode(mChannel, NTV2_MODE_CAPTURE))
			return false;


		mDevice->WaitForOutputVerticalInterrupt(mChannel, 4);

		//	Enable SDI output from the channel being used, but only if the device supports bi-directional SDI...
		if (::NTV2DeviceHasBiDirectionalSDI(mDevice->GetDeviceID()))
		{
			mDevice->SetSDITransmitEnable(mChannel, false);
		}

		//mVideoFormat = mDevice->GetInputVideoFormat(mInputSource);

		//mDevice->SetRP188Source(mChannel, 0);	//	Set all SDI spigots to capture embedded LTC (VITC could be an option)

		const NTV2Standard VideoStandard(GetNTV2StandardFromVideoFormat(mVideoFormat));
		mDevice->SetSDIOutputStandard(mChannel, VideoStandard);

		RouteInputSignal();

		mDevice->AutoCirculateStop(mChannel);


		//	Sufficient and safe for all devices & FBFs
		const int BuffersPerChannel(7);
		mDevice->AutoCirculateInitForInput(mChannel, BuffersPerChannel,
			NTV2_AUDIOSYSTEM_INVALID,								//	Which audio system?
			AUTOCIRCULATE_WITH_RP188);								//	Add RP188 timecode!

		return true;
	}

	void RouteInputSignal()
	{
		NTV2InputSource InputSource = ::NTV2ChannelToInputSource(mChannel);
		const bool						isInputRGB = mbInputFormatIsRGB;
		const bool						isFrameRGB(::IsRGBFormat(mFrameBufferFormat));
		const NTV2OutputCrosspointID	inputWidgetOutputXpt(::GetInputSourceOutputXpt(InputSource, false, isInputRGB, 0));
		const NTV2InputCrosspointID		frameBufferInputXpt(::GetFrameBufferInputXptFromChannel(mChannel));
		const NTV2InputCrosspointID		cscWidgetVideoInputXpt(::GetCSCInputXptFromChannel(mChannel));
		const NTV2OutputCrosspointID	cscWidgetRGBOutputXpt(::GetCSCOutputXptFromChannel(mChannel, /*inIsKey*/ false, /*inIsRGB*/ true));
		const NTV2OutputCrosspointID	cscWidgetYUVOutputXpt(::GetCSCOutputXptFromChannel(mChannel, /*inIsKey*/ false, /*inIsRGB*/ false));


		if (isInputRGB && !isFrameRGB)
		{
			mDevice->Connect(frameBufferInputXpt, cscWidgetYUVOutputXpt);	//	Frame store input to CSC widget's YUV output
			mDevice->Connect(cscWidgetVideoInputXpt, inputWidgetOutputXpt);	//	CSC widget's RGB input to input widget's output
		}
		else if (!isInputRGB && isFrameRGB)
		{
			mDevice->Connect(frameBufferInputXpt, cscWidgetRGBOutputXpt);	//	Frame store input to CSC widget's RGB output
			mDevice->Connect(cscWidgetVideoInputXpt, inputWidgetOutputXpt);	//	CSC widget's YUV input to input widget's output
		}
		else
			mDevice->Connect(frameBufferInputXpt, inputWidgetOutputXpt);	//	Frame store input to input widget's output
	}

	~FCapture()
	{
		if (mDevice)
		{
			mDevice->SetEveryFrameServices(mSavedTaskMode);													//	Restore prior service level
			mDevice->ReleaseStreamForApplication(kAppSignature, static_cast <uint32_t> (AJAProcess::GetPid()));	//	Release the device
		}
	}

	void Run()
	{
		if (mDevice == nullptr)
		{
			return;
		}

		NTV2VANCMode	vancMode(NTV2_VANCMODE_INVALID);
		NTV2Standard	standard(NTV2_STANDARD_INVALID);
		mDevice->GetVANCMode(vancMode);
		mDevice->GetStandard(standard);

		uint32_t VideoBufferSize = ::GetVideoWriteSize(mVideoFormat, mFrameBufferFormat, vancMode);
		NTV2FormatDescriptor FormatDesc = NTV2FormatDescriptor(standard, mFrameBufferFormat, vancMode);

		//	Allocate and add each in-host AVDataBuffer to my circular buffer member variable...
		uint32_t* VideoBuffer = reinterpret_cast<uint32_t *>(new uint8_t[VideoBufferSize]);


		AJATimeCodeBurn TCBurner;
		TCBurner.RenderTimeCodeFont(GetAJAPixelFormat(mFrameBufferFormat), FormatDesc.numPixels, FormatDesc.numLines - FormatDesc.firstActiveLine);

		mDevice->AutoCirculateStart(mChannel);


		AUTOCIRCULATE_TRANSFER	inputXfer;	//	My A/C input transfer info

		uint32_t FrameCounter = 0;
		bool bExpectFirstLineToBeWhite = true;
		while (true)
		{
			AUTOCIRCULATE_STATUS acStatus;
			mDevice->AutoCirculateGetStatus(mChannel, acStatus);

			if (acStatus.IsRunning() && acStatus.HasAvailableInputFrame())
			{
				if ((FrameCounter % 60) == 0)
				{
					std::cout << "Frame number " << FrameCounter << std::endl;
				}

				inputXfer.SetVideoBuffer(VideoBuffer, VideoBufferSize);
				mDevice->AutoCirculateTransfer(mChannel, inputXfer);

				struct FUYV422Format
				{
					uint8_t Y0;
					uint8_t U;
					uint8_t Y1;
					uint8_t V;
				};
				uint8_t* pVideoBuffer = (uint8_t*)(VideoBuffer)+FormatDesc.firstActiveLine * FormatDesc.linePitch * 4;
				FUYV422Format* pFirstLine = reinterpret_cast<FUYV422Format*>(pVideoBuffer);
				FUYV422Format* pSecondLine = pFirstLine + FormatDesc.linePitch + 4;

				auto IsLineWhite = [](const FUYV422Format* pLine) -> bool { return pLine->Y0 >= 0x0 && pLine->Y0 < 0x20 && pLine->Y1 >= 0x0 && pLine->Y1 < 0x20; };

				if (FrameCounter == 0)
				{
					bExpectFirstLineToBeWhite = IsLineWhite(pFirstLine);
				}

				bool bIsFirstLineWhite = IsLineWhite(pFirstLine);
				bool pIsSecondLineWhite = IsLineWhite(pSecondLine);
				if (bIsFirstLineWhite == pIsSecondLineWhite)
				{
					std::cerr << "The 2 lines are the same color " << FrameCounter << std::endl;
				}
				if (bIsFirstLineWhite != bExpectFirstLineToBeWhite)
				{
					bExpectFirstLineToBeWhite = !bExpectFirstLineToBeWhite;
					std::cerr << "The lines has swap color " << FrameCounter << std::endl;
				}

				++FrameCounter;
			}
			else
			{
				//	Either AutoCirculate is not running, or there were no frames available on the device to transfer.
				//	Rather than waste CPU cycles spinning, waiting until a frame becomes available, it's far more
				//	efficient to wait for the next input vertical interrupt event to get signaled...
				mDevice->WaitForInputVerticalInterrupt(mChannel);
			}
		}


		mDevice->AutoCirculateStop(mChannel);
	}

	bool AskQuestions()
	{
		{
			std::wcout << "AJA Device Index?" << std::endl;
			std::wcin >> mDeviceIndex;
			std::wcout << std::endl;
		}

		{
			int ChannelIndex;
			std::wcout << "What is the channel index? [0..7]" << std::endl;
			std::wcin >> ChannelIndex;
			mChannel = NTV2Channel(ChannelIndex);
			if (!NTV2_IS_VALID_CHANNEL(mChannel))
			{
				std::wcerr << "The channel index should be between 1 and 4" << std::endl;
				return false;
			}
			std::wcout << std::endl;
		}

		{
			int Format;

			std::wcout << "What is the video format index to output? (NB That software do not support drop frame.)" 
				<< std::endl << std::tab << "see ntv2enums.h"
				<< std::endl << std::tab << "NTV2_FORMAT_1080i_5000 == 1"
				<< std::endl << std::tab << "NTV2_FORMAT_1080i_6000 == 3"
				<< std::endl << std::tab << "NTV2_FORMAT_1080p_3000 == 9"
				<< std::endl << std::tab << "NTV2_FORMAT_1080p_2400 == 12"
				<< std::endl << std::tab << "NTV2_FORMAT_1080psf_3000_2 == 30 ";
			std::wcout << std::endl;
			std::wcin >> Format;
			if (!NTV2_IS_VALID_VIDEO_FORMAT(Format))
			{
				std::wcerr << "Wrong video format" << std::endl;
				return false;
			}
			mVideoFormat = NTV2VideoFormat(Format);
			std::wcout << std::endl;
		}

		{
			std::wcout << "Is the incoming pixel format RGB (1/0)?" << std::endl;
			std::wcin >> mbInputFormatIsRGB;
			std::wcout << std::endl;
			mFrameBufferFormat = NTV2_FBF_ARGB;
		}

		//{

		//	std::wcout << "What is the folder path?" << std::endl;
		//	std::cin >> mFolderPath;
		//	if (mFolderPath.empty())
		//	{
		//		std::wcerr << "The path is invalid." << std::endl;
		//		return false;
		//	}
		//	std::wcout << std::endl;
		//}

		return true;
	}
};

int main()
{
	FCapture Burner;
	
	if (Burner.Initialize())
	{
		Burner.Run();
	}

	return 0;
}
