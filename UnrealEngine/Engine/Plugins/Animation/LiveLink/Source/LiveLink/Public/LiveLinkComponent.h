// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkAnimationBlueprintStructs.h"
#include "LiveLinkComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLiveLinkTickSignature, float, DeltaTime);

// An actor component to enable accessing LiveLink data in Blueprints. 
// Data can be accessed in Editor through the "OnLiveLinkUpdated" event.
// Any Skeletal Mesh Components on the parent will be set to animate in editor causing their AnimBPs to run.
UCLASS( ClassGroup=(LiveLink), meta=(BlueprintSpawnableComponent), meta = (DisplayName = "LiveLink Skeletal Animation"))
class LIVELINK_API ULiveLinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULiveLinkComponent();

protected:
	virtual void OnRegister() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// This Event is triggered any time new LiveLink data is available, including in the editor
	UPROPERTY(BlueprintAssignable, Category = "LiveLink")
	FLiveLinkTickSignature OnLiveLinkUpdated;

	// Returns a list of available Subject Names for LiveLink
	UE_DEPRECATED(4.23, "GetAvailableSubjectNames is deprecated, use GetLiveLinkEnabledSubjectNames.")
	UFUNCTION(BlueprintCallable, Category = "LiveLink", meta=(DeprecatedFunction, DeprecationMessage="GetAvailableSubjectNames is deprecated, use GetLiveLinkEnabledSubjectNames."))
	void GetAvailableSubjectNames(TArray<FName>& SubjectNames);
	
	// Returns a handle to the current frame of data in LiveLink for a given subject along with a boolean for whether a frame was found.
	// Returns a handle to an empty frame if no frame of data is found.
	UE_DEPRECATED(4.23, "GetSubjectData is deprecated, use EvaluateLiveLinkFrame.")
	UFUNCTION(BlueprintCallable, Category = "LiveLink", meta=(DeprecatedFunction, DeprecationMessage="GetSubjectData is deprecated, EvaluateLiveLinkFrame."))
	void GetSubjectData(const FName SubjectName, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle);

	// Returns a handle to the current frame of data in LiveLink for a given subject along with a boolean for whether a frame was found.
	// Returns a handle to an empty frame if no frame of data is found.
	UE_DEPRECATED(4.23, "GetSubjectDataAtWorldTime is deprecated, use EvaluateLiveLinkFrameAtWorldTime.")
	UFUNCTION(BlueprintCallable, Category = "LiveLink", meta=(DeprecatedFunction, DeprecationMessage="GetSubjectDataAtWorldTime is deprecated, use EvaluateLiveLinkFrameAtWorldTime."))
	void GetSubjectDataAtWorldTime(const FName SubjectName, const float WorldTime, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle);
	
	UE_DEPRECATED(4.23, "GetSubjectDataAtTime is deprecated, use EvaluateLiveLinkFrameAtSceneTime.")
	void GetSubjectDataAtTime(const FName SubjectName, const double WorldTime, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle);

	// Returns a handle to the frame of data in LiveLink for a given subject at the specified time along with a boolean for whether a frame was found.
	// Returns a handle to an empty frame if no frame of data is found.
	UE_DEPRECATED(4.23, "GetSubjectDataAtSceneTime is deprecated, use EvaluateLiveLinkFrameAtSceneTime.")
	UFUNCTION(BlueprintCallable, Category = "LiveLink", meta=(DeprecatedFunction, DeprecationMessage="GetSubjectDataAtSceneTime is deprecated, use EvaluateLiveLinkFrameAtSceneTime."))
	void GetSubjectDataAtSceneTime(const FName SubjectName, const FTimecode& SceneTime, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle);

private:
	bool HasLiveLinkClient();

	// Record whether we have been recently registered
	bool bIsDirty;

	ILiveLinkClient* LiveLinkClient;
};