// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Output/VCamOutputProviderBase.h"
#include "DecoupledOutputProvider.generated.h"

namespace UE::DecoupledOutputProvider
{
	class IOutputProviderLogic;
}

/**
 * A decoupled output provider only contains data and forwards all important events to an IOutputProviderLogic, which
 * may or may not exist. This allows the data to be loaded on all platforms but perform no operations on unsupported platforms.
 * This decoupling is important to avoid failing LoadPackage warnings during cooking.
 * 
 * Example: Pixel Streaming.
 */
UCLASS(Abstract, NotBlueprintable)
class DECOUPLEDOUTPUTPROVIDER_API UDecoupledOutputProvider : public UVCamOutputProviderBase
{
	GENERATED_BODY()
public:

	//~ Begin UVCamOutputProviderBase Interface
	virtual void Initialize() override;
	virtual void Deinitialize() override;
	virtual void Tick(const float DeltaTime) override;
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
	virtual UE::VCamCore::EViewportChangeReply PreReapplyViewport() override;
	virtual void PostReapplyViewport() override;
	//~ End UVCamOutputProviderBase Interface

	//~ Begin UObject Interface
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface
};
