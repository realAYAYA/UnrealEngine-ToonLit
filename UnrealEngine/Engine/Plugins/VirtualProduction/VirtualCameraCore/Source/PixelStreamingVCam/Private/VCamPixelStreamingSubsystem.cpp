// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamPixelStreamingSubsystem.h"

#include "BuiltinProviders/VCamPixelStreamingSession.h"
#include "VCamPixelStreamingLiveLink.h"

#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "IPixelStreamingEditorModule.h"

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
	if (ensure(OutputProvider) && LiveLinkSource)
	{
		FName SubjectName = FName(OutputProvider->StreamerId);
		LiveLinkSource->CreateSubject(SubjectName);
		LiveLinkSource->PushTransformForSubject(SubjectName, FTransform::Identity);
	}
}

void UVCamPixelStreamingSubsystem::UnregisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider)
{
	if (ensure(OutputProvider) && LiveLinkSource)
	{
		FName SubjectName = FName(OutputProvider->StreamerId);
		LiveLinkSource->RemoveSubject(SubjectName);
	}
}

TSharedPtr<FPixelStreamingLiveLinkSource> UVCamPixelStreamingSubsystem::TryGetLiveLinkSource(UVCamPixelStreamingSession* OutputProvider)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		return nullptr;
	}

	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	if (!LiveLinkSource.IsValid()
		// User can manually remove live link sources via UI
		|| !LiveLinkClient->HasSourceBeenAdded(LiveLinkSource))
	{
		LiveLinkSource = MakeShared<FPixelStreamingLiveLinkSource>();
		LiveLinkClient->AddSource(LiveLinkSource);

		if (IsValid(OutputProvider))
		{
			FName SubjectName = FName(OutputProvider->StreamerId);
			LiveLinkSource->CreateSubject(SubjectName);
			LiveLinkSource->PushTransformForSubject(SubjectName, FTransform::Identity);
		}
	}
	return LiveLinkSource;
}

void UVCamPixelStreamingSubsystem::LaunchSignallingServer()
{
	IPixelStreamingEditorModule::Get().StartSignalling();
}

void UVCamPixelStreamingSubsystem::StopSignallingServer()
{
	IPixelStreamingEditorModule::Get().StopSignalling();
}