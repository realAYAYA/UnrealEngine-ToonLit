// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ARTypes.h"
#include "ARSessionConfig.h"
#include "ARTextures.h"
#include "Engine/Engine.h" // for FWorldContext

class IARSystemSupport;

DECLARE_MULTICAST_DELEGATE(FARSystemOnSessionStarted);
DECLARE_MULTICAST_DELEGATE_OneParam(FARSystemOnAlignmentTransformUpdated, const FTransform&);

#define DECLARE_AR_SI_DELEGATE_FUNCS(DelegateName) \
public: \
	FDelegateHandle Add##DelegateName##Delegate_Handle(const F##DelegateName##Delegate& Delegate); \
	void Clear##DelegateName##Delegate_Handle(FDelegateHandle& Handle); \
	void Clear##DelegateName##Delegates(void* Object);

/**
 * Composition Components for tracking system features
*/

class AUGMENTEDREALITY_API FARSupportInterface : public TSharedFromThis<FARSupportInterface, ESPMode::ThreadSafe>, public FGCObject, public IModularFeature
{
public:
	FARSupportInterface (IARSystemSupport* InARImplementation, IXRTrackingSystem* InXRTrackingSystem);
	virtual ~FARSupportInterface ();


	static FName GetModularFeatureName()
	{
		static const FName ModularFeatureName = FName(TEXT("ARSystem"));
		return ModularFeatureName;
	}

	void InitializeARSystem();
	IXRTrackingSystem* GetXRTrackingSystem();

	bool StartARGameFrame(FWorldContext& WorldContext);

	const FTransform& GetAlignmentTransform() const;
	const UARSessionConfig& GetSessionConfig() const;
	UARSessionConfig& AccessSessionConfig();

	/** \see UARBlueprintLibrary::GetTrackingQuality() */
	EARTrackingQuality GetTrackingQuality() const;
	/** \see UARBlueprintLibrary::GetTrackingQualityReason() */
	EARTrackingQualityReason GetTrackingQualityReason() const;
	/** \see UARBlueprintLibrary::StartARSession() */
	void StartARSession(UARSessionConfig* InSessionConfig);
	/** \see UARBlueprintLibrary::PauseARSession() */
	void PauseARSession();
	/** \see UARBlueprintLibrary::StopARSession() */
	void StopARSession();
	/** \see UARBlueprintLibrary::GetARSessionStatus() */
	FARSessionStatus GetARSessionStatus() const;
	/** \see UARBlueprintLibrary::IsSessionTypeSupported() */
	bool IsSessionTypeSupported(EARSessionType SessionType) const;

	/** \see UARBlueprintLibrary::ToggleARCapture() */
	bool ToggleARCapture(const bool bOnOff, const EARCaptureType CaptureType);

	/** \see UARBlueprintLibrary::ToggleARCapture() */
	void SetEnabledXRCamera(bool bOnOff);
	/** \see UARBlueprintLibrary::ToggleARCapture() */
	FIntPoint ResizeXRCamera(const FIntPoint& InSize);

	/**
	 * \see UARBlueprintLibrary::SetAlignmentTransform()
	 * \see IARSystemSupport
	 * To understand the various spaces involved in Augmented Reality system, \see IARSystemSupport.
	 */
	void SetAlignmentTransform(const FTransform& InAlignmentTransform);

	/** \see UARBlueprintLibrary::LineTraceTrackedObjects() */
	TArray<FARTraceResult> LineTraceTrackedObjects(const FVector2D ScreenCoords, EARLineTraceChannels TraceChannels);
	/** \see UARBlueprintLibrary::LineTraceTrackedObjects() */
	TArray<FARTraceResult> LineTraceTrackedObjects(const FVector Start, const FVector End, EARLineTraceChannels TraceChannels);
	/** \see UARBlueprintLibrary::GetAllTrackedGeometries() */
	TArray<UARTrackedGeometry*> GetAllTrackedGeometries() const;
	/** \see UARBlueprintLibrary::GetAllPins() */
	TArray<UARPin*> GetAllPins() const;
	/**\see UARBlueprintLibrary::IsEnvironmentCaptureSupported() */
	bool IsEnvironmentCaptureSupported() const;
	/**\see UARBlueprintLibrary::AddEnvironmentCaptureProbe() */
	bool AddManualEnvironmentCaptureProbe(FVector Location, FVector Extent);
	/** Creates an async task that will perform the work in the background */
	TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe> GetCandidateObject(FVector Location, FVector Extent) const;
	/** Creates an async task that will perform the work in the background */
	TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe> SaveWorld() const;
	/** @return the current mapping status */
	EARWorldMappingState GetWorldMappingStatus() const;

