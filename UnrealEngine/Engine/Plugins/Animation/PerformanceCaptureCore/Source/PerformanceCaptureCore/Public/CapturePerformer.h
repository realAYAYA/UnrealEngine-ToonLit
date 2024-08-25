// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/SkeletalMeshActor.h"
#include "LiveLinkTypes.h"
#include "LiveLinkInstance.h"
#include "CapturePerformer.generated.h"

class ULiveLinkInstance;
class UPerformerComponent;

UCLASS(Blueprintable, Category="Performance Capture")
class ACapturePerformer : public ASkeletalMeshActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACapturePerformer();

	/**
	* LiveLink Subject Name. Must have the Animation Role Type.
	*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Performance Capture")
	FLiveLinkSubjectName SubjectName;

	/**
	* Force all skeletal meshes to follow the pose of the Controlled Skeletal Mesh.
	*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Performance Capture")
	bool bForceAllMeshesToFollowLeader = true;

	/**
	* Evaluate LiveLink animation. Set to false to pause animation.
	*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Performance Capture")
	bool bEvaluateAnimation = true;

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
	* Get state of LiveLink data evaluation. True = animation is updated.
	* @return bool Is LiveLink data being evaluated.
	*/
	UFUNCTION(BlueprintPure, Category ="Performance Capture")
	bool GetEvaluateLiveLinkData();

	/**
	* Set the Skeletal Mesh Asset of the root Skeletal Mesh.
	* @param MocapMesh New Skeletal Mesh Asset.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture")
	void SetMocapMesh(USkeletalMesh* MocapMesh);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void PostRegisterAllComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
private:
	TObjectPtr<UPerformerComponent> PerformerComponent;
};