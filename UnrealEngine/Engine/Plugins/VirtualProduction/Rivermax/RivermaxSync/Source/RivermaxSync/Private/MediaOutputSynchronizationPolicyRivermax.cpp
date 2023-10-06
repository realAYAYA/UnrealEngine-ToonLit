// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaOutputSynchronizationPolicyRivermax.h"

#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "RivermaxMediaCapture.h"
#include "RivermaxPTPUtils.h"


FMediaOutputSynchronizationPolicyRivermaxHandler::FMediaOutputSynchronizationPolicyRivermaxHandler(UMediaOutputSynchronizationPolicyRivermax* InPolicyObject)
	: Super(InPolicyObject)
{

}

TSubclassOf<UDisplayClusterMediaOutputSynchronizationPolicy> FMediaOutputSynchronizationPolicyRivermaxHandler::GetPolicyClass() const
{
	return UMediaOutputSynchronizationPolicyRivermax::StaticClass();
}

bool FMediaOutputSynchronizationPolicyRivermaxHandler::IsCaptureTypeSupported(UMediaCapture* MediaCapture) const
{
	// We need to make sure:
	// - it's RivermaxCapture
	// - it uses PTP or System time source
	// - it uses AlignmentPoint alignment mode
	if (URivermaxMediaCapture* RmaxCapture = Cast<URivermaxMediaCapture>(MediaCapture))
	{
		using namespace UE::RivermaxCore;

		if (TSharedPtr<IRivermaxManager> RivermaxMgr = IRivermaxCoreModule::Get().GetRivermaxManager())
		{
			const ERivermaxTimeSource TimeSource = RivermaxMgr->GetTimeSource();

			if (TimeSource == ERivermaxTimeSource::PTP || TimeSource == ERivermaxTimeSource::System)
			{
				const FRivermaxOutputStreamOptions Options = RmaxCapture->GetOutputStreamOptions();

				if (Options.AlignmentMode == ERivermaxAlignmentMode::AlignmentPoint)
				{
					return true;
				}
			}
		}
	}

	return false;
}

double FMediaOutputSynchronizationPolicyRivermaxHandler::GetTimeBeforeNextSyncPoint()
{
	if (URivermaxMediaCapture* RmaxCapture = Cast<URivermaxMediaCapture>(CapturingDevice))
	{
		if (RmaxCapture->GetState() == EMediaCaptureState::Capturing)
		{
			using namespace UE::RivermaxCore;

			if (TSharedPtr<IRivermaxManager> RivermaxMgr = IRivermaxCoreModule::Get().GetRivermaxManager())
			{
				// Get current time
				const uint64 CurrentTimeNanosec = RivermaxMgr->GetTime();

				// Get next alignment timepoint
				FRivermaxOutputStreamOptions Options = RmaxCapture->GetOutputStreamOptions();
				const uint64 NextAlignmentTimeNanosec = GetNextAlignmentPoint(CurrentTimeNanosec, Options.FrameRate);

				// Time left
				checkSlow(NextAlignmentTimeNanosec > CurrentTimeNanosec);
				const uint64 TimeLeftNanosec = NextAlignmentTimeNanosec - CurrentTimeNanosec;

				// Return remaining time in seconds
				return double(TimeLeftNanosec * 1E9);
			}
		}
	}

	// Normally we should never get here. As a fallback approach, return some big time interval
	// to prevent calling thread blocking. 1 second is more than any possible threshold.
	return 1.f;
}

TSharedPtr<IDisplayClusterMediaOutputSynchronizationPolicyHandler> UMediaOutputSynchronizationPolicyRivermax::GetHandler()
{
	if (!Handler)
	{
		Handler = MakeShared<FMediaOutputSynchronizationPolicyRivermaxHandler>(this);
	}

	return Handler;
}
