// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamOutputProviderBase.h"
#include "ImageProviders/RemoteSessionMediaOutput.h"
#include "IRemoteSessionRole.h"
#include "RemoteSession.h"
#include "VCamOutputRemoteSession.generated.h"

UCLASS(meta = (DisplayName = "Unreal Remote Output Provider"))
class VCAMCORE_API UVCamOutputRemoteSession : public UVCamOutputProviderBase
{
	GENERATED_BODY()
public:
	
	/** Network port number - change this only if connecting multiple RemoteSession devices to the same PC */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	int32 PortNumber = IRemoteSessionModule::kDefaultPort;

	/** If using the output from a Composure Output Provider, specify it here */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	int32 FromComposureOutputProviderIndex = INDEX_NONE;

	UVCamOutputRemoteSession();

	//~ Begin UVCamOutputProviderBase Interface
	virtual void Initialize() override;
	virtual void Deinitialize() override;
	virtual void Activate() override;
	virtual void Deactivate() override;
	virtual void Tick(const float DeltaTime) override;
	//~ End UVCamOutputProviderBase Interface

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
#endif

protected:

	UPROPERTY(Transient)
	TObjectPtr<URemoteSessionMediaOutput> MediaOutput = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URemoteSessionMediaCapture> MediaCapture = nullptr;

	//~ Begin UVCamOutputProviderBase Interface
	virtual bool ShouldOverrideResolutionOnActivationEvents() const { return true; }
	//~ End UVCamOutputProviderBase Interface

private:
	
	TSharedPtr<IRemoteSessionUnmanagedRole> RemoteSessionHost;
	FHitResult LastViewportTouchResult;
	bool bUsingDummyUMG = false;

	void CreateRemoteSession();
	void DestroyRemoteSession();

	void OnRemoteSessionChannelChange(IRemoteSessionRole* Role, TWeakPtr<IRemoteSessionChannel> Channel, ERemoteSessionChannelChange Change);
	void OnImageChannelCreated(TWeakPtr<IRemoteSessionChannel> Instance);
	void OnInputChannelCreated(TWeakPtr<IRemoteSessionChannel> Instance);
	void OnTouchEventOutsideUMG(const FVector2D& InViewportPosition);

	bool DeprojectScreenToWorld(const FVector2D& InScreenPosition, FVector& OutWorldPosition, FVector& OutWorldDirection) const;
};
