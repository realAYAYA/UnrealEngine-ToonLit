// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteFeedback.h"

#include "ScenePrivate.h"
#include "RendererModule.h"
#include "RendererOnScreenNotification.h"

#include "GPUMessaging.h"

#define LOCTEXT_NAMESPACE "NaniteFeedbackStatus"

static TAutoConsoleVariable<int32> CVarEmitMaterialPerformanceWarnings(
	TEXT("r.Nanite.EmitMaterialPerformanceWarnings"),
	0,
	TEXT("Emit log and on-screen messages to warn when a Nanite material is both programmable and using either masking or pixel depth offset (PDO)."),
	ECVF_RenderThreadSafe
);

namespace Nanite
{

#if !UE_BUILD_SHIPPING
FFeedbackManager::FFeedbackManager()
{
	StatusFeedbackSocket = GPUMessage::RegisterHandler(TEXT("Nanite.StatusFeedback"),
		[this](GPUMessage::FReader Message)
	{
		const uint32 MaxNodes				= Nanite::FGlobalResources::GetMaxNodes();
		const uint32 MaxCandidateClusters	= Nanite::FGlobalResources::GetMaxCandidateClusters();
		const uint32 MaxVisibleClusters		= Nanite::FGlobalResources::GetMaxVisibleClusters();

		const uint32 PeakNodes				= Message.Read<uint32>(0);
		const uint32 PeakCandidateClusters	= Message.Read<uint32>(0);
		const uint32 PeakVisibleClusters	= Message.Read<uint32>(0);

		if (NodeState.Update(PeakNodes, MaxNodes))
		{
			UE_LOG(LogRenderer, Warning, TEXT(	"Nanite node buffer overflow detected. New high-water mark is %d / %d. "
												"Increase r.Nanite.MaxNodes to prevent potential visual artifacts."), NodeState.HighWaterMark, MaxNodes);
		}
		
		if (CandidateClusterState.Update(PeakCandidateClusters, MaxCandidateClusters))
		{
			UE_LOG(LogRenderer, Warning, TEXT(	"Nanite candidate cluster buffer overflow detected. New high-water mark is %d / %d. "
												"Increase r.Nanite.MaxCandidateClusters to prevent potential visual artifacts."), CandidateClusterState.HighWaterMark, MaxCandidateClusters);
		}
		
		if (VisibleClusterState.Update(PeakVisibleClusters, MaxVisibleClusters))
		{
			UE_LOG(LogRenderer, Warning, TEXT(	"Nanite visible cluster buffer overflow detected. New high-water mark is %d / %d. "
												"Increase r.Nanite.MaxVisibleClusters to prevent potential visual artifacts."), VisibleClusterState.HighWaterMark, MaxVisibleClusters);
		}
	});


	// TODO: Should be FRendererOnScreenNotification::Get(). Temporary workaround for singleton initialization issue.
	//       The problem is that the FFeedbackManager lives inside Nanite::FGlobalResources which is a global resource, released by the system after the 
	//       FRendererOnScreenNotification singleton is destroyed.
	// WARNING: FCoreDelegates::OnGetOnScreenMessages is invoked from the Game Thread!
	ScreenMessageDelegate = FCoreDelegates::OnGetOnScreenMessages.AddLambda(	
		[this](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText>& OutMessages)
	{
		const uint32 MaxNodes				= Nanite::FGlobalResources::GetMaxNodes();
		const uint32 MaxCandidateClusters	= Nanite::FGlobalResources::GetMaxCandidateClusters();
		const uint32 MaxVisibleClusters		= Nanite::FGlobalResources::GetMaxVisibleClusters();

		const double ShowWarningSeconds = 5.0;
		const double CurrentTime = FPlatformTime::Seconds();

		if (CurrentTime - NodeState.LatestOverflowTime < ShowWarningSeconds)
		{
			OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning,
				FText::Format(LOCTEXT("NaniteNodeOverflow",
					"Nanite node buffer overflow detected: {0} / {1}. High-water mark is {2}. "
					"Increase r.Nanite.MaxNodes to prevent potential visual artifacts."),
					NodeState.LatestOverflowPeak, MaxNodes, NodeState.HighWaterMark));
		}

		if (CurrentTime - CandidateClusterState.LatestOverflowTime < ShowWarningSeconds)
		{
			OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning,
				FText::Format(LOCTEXT("NaniteCandidateClusterOverflow",
					"Nanite candidate cluster buffer overflow detected: {0} / {1}. High-water mark is {2}. "
					"Increase r.Nanite.MaxCandidateClusters to prevent potential visual artifacts."),
					CandidateClusterState.LatestOverflowPeak, MaxCandidateClusters, CandidateClusterState.HighWaterMark));
		}

		if (CurrentTime - VisibleClusterState.LatestOverflowTime < ShowWarningSeconds)
		{
			OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning,
				FText::Format(LOCTEXT("NaniteVisibleClusterOverflow",
					"Nanite visible cluster buffer overflow detected: {0} / {1}. High-water mark is {2}. "
					"Increase r.Nanite.MaxVisibleClusters to prevent potential visual artifacts."),
					VisibleClusterState.LatestOverflowPeak, MaxVisibleClusters, VisibleClusterState.HighWaterMark));
		}

