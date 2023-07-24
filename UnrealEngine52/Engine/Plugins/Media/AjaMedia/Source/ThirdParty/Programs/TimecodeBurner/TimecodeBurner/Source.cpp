#define MSWindows 1
#define AJA_WINDOWS 1
#define AJA_NO_AUTOIMPORT 1

const int kAppSignature = 12356;

#include "ajantv2\includes\ntv2card.h"
#include "ajantv2\includes\ntv2enums.h"
#include "ajabase/common/types.h"
#include "ajabase/system/process.h"
#include "ajabase/common/testpatterngen.h"
#include "ajabase/common/options_popt.h"
#include "ajantv2\includes\ntv2utils.h"
#include "ajantv2\includes\ntv2rp188.h"
#include "ajabase\common\timecodeburn.h"

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
	case NTV2_FBF_PRORES:					return AJA_PixelFormat_PRORES;
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

struct FBurner
{
	CNTV2Card* mDevice;
	int mDeviceIndex;
	bool bUseSingle; // or Quad
	bool bUseQuadSquare;
	int mNumStream;
	NTV2VideoFormat mVideoFormat;
	NTV2FrameBufferFormat mFrameBufferFormat;
	NTV2EveryFrameTaskMode mSavedTaskMode;
	std::vector<int> DeltaForEachBuffer;
	std::vector<int> FrameCounters;
	std::vector<unsigned char> SpecialTexel;
	bool bUseRefTimecode;
	bool bWriteCounterInFirstPixel;
	bool bWantInterlacedTest;

	FBurner()
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

		if (!::NTV2DeviceCanDoVideoFormat(mDevice->GetDeviceID(), mVideoFormat))
		{
			std::cerr << "## ERROR:  This device cannot handle '" << ::NTV2VideoFormatToString(mVideoFormat) << "'" << std::endl;
			return false;;
		}

		if (!::NTV2DeviceCanDoFrameBufferFormat(mDevice->GetDeviceID(), mFrameBufferFormat))
		{

			std::cerr << "## WARNING:  This device cannot handle '" << ::NTV2FrameBufferFormatToString(mFrameBufferFormat) << "'. 8bit YCbCr will be used." << std::endl;
			mFrameBufferFormat = NTV2_FBF_8BIT_YCBCR;
		}

		mDevice->ClearRouting();

		if (bUseRefTimecode)
		{
			mDevice->SetLTCInputEnable(true);
		}

		std::cout << "Generating frames..." << std::endl;

		if (bUseSingle)
		{
			for (int Index = 0; Index < mNumStream; ++Index)
			{
				NTV2Channel Channel = NTV2Channel(Index);

				//	Set the video format for the channel's Frame Store to 8-bit YCbCr...
				if (!mDevice->SetVideoFormat(mVideoFormat, Channel))
					return false;

				//	Set the video format for the channel's Frame Store to 8-bit YCbCr...
				if (!mDevice->SetFrameBufferFormat(Channel, mFrameBufferFormat))
					return false;

				//	Enable the Frame Buffer, just in case it's not currently enabled...
				mDevice->EnableChannel(Channel);

				//	Set the channel mode to "playout" (not capture)...
				if (!mDevice->SetMode(Channel, NTV2_MODE_DISPLAY))
					return false;

				//	Enable SDI output from the channel being used, but only if the device supports bi-directional SDI...
				if (::NTV2DeviceHasBiDirectionalSDI(mDevice->GetDeviceID()))
				{
					mDevice->SetSDITransmitEnable(Channel, true);
				}

				mDevice->SetRP188Source(Channel, 0);	//	Set all SDI spigots to capture embedded LTC (VITC could be an option)

				const NTV2Standard VideoStandard(GetNTV2StandardFromVideoFormat(mVideoFormat));
				mDevice->SetSDIOutputStandard(Channel, VideoStandard);

				RouteOutputSignal(Channel);

				mDevice->AutoCirculateStop(Channel);
			}

			for (int Index = 0; Index < mNumStream; ++Index)
			{
				NTV2Channel Channel = NTV2Channel(Index);

				//	Sufficient and safe for all devices & FBFs
				const int BuffersPerChannel(7);
				mDevice->AutoCirculateInitForOutput(Channel, BuffersPerChannel,
					NTV2_AUDIOSYSTEM_INVALID,								//	Which audio system?
					AUTOCIRCULATE_WITH_RP188 | AUTOCIRCULATE_WITH_ANC);		//	Add RP188 timecode!
			}
		}
		else
		{
			//	Configure the device to handle the requested video format...
			mDevice->SetVideoFormat(mVideoFormat, false, false, NTV2Channel::NTV2_CHANNEL1);

			//	VANC data is not processed by this application
			mDevice->SetEnableVANCData(false, false);

			NTV2Channel startChannel = NTV2_CHANNEL1;
			NTV2Channel endChannel = NTV2_CHANNEL5;

			if (!bUseQuadSquare)
			{
				endChannel = NTV2_CHANNEL3;
			}

			for (NTV2Channel chan(startChannel); chan < endChannel; chan = NTV2Channel(chan + 1))
			{
				//	Configure output for HFR Level A and RGB Level B
				if (!::NTV2DeviceCanDo12gRouting(mDevice->GetDeviceID()))
				{
					mDevice->SetSDIOutLevelAtoLevelBConversion(chan, false);
					mDevice->SetSDIOutRGBLevelAConversion(chan, false);
				}

				mDevice->SetFrameBufferFormat(chan, mFrameBufferFormat);
				mDevice->EnableChannel(chan);
				mDevice->SetTsiFrameEnable(!bUseQuadSquare, chan);

				//	Set the channel mode to "playout" (not capture)...
				if (!mDevice->SetMode(chan, NTV2_MODE_DISPLAY))
					return false;

				//	Enable SDI output from the channel being used, but only if the device supports bi-directional SDI...
				if (::NTV2DeviceHasBiDirectionalSDI(mDevice->GetDeviceID()))
				{
					mDevice->SetSDITransmitEnable(chan, true);
				}

				mDevice->SetRP188Source(chan, 0);	//	Set all SDI spigots to capture embedded LTC (VITC could be an option)
			}

			const NTV2Standard VideoStandard(GetNTV2StandardFromVideoFormat(mVideoFormat));
			mDevice->SetSDIOutputStandard(startChannel, VideoStandard);

			mDevice->SubscribeOutputVerticalEvent(startChannel);

			RouteQuadOutputSignal(startChannel);

			mDevice->AutoCirculateStop(startChannel);

			mDevice->AutoCirculateInitForOutput(startChannel, 0
				, NTV2_AUDIOSYSTEM_INVALID
				, AUTOCIRCULATE_WITH_RP188 | AUTOCIRCULATE_WITH_ANC);
		}

