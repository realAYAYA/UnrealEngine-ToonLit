// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoSourceGroup.h"
#include "VideoSource.h"
#include "Settings.h"
#include "PixelStreamingPrivate.h"

namespace UE::PixelStreaming
{
	TSharedPtr<FVideoSourceGroup> FVideoSourceGroup::Create()
	{
		return TSharedPtr<FVideoSourceGroup>(new FVideoSourceGroup());
	}

	FVideoSourceGroup::FVideoSourceGroup()
		: bCoupleFramerate(Settings::CoupleFrameRate() || !Settings::IsUsingSafeTextureCopy())
		, FramesPerSecond(Settings::CVarPixelStreamingWebRTCFps.GetValueOnAnyThread())
	{
	}

	FVideoSourceGroup::~FVideoSourceGroup()
	{
		Stop();
	}

	void FVideoSourceGroup::SetVideoInput(TSharedPtr<FPixelStreamingVideoInput> InVideoInput)
	{
		if (VideoInput)
		{
			VideoInput->OnFrameCaptured.Remove(FrameDelegateHandle);
		}

		VideoInput = InVideoInput;

		if (VideoInput)
		{
			FrameDelegateHandle = VideoInput->OnFrameCaptured.AddSP(AsShared(), &FVideoSourceGroup::OnFrameCaptured);
		}
	}

	void FVideoSourceGroup::SetFPS(int32 InFramesPerSecond)
	{
		FramesPerSecond = InFramesPerSecond;
	}

	int32 FVideoSourceGroup::GetFPS()
	{
		return FramesPerSecond;
	}

	void FVideoSourceGroup::SetCoupleFramerate(bool Couple)
	{
		bCoupleFramerate = Couple;
	}

	rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> FVideoSourceGroup::CreateVideoSource(const TFunction<bool()>& InShouldGenerateFramesCheck)
	{
		rtc::scoped_refptr<FVideoSource> NewVideoSource = new FVideoSource(VideoInput, InShouldGenerateFramesCheck);
		{
			FScopeLock Lock(&CriticalSection);
			VideoSources.Add(NewVideoSource);
		}
		CheckStartStopThread();
		return NewVideoSource;
	}

	void FVideoSourceGroup::RemoveVideoSource(const webrtc::VideoTrackSourceInterface* ToRemove)
	{
		{
			FScopeLock Lock(&CriticalSection);
			VideoSources.RemoveAll([ToRemove](const rtc::scoped_refptr<FVideoSource>& Target) {
				return Target.get() == ToRemove;
			});
		}
		CheckStartStopThread();
	}

	void FVideoSourceGroup::RemoveAllVideoSources()
	{
		{
			FScopeLock Lock(&CriticalSection);
			VideoSources.Empty();
		}
		CheckStartStopThread();
	}

	void FVideoSourceGroup::Start()
	{
		if (!bRunning)
		{
			if (VideoSources.Num() > 0)
			{
				StartThread();
			}
			bRunning = true;
		}
	}

	void FVideoSourceGroup::Stop()
	{
		if (bRunning)
		{
			StopThread();
			bRunning = false;
		}
	}

	void FVideoSourceGroup::Tick()
	{
		FScopeLock Lock(&CriticalSection);
		// for each player session, push a frame
		for (auto& VideoSource : VideoSources)
		{
			if (VideoSource)
			{
				VideoSource->MaybePushFrame();
			}
		}
	}

	void FVideoSourceGroup::OnFrameCaptured()
	{
		if (bCoupleFramerate)
		{
			Tick();
		}
	}

	void FVideoSourceGroup::StartThread()
	{
		if (!bCoupleFramerate && !bThreadRunning)
		{
			FrameRunnable = MakeUnique<FFrameThread>(this);
			FrameThread = FRunnableThread::Create(FrameRunnable.Get(), TEXT("FVideoSourceGroup Thread"));
			bThreadRunning = true;
		}
	}

	void FVideoSourceGroup::StopThread()
	{
		if (FrameThread != nullptr)
		{
			FrameThread->Kill(true);
		}
		FrameThread = nullptr;
		bThreadRunning = false;
	}

	void FVideoSourceGroup::CheckStartStopThread()
	{
		if (bRunning)
		{
			const int32 NumSources = VideoSources.Num();
			if (bThreadRunning && NumSources == 0)
			{
				StopThread();
			}
			else if (!bThreadRunning && NumSources > 0)
			{
				StartThread();
			}
		}
	}

	bool FVideoSourceGroup::FFrameThread::Init()
	{
		return true;
	}

	uint32 FVideoSourceGroup::FFrameThread::Run()
	{
		bIsRunning = true;

		while (bIsRunning)
		{
			uint64 PreTickCycles = FPlatformTime::Cycles64();

			PushFrame();

			const double DeltaMs = FPlatformTime::ToMilliseconds64(LastTickCycles - PreTickCycles);
			const double FrameDeltaMs = 1000.0 / TickGroup->FramesPerSecond;
			const double SleepMs = FrameDeltaMs - DeltaMs;
			PreTickCycles = LastTickCycles;

			// Sleep as long as we need for a constant FPS
			if (SleepMs > 0)
			{
				FPlatformProcess::Sleep(static_cast<float>(SleepMs / 1000.0));
			}
		}

		return 0;
	}

	void FVideoSourceGroup::FFrameThread::Stop()
	{
		bIsRunning = false;
	}

	void FVideoSourceGroup::FFrameThread::Exit()
	{
		bIsRunning = false;
	}

	void FVideoSourceGroup::FFrameThread::Tick()
	{
		const uint64 NowCycles = FPlatformTime::Cycles64();
		const double DeltaMs = FPlatformTime::ToMilliseconds64(NowCycles - LastTickCycles);
		const double FrameDeltaMs = 1000.0 / TickGroup->FramesPerSecond;
		if (DeltaMs >= FrameDeltaMs)
		{
			PushFrame();
		}
	}

	void FVideoSourceGroup::FFrameThread::PushFrame()
	{
		TickGroup->Tick();
		LastTickCycles = FPlatformTime::Cycles64();
	}
} // namespace UE::PixelStreaming
