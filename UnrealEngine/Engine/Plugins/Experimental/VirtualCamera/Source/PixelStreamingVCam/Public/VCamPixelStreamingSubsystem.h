// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "PixelStreamingServers.h"
#include "VCamPixelStreamingSubsystem.generated.h"

class FPixelStreamingLiveLinkSource;
class UVCamPixelStreamingSession;

UCLASS()
class PIXELSTREAMINGVCAM_API UVCamPixelStreamingSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// Convenience function for accessing the subsystem
	static UVCamPixelStreamingSubsystem* Get();
	void RegisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider);
	void UnregisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider);

	// Get the LiveLinkSource if it already exists or attempt to create one
	TSharedPtr<FPixelStreamingLiveLinkSource> TryGetLiveLinkSource();
	
	// Get the LiveLinkSource if it already exists or attempt to create one
	// Additionally registers the OutputProvider to the Source if a new source was created
	TSharedPtr<FPixelStreamingLiveLinkSource> TryGetLiveLinkSource(UVCamPixelStreamingSession* OutputProvider);

	void LaunchSignallingServer();
	void StopSignallingServer();
	
private:
	// Keep track of which output providers are currently active
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UVCamPixelStreamingSession>> ActiveOutputProviders;

	// Signalling/webserver
	TSharedPtr<UE::PixelStreamingServers::IServer> Server;

	// An associated Live Link Source shared by all output providers
	TSharedPtr<FPixelStreamingLiveLinkSource> LiveLinkSource;
};