		if (CVarEmitMaterialPerformanceWarnings.GetValueOnAnyThread() != 0)
		{
			FScopeLock Lock(&DelgateCallbackCS);

			for (const auto& Item : MaterialWarningItems)
			{
				if (CurrentTime - Item.Value.LastTimeSeen < 2.5f)
				{
					OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning, FText::FromString(FString::Printf(TEXT("Performance Warning: Programmable Nanite material '%s' uses PDO or is Masked!"), *Item.Key)));
				}
			}

			// Strip old material warning items
			MaterialWarningItems = MaterialWarningItems.FilterByPredicate([CurrentTime](const TMap<FString, FMaterialWarningItem>::ElementType& Element)
			{
				return CurrentTime - Element.Value.LastTimeSeen < 5.0f;
			});
		}
		else
		{
			FScopeLock Lock(&DelgateCallbackCS);

			MaterialWarningItems.Empty();
		}

	});
}

FFeedbackManager::~FFeedbackManager()
{
	FCoreDelegates::OnGetOnScreenMessages.Remove(ScreenMessageDelegate);	// TODO: Should be FRendererOnScreenNotification::Get(). Temporary workaround for singleton initialization issue.
}

bool FFeedbackManager::FBufferState::Update(const uint32 Peak, const uint32 Capacity)
{
	bool bNewHighWaterMark = false;
	if (Peak > Capacity)
	{
		LatestOverflowTime = FPlatformTime::Seconds();
		LatestOverflowPeak = Peak;
		bNewHighWaterMark = (Peak > HighWaterMark);
	}
	HighWaterMark = FMath::Max(HighWaterMark, Peak);
	return bNewHighWaterMark;
}

void FFeedbackManager::ReportMaterialPerformanceWarning(const FString &MaterialName)
{
	bool bShouldLogNow = false;
	{
		FScopeLock Lock(&DelgateCallbackCS);
		FMaterialWarningItem& Item = MaterialWarningItems.FindOrAdd(MaterialName);
		const double CurrentTime = FPlatformTime::Seconds();
		bShouldLogNow = CurrentTime - Item.LastTimeLogged > 5.0f;
		Item.LastTimeSeen = CurrentTime;
		if (bShouldLogNow)
		{
			Item.LastTimeLogged = CurrentTime;
		}
	}
	// Keep logging outside critical section
	if (bShouldLogNow)
	{
		UE_LOG(LogRenderer, Log, TEXT("Performance Warning: Programmable Nanite material uses PDO or is Masked, %s"), *MaterialName);
	}
}

bool ShouldReportFeedbackMaterialPerformanceWarning()
{
	return CVarEmitMaterialPerformanceWarnings.GetValueOnRenderThread() != 0;
}


#endif // !UE_BUILD_SHIPPING

}	// namespace Nanite

#undef LOCTEXT_NAMESPACE