// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkTypes.h"
#include "Components/ActorComponent.h"

#include "PerformerComponent.generated.h"

class ULiveLinkInstance;

UCLASS(BlueprintType, DisplayName="Performer Component",  ClassGroup=("Performance Capture"), meta=(BlueprintSpawnableComponent))
class UPerformerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPerformerComponent();

	/**
	* LiveLink Subject Name. Must have the Animation Role Type.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Performance Capture")
	FLiveLinkSubjectName SubjectName;

	/**
	* The Skeletal Mesh driven by the LiveLink subject. Skeleton mush be compatible with the LiveLink subject's bone hierarchy. 
	*/
	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category = "Performance Capture", meta=(UseComponentPicker, AllowedClasses = "/Script/Engine.SkeletalMeshComponent"))
	FComponentReference ControlledSkeletalMesh;
	
	/**
	* Evaluate LiveLink animation. Set to false to pause animation.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Performance Capture")
	bool bEvaluateAnimation = true;
	
	/**
	* Force all other skeletal meshes in the Owner Actor to follow the pose of the Controlled Skeletal Mesh.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Performance Capture")
	bool bForceOtherMeshesToFollowControlledMesh = true;

	/**
	* Set the LiveLink Subject Name. Subject must have the Animation Role Type.
	* @param Subject New LiveLink Subject.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture")
	void SetLiveLinkSubject(FLiveLinkSubjectName Subject);
	
	/**
	* Get the LiveLink Subject Name.
	* @return FLiveLinkSubjectName Current LiveLink Subject.
	*/
	UFUNCTION(BlueprintCallable, Category ="Performance Capture")
	FLiveLinkSubjectName GetLiveLinkSubject() const;
	
	/**
	* Set the LiveLink data to update the Skeletal Mesh pose.
	* @param bEvaluateLinkLink Drive or pause the Skeletal Mesh from LiveLink Subject data.
	*/
	UFUNCTION(BlueprintCallable, Category ="Performance Capture")
	void SetEvaluateLiveLinkData(bool bEvaluateLinkLink);
	
	/**
	* Get the LiveLink Subject Name. Subject must have the Animation Role Type.
	* @return bool Is LiveLink data being evaluated.
	*/
	UFUNCTION(BlueprintPure, Category ="Performance Capture")
	bool GetEvaluateLiveLinkData();

protected:
	virtual void DestroyComponent(bool bPromoteChildren) override;

	virtual void OnRegister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void InitiateAnimation();

private:

	bool bIsDirty;
	
};