		return true;
	}

	~FBurner()
	{
		if (mDevice)
		{
			mDevice->SetEveryFrameServices(mSavedTaskMode);													//	Restore prior service level
			mDevice->ReleaseStreamForApplication(kAppSignature, static_cast <uint32_t> (AJAProcess::GetPid()));	//	Release the device
		}
	}

	void RouteOutputSignal(NTV2Channel Channel)
	{
		const NTV2InputCrosspointID		outputInputXpt(::GetOutputDestInputXpt(::NTV2ChannelToOutputDestination(Channel)));
		const NTV2OutputCrosspointID	fbOutputXpt(::GetFrameBufferOutputXptFromChannel(Channel, ::IsRGBFormat(mFrameBufferFormat)));
		NTV2OutputCrosspointID			outputXpt(fbOutputXpt);

		if (::IsRGBFormat(mFrameBufferFormat))
		{
			const NTV2OutputCrosspointID	cscVidOutputXpt(::GetCSCOutputXptFromChannel(Channel));	//	Use CSC's YUV video output
			const NTV2InputCrosspointID		cscVidInputXpt(::GetCSCInputXptFromChannel(Channel));

			mDevice->Connect(cscVidInputXpt, fbOutputXpt);		//	Connect the CSC's video input to the frame store's output
			mDevice->Connect(outputInputXpt, cscVidOutputXpt);	//	Connect the SDI output's input to the CSC's video output
			outputXpt = cscVidOutputXpt;
		}
		else
			mDevice->Connect(outputInputXpt, outputXpt);
	}

	void RouteQuadOutputSignal(NTV2Channel Channel)
	{
		if (bUseQuadSquare)
		{
			mDevice->Connect(NTV2_XptSDIOut1Input, NTV2_XptFrameBuffer1YUV);
			mDevice->Connect(NTV2_XptSDIOut2Input, NTV2_XptFrameBuffer2YUV);
			mDevice->Connect(NTV2_XptSDIOut3Input, NTV2_XptFrameBuffer3YUV);
			mDevice->Connect(NTV2_XptSDIOut4Input, NTV2_XptFrameBuffer4YUV);
		}
		else
		{
			mDevice->Connect(NTV2_Xpt425Mux1AInput, NTV2_XptFrameBuffer1YUV);
			mDevice->Connect(NTV2_Xpt425Mux1BInput, NTV2_XptFrameBuffer1_425YUV);
			mDevice->Connect(NTV2_Xpt425Mux2AInput, NTV2_XptFrameBuffer2YUV);
			mDevice->Connect(NTV2_Xpt425Mux2BInput, NTV2_XptFrameBuffer2_425YUV);
			mDevice->Connect(NTV2_XptSDIOut1Input, NTV2_Xpt425Mux1ARGB);
			mDevice->Connect(NTV2_XptSDIOut2Input, NTV2_Xpt425Mux1BRGB);
			mDevice->Connect(NTV2_XptSDIOut3Input, NTV2_Xpt425Mux2ARGB);
			mDevice->Connect(NTV2_XptSDIOut4Input, NTV2_Xpt425Mux2BRGB);
		}
	}

	void Run()
	{
		if (mDevice == nullptr)
		{
			return;
		}

		NTV2VANCMode VancMode;
		mDevice->GetVANCMode(VancMode, NTV2_CHANNEL1);

		int VideoBufferSize = GetVideoWriteSize(mVideoFormat, mFrameBufferFormat, VancMode);
		int FramesPerSec = (int)::GetFramesPerSecond(::GetNTV2FrameRateFromVideoFormat(mVideoFormat));
		int FramesPerMin = FramesPerSec * 60;		// 60 seconds/minute
		int FramesPerHr = FramesPerMin * 60;		// 60 minutes/hr.
		int FramesPerDay = FramesPerHr * 24;		// 24 hours/day


		SYSTEMTIME SystemTime;
		GetLocalTime(&SystemTime);
		for (int Index = 0; Index < mNumStream; ++Index)
		{
			NTV2Channel Channel = NTV2Channel(Index);
			mDevice->AutoCirculateStart(Channel);	// Start it running

			const int Frames = ((int)((SystemTime.wMilliseconds / 1000.0) * FramesPerSec) + DeltaForEachBuffer[Index]) + FramesPerHr * SystemTime.wHour + FramesPerMin * SystemTime.wMinute + FramesPerSec * SystemTime.wSecond;

			FrameCounters.push_back(Frames);

			SpecialTexel.push_back(0);
		}

		AUTOCIRCULATE_TRANSFER OutputXferInfo;
		NTV2FormatDescriptor FormatDescriptor = NTV2FormatDescriptor(::GetNTV2StandardFromVideoFormat(mVideoFormat), mFrameBufferFormat);

		AJATestPatternGen TestPatternGen;
		AJATestPatternSelect SelectedPattern = bWantInterlacedTest ? AJATestPatternSelect::AJA_TestPatt_Black : AJATestPatternSelect::AJA_TestPatt_ColorBars100;
		AJATestPatternBuffer TestPatternBuffer;
		TestPatternGen.DrawTestPattern(SelectedPattern,
			FormatDescriptor.numPixels,
			FormatDescriptor.numLines - FormatDescriptor.firstActiveLine,
			GetAJAPixelFormat(mFrameBufferFormat),
			TestPatternBuffer);

		if (bWantInterlacedTest)
		{
			AJATestPatternBuffer SecondPatternBuffer;
			TestPatternGen.DrawTestPattern(AJATestPatternSelect::AJA_TestPatt_White,
				FormatDescriptor.numPixels,
				FormatDescriptor.numLines - FormatDescriptor.firstActiveLine,
				GetAJAPixelFormat(mFrameBufferFormat),
				SecondPatternBuffer);

			for (uint32_t line = 0; line < FormatDescriptor.numLines - FormatDescriptor.firstActiveLine; line+=2)
			{
				void* pDestBuffer = FormatDescriptor.GetWriteableRowAddress((void*)(&TestPatternBuffer[0]), line);
				void* pSrcBuffer = FormatDescriptor.GetWriteableRowAddress((void*)(&SecondPatternBuffer[0]), line);
				memcpy(pDestBuffer, pSrcBuffer, FormatDescriptor.GetBytesPerRow());
			}
		}

		if (FormatDescriptor.firstActiveLine)
		{
			//	Fill the VANC area with something valid -- otherwise the device won't emit a correctly-timed signal...
			unsigned	nVancLines(FormatDescriptor.firstActiveLine);
			uint8_t *	pVancLine = &TestPatternBuffer[0];
			while (nVancLines--)
			{
				if (mFrameBufferFormat == NTV2_FBF_10BIT_YCBCR)
				{
					::Make10BitBlackLine(reinterpret_cast <UWord *> (pVancLine), FormatDescriptor.numPixels);
					::PackLine_16BitYUVto10BitYUV(reinterpret_cast <const UWord *>(pVancLine), reinterpret_cast <ULWord *> (pVancLine), FormatDescriptor.numPixels);
				}
				else if (mFrameBufferFormat == NTV2_FBF_8BIT_YCBCR)
				{
					::Make8BitBlackLine(pVancLine, FormatDescriptor.numPixels);
				}
				else
				{
					std::cerr << "## ERROR:  Cannot initialize video buffer's VANC area" << std::endl;
					return;
				}
				pVancLine += FormatDescriptor.linePitch * 4;
			}	//	for each VANC line
		}	//	if has VANC area


		AJATimeCodeBurn TCBurner;
		TCBurner.RenderTimeCodeFont(GetAJAPixelFormat(mFrameBufferFormat), FormatDescriptor.numPixels, FormatDescriptor.numLines - FormatDescriptor.firstActiveLine);


		time_t LastTime = time(NULL);
		WCHAR Buffer[4][128] = { L"", L"", L"", L"" };

		while (true)
		{
			bool bOutput = false;
			time_t Now = time(NULL);

			if (LastTime != Now)
			{
				bOutput = true;
				LastTime = Now;
			}

			RP188_STRUCT TimecodeValue;

			if (bUseRefTimecode)
			{
				mDevice->ReadRegister(kRegLTCAnalogBits0_31, TimecodeValue.Low);
				mDevice->ReadRegister(kRegLTCAnalogBits32_63, TimecodeValue.High);
			}

			for (int Index = 0; Index < mNumStream; ++Index)
			{
				NTV2Channel Channel = NTV2Channel(Index);

				AUTOCIRCULATE_STATUS OutputStatus;
				mDevice->AutoCirculateGetStatus(Channel, OutputStatus);

				//static ULWord lastFrameDropped = OutputStatus.acFramesDropped;
				//if (lastFrameDropped != OutputStatus.acFramesDropped)
				//{
				//	lastFrameDropped = OutputStatus.acFramesDropped;
				//	std::wcerr << "Dropped frames";
				//}

				static ULWord lastFramesProcessed = OutputStatus.acFramesProcessed;
				if (OutputStatus.GetNumAvailableOutputFrames() > 1)
				{
					//if (lastFramesProcessed + 1 != OutputStatus.acFramesProcessed)
					//{
					//	std::wcerr << "Last not ok";
					//}
					//lastFramesProcessed = OutputStatus.acFramesProcessed;

					++FrameCounters[Index];
					CRP188 FrameRP188Info;
					{
						TimecodeFormat TimecodeFormat = kTCFormatUnknown;
						switch (::GetNTV2FrameRateFromVideoFormat(mVideoFormat))
						{
						case NTV2_FRAMERATE_6000:	TimecodeFormat = kTCFormat60fps;	break;
						case NTV2_FRAMERATE_5994:	TimecodeFormat = kTCFormat60fpsDF;	break;
						case NTV2_FRAMERATE_4800:	TimecodeFormat = kTCFormat48fps;	break;
						case NTV2_FRAMERATE_4795:	TimecodeFormat = kTCFormat48fps;	break;
						case NTV2_FRAMERATE_3000:	TimecodeFormat = kTCFormat30fps;	break;
						case NTV2_FRAMERATE_2997:	TimecodeFormat = kTCFormat30fpsDF;	break;
						case NTV2_FRAMERATE_2500:	TimecodeFormat = kTCFormat25fps;	break;
						case NTV2_FRAMERATE_2400:	TimecodeFormat = kTCFormat24fps;	break;
						case NTV2_FRAMERATE_2398:	TimecodeFormat = kTCFormat24fps;	break;
						case NTV2_FRAMERATE_5000:	TimecodeFormat = kTCFormat50fps;	break;
						default:					break;
						}

						if (bUseRefTimecode)
						{
							FrameRP188Info.SetRP188(TimecodeValue);

							ULWord FrameCount = 0;
							FrameRP188Info.GetFrameCount(FrameCount);

							// Adjust delta
							FrameCount += DeltaForEachBuffer[Index];

							FrameRP188Info.SetRP188(FrameCount, TimecodeFormat);
						}
						else
						{
							FrameRP188Info.SetRP188(FrameCounters[Index], TimecodeFormat);
						}
					}

					std::string TimeCodeString;
					FrameRP188Info.GetRP188Str(TimeCodeString);

					uint8_t* pVideoBuffer = &TestPatternBuffer[0] + FormatDescriptor.firstActiveLine * FormatDescriptor.linePitch * 4;
					TCBurner.BurnTimeCode((char*)pVideoBuffer, TimeCodeString.c_str(), 80);

					NTV2_RP188 Timecode;
					FrameRP188Info.GetRP188Reg(Timecode);

					ULWord F, M, S, H;
					FrameRP188Info.GetRP188Frms(F);
					FrameRP188Info.GetRP188Secs(S);
					FrameRP188Info.GetRP188Mins(M);
					FrameRP188Info.GetRP188Hrs(H);

					// Cache timecode for next print
					wsprintf(Buffer[Index], L"%d: %02d:%02d:%02d:%02d  ", Index, H, M, S, F);

					// Write a increasing value on the first bit to test drop frames in Unreal Engine
					if (bWriteCounterInFirstPixel)
					{
						// 0 & 255 value are clipped in YUV format
						if (SpecialTexel[Index] == 254)
						{
							SpecialTexel[Index] = 1;
						}
						else
						{
							++SpecialTexel[Index];
						}

						uint8_t* pDestBufferF1 = (uint8_t*)(FormatDescriptor.GetWriteableRowAddress(TestPatternBuffer, 0));
						pDestBufferF1[0] = SpecialTexel[Index];

						if (!NTV2_VIDEO_FORMAT_HAS_PROGRESSIVE_PICTURE(mVideoFormat))
						{
							if (SpecialTexel[Index] == 254)
							{
								SpecialTexel[Index] = 1;
							}
							else
							{
								++SpecialTexel[Index];
							}
							uint8_t* pDestBufferF2 = (uint8_t*)(FormatDescriptor.GetWriteableRowAddress(TestPatternBuffer, 1));
							*pDestBufferF2 = SpecialTexel[Index];
						}
					}

					OutputXferInfo.SetAllOutputTimeCodes(Timecode);
					OutputXferInfo.SetVideoBuffer((ULWord*)pVideoBuffer, (ULWord)TestPatternBuffer.size());

					mDevice->AutoCirculateTransfer(Channel, OutputXferInfo);
				}
			}

			if (bOutput)
			{
				WCHAR BufferTotal[128] = L"";

				// Combine all output
				for (int Index = 0; Index < mNumStream; ++Index)
				{
					wcscat_s(BufferTotal, Buffer[Index]);
				}
				std::wcout << BufferTotal << std::endl;
			}

			::Sleep(0);
		}
	}

	bool AskQuestions()
	{
		{
			std::wcout << "AJA Device Index?" << std::endl;
			std::wcin >> mDeviceIndex;
			std::wcout << std::endl;
		}

		{
			std::wcout << "Single [1] or Quad [0]?" << std::endl;
			std::wcin >> bUseSingle;
			std::wcout << std::endl;
		}

		if (bUseSingle)
		{
			std::wcout << "How many stream? [1..4]" << std::endl;
			std::wcin >> mNumStream;
			if (mNumStream < 1 || mNumStream > 4)
			{
				std::wcerr << "The Number of stream should be between 1 and 4" << std::endl;
				return false;
			}
			std::wcout << std::endl;
			bUseQuadSquare = true;
		}
		else
		{
			std::wcout << "Square [1] or TSI [0]?" << std::endl;
			std::wcin >> bUseQuadSquare;
			std::wcout << std::endl;
			mNumStream = 1;
		}

		{
			int Format;

			if (bUseSingle)
			{
				std::wcout << "What is the video format index to output? (NB That software do not support drop frame.)"
					<< std::endl << std::tab << "see ntv2enums.h"
					<< std::endl << std::tab << "NTV2_FORMAT_1080i_5000 == 1"
					<< std::endl << std::tab << "NTV2_FORMAT_1080i_6000 == 3"
					<< std::endl << std::tab << "NTV2_FORMAT_1080p_3000 == 9"
					<< std::endl << std::tab << "NTV2_FORMAT_1080p_2400 == 12"
					<< std::endl << std::tab << "NTV2_FORMAT_1080psf_3000_2 == 30 "
					<< std::endl;
			}
			else
			{
				std::wcout << "What is the video format index to output? (NB That software do not support drop frame.)"
					<< std::endl << std::tab << "see ntv2enums.h"
					<< std::endl << std::tab << "NTV2_FORMAT_4x1920x1080p_2400 == 84"
					<< std::endl << std::tab << "NTV2_FORMAT_4x1920x1080p_3000 == 93"
					<< std::endl << std::tab << "NTV2_FORMAT_4x1920x1080p_5000 == 100"
					<< std::endl << std::tab << "NTV2_FORMAT_4x1920x1080p_6000 == 102"
					<< std::endl;
			}
			std::wcin >> Format;
			if (!NTV2_IS_VALID_VIDEO_FORMAT(Format))
			{
				std::wcerr << "Wrong video format" << std::endl;
				return false;
			}
			if (bUseSingle && !NTV2_IS_HD_VIDEO_FORMAT(Format))
			{
				std::wcerr << "Not a HD format" << std::endl;
				return false;
			}
			if (!bUseSingle && !NTV2_IS_QUAD_FRAME_FORMAT(Format))
			{
				std::wcerr << "Not a QUAD format" << std::endl;
				return false;
			}
			mVideoFormat = NTV2VideoFormat(Format);
			std::wcout << std::endl;
		}

		{
			bWantInterlacedTest = false;
			if (!NTV2_VIDEO_FORMAT_HAS_PROGRESSIVE_PICTURE(mVideoFormat))
			{
				std::wcout << "Would you like to test the interlaced format? (y/n)" << std::endl;
				WCHAR input;
				std::wcin >> input;
				bWantInterlacedTest = (input == L'y' || input == L'Y');
				std::wcout << std::endl;
			}
		}

		{
			int Pixel;

			if (bUseSingle)
			{
				std::wcout << "What is the pixel format index to output?"
					<< std::endl << std::tab << "see ntv2enums.h"
					<< std::endl << std::tab << "NTV2_FBF_10BIT_YCBCR == 0"
					<< std::endl << std::tab << "NTV2_FBF_8BIT_YCBCR == 1"
					<< std::endl << std::tab << "NTV2_FBF_ARGB == 2"
					<< std::endl << std::tab << "NTV2_FBF_10BIT_RGB == 4";
			}
			else
			{
				std::wcout << "What is the pixel format index to output? (NB That software do not support RGB in Quad.)"
					<< std::endl << std::tab << "see ntv2enums.h"
					<< std::endl << std::tab << "NTV2_FBF_10BIT_YCBCR == 0"
					<< std::endl << std::tab << "NTV2_FBF_8BIT_YCBCR == 1";
			}
			std::wcout << std::endl;
			std::wcin >> Pixel;
			if (!NTV2_IS_VALID_FRAME_BUFFER_FORMAT(Pixel))
			{
				std::wcerr << "Wrong pixel format" << std::endl;
				return false;
			}
			if (!bUseSingle && NTV2_IS_FBF_RGB(Pixel))
			{
				std::wcerr << "RGB is not support in quad in that software." << std::endl;
				return false;
			}
			mFrameBufferFormat = NTV2FrameBufferFormat(Pixel);
			std::wcout << std::endl;
		}

		{
			for(int Index = 0; Index < mNumStream; ++Index)
			{
				int Delta;
				std::wcout << "What is timecode delta for buffer " << Index << "? (may be negative)" << std::endl;
				std::wcin >> Delta;
				DeltaForEachBuffer.push_back(Delta);
			}
			std::wcout << std::endl;
		}

		{
			std::wcout << "Do you want to use ref pin timecode? (y/n)" << std::endl;
			WCHAR input;
			std::wcin >> input;
			bUseRefTimecode = (input == L'y' || input == L'Y');
			std::wcout << std::endl;
		}

		{ 
			std::wcout << "Do you want to write the counter in the first word? (y/n)" << std::endl;
			WCHAR input;
			std::wcin >> input;
			bWriteCounterInFirstPixel = (input == L'y' || input == L'Y');
			std::wcout << std::endl;
		}

		return true;
	}
};

int main()
{
	FBurner Burner;
	
	if (Burner.Initialize())
	{
		Burner.Run();
	}

	return 0;
}