	/** \see UARBlueprintLibrary::GetCurrentLightEstimate() */
	UARLightEstimate* GetCurrentLightEstimate() const;

	/** \see UARBlueprintLibrary::PinComponent() */
	UARPin* PinComponent(USceneComponent* ComponentToPin, const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry = nullptr, const FName DebugName = NAME_None);
	/** \see UARBlueprintLibrary::PinComponentToTraceResult() */
	UARPin* PinComponent(USceneComponent* ComponentToPin, const FARTraceResult& HitResult, const FName DebugName = NAME_None);
	/** \see UARBlueprintLibrary::RemovePin() */
	void RemovePin(UARPin* PinToRemove);
	bool TryGetOrCreatePinForNativeResource(void* InNativeResource, const FString& InAnchorName, UARPin*& OutAnchor);

	/** \see UARBlueprintLibrary::GetSupportedVideoFormats() */
	TArray<FARVideoFormat> GetSupportedVideoFormats(EARSessionType SessionType = EARSessionType::World) const;

	/** @return the current point cloud data for the ar scene */
	TArray<FVector> GetPointCloud() const;

	/** \see UARBlueprintLibrary::AddRuntimeCandidateImage() */
	UARCandidateImage* AddRuntimeCandidateImage(UARSessionConfig* SessionConfig, UTexture2D* CandidateTexture, FString FriendlyName, float PhysicalWidth);

	void* GetARSessionRawPointer();
	void* GetGameThreadARFrameRawPointer();
	
	/** \see UARBlueprintLibrary::IsSessionTrackingFeatureSupported() */
	bool IsSessionTrackingFeatureSupported(EARSessionType SessionType, EARSessionTrackingFeature SessionTrackingFeature) const;
	
	/** \see UARBlueprintLibrary::GetTracked2DPose() */
	TArray<FARPose2D> GetTracked2DPose() const;
	
	/** \see UARBlueprintLibrary::IsSceneReconstructionSupported() */
	bool IsSceneReconstructionSupported(EARSessionType SessionType, EARSceneReconstruction SceneReconstructionMethod) const;
	
	/** \see UARBlueprintLibrary::AddTrackedPointWithName() */
	bool AddTrackedPointWithName(const FTransform& WorldTransform, const FString& PointName, bool bDeletePointsWithSameName);
	
	/** \see UARBlueprintLibrary::GetNumberOfTrackedFacesSupported() */
	int32 GetNumberOfTrackedFacesSupported() const;
	
	/** \see UARBlueprintLibrary::GetARTexture() */
	UARTexture* GetARTexture(EARTextureType TextureType) const;
	
	/** \see UARBlueprintLibrary::GetCameraIntrinsics() */
	bool GetCameraIntrinsics(FARCameraIntrinsics& OutCameraIntrinsics) const;
	
	/** \see UARBlueprintLibrary::IsARSupported() */
	bool IsARAvailable() const;

	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FARSupportInterface");
	}
	//~ FGCObject

	// Pass through helpers to create the methods needed to add/remove delegates from the AR system
	DECLARE_AR_SI_DELEGATE_FUNCS(OnTrackableAdded)
	DECLARE_AR_SI_DELEGATE_FUNCS(OnTrackableUpdated)
	DECLARE_AR_SI_DELEGATE_FUNCS(OnTrackableRemoved)
	// End helpers

	/** Pin Interface */
	bool PinComponent(USceneComponent* ComponentToPin, UARPin* Pin);
	bool IsLocalPinSaveSupported() const;
	bool ArePinsReadyToLoad();
	void LoadARPins(TMap<FName, UARPin*>& LoadedPins);
	bool SaveARPin(FName InName, UARPin* InPin);
	void RemoveSavedARPin(FName InName);
	void RemoveAllSavedARPins();


	FARSystemOnSessionStarted OnARSessionStarted;
	FARSystemOnAlignmentTransformUpdated OnAlignmentTransformUpdated;

private:
	IARSystemSupport* ARImplemention;
	IXRTrackingSystem* XRTrackingSystem;

	/** Alignment transform between AR System's tracking space and Unreal's World Space. Useful in static lighting/geometry scenarios. */
	FTransform AlignmentTransform;
	TObjectPtr<UARSessionConfig> ARSettings;
};

