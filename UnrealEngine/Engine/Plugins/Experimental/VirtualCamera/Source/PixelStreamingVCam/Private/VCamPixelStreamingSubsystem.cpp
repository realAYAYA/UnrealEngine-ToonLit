// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamPixelStreamingSubsystem.h"
#include "ILiveLinkClient.h"
#include "VCamPixelStreamingLiveLink.h"
#include "VCamPixelStreamingSession.h"
#include "PixelStreamingVCamLog.h"
#include "PixelStreamingEditorModule.h"

void UVCamPixelStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UVCamPixelStreamingSubsystem::Deinitialize()
{
	Super::Deinitialize();
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (LiveLinkSource && ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		LiveLinkClient->RemoveSource(LiveLinkSource);
	}
	LiveLinkSource.Reset();
}

UVCamPixelStreamingSubsystem* UVCamPixelStreamingSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UVCamPixelStreamingSubsystem>() : nullptr;
}

void UVCamPixelStreamingSubsystem::RegisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider)
{
	if (!OutputProvider) return;
	
	ActiveOutputProviders.AddUnique(OutputProvider);
	if (LiveLinkSource)
	{
		LiveLinkSource->CreateSubject(OutputProvider->GetFName());
		LiveLinkSource->PushTransformForSubject(OutputProvider->GetFName(), FTransform::Identity);
	}
}

void UVCamPixelStreamingSubsystem::UnregisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider)
{
	if (!OutputProvider) return;
	if (ActiveOutputProviders.Remove(OutputProvider) && LiveLinkSource)
	{
		LiveLinkSource->RemoveSubject(OutputProvider->GetFName());
	}
}

TSharedPtr<FPixelStreamingLiveLinkSource> UVCamPixelStreamingSubsystem::TryGetLiveLinkSource()
{
	if (!LiveLinkSource.IsValid())
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			LiveLinkSource = MakeShared<FPixelStreamingLiveLinkSource>();
			LiveLinkClient->AddSource(LiveLinkSource);
		}
	}
	return LiveLinkSource;
}

TSharedPtr<FPixelStreamingLiveLinkSource> UVCamPixelStreamingSubsystem::TryGetLiveLinkSource(UVCamPixelStreamingSession* OutputProvider)
{
	if (!LiveLinkSource.IsValid())
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			LiveLinkSource = MakeShared<FPixelStreamingLiveLinkSource>();
			LiveLinkClient->AddSource(LiveLinkSource);

			if (IsValid(OutputProvider))
			{
				ActiveOutputProviders.AddUnique(OutputProvider);
				LiveLinkSource->CreateSubject(OutputProvider->GetFName());
				LiveLinkSource->PushTransformForSubject(OutputProvider->GetFName(), FTransform::Identity);
			}
		}
	}
	return LiveLinkSource;
}

void UVCamPixelStreamingSubsystem::LaunchSignallingServer()
{
	FPixelStreamingEditorModule::GetModule()->StartSignalling();
}

void UVCamPixelStreamingSubsystem::StopSignallingServer()
{
	FPixelStreamingEditorModule::GetModule()->StopSignalling();
}