// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Retargeter/IKRetargetProfile.h"

#include "RetargetComponent.generated.h"


class UIKRetargeter;

UCLASS(BlueprintType, ClassGroup=("Performance Capture"), meta=(BlueprintSpawnableComponent), DisplayName = "Retarget Component")
class URetargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URetargetComponent();

	/**
	* Skeletal Mesh component that will be the source for retargeting. Can be on the Owner Actor or another Actor in the same level.
	*/
	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category = "Performance Capture",  meta = (UseComponentPicker, AllowAnyActor, AllowedClasses = "/Script/Engine.SkeletalMeshComponent"))
	FComponentReference SourceSkeletalMeshComponent ;

	/**
	* Skeletal Mesh that will be driven by the IKRetargeter. Limited to skeletal meshes on this component's Owner Actor.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category = "Performance Capture", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SkeletalMeshComponent"))
	FComponentReference ControlledSkeletalMeshComponent;

	/**
	* Force all skeletal meshes to use the ControlledSkeletalMeshComponent as their Leader. Default = True.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Performance Capture")
	bool bForceOtherMeshesToFollowControlledMesh = true;

	/**
	* The IKRetarget Asset to use for retargeting between the source and controlled skeletal meshes.
	*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Performance Capture")
	TObjectPtr<UIKRetargeter> RetargetAsset;

	/**
	* Custom Retarget Profile. Should be used to override Retarget settings on the RetargetAsset.
	*/
	UPROPERTY(BlueprintReadWrite, Interp, EditAnywhere, Category = "Performance Capture")
	FRetargetProfile CustomRetargetProfile;
	
	/**
	* Set the Source Performer Mesh.
	* @param InPerformerMesh New Source Skeletal Mesh.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture")
	void SetSourcePerformerMesh(USkeletalMeshComponent* InPerformerMesh);
	
	/**
	* Set the Controlled Skeletal Mesh.
	* @param InControlledMesh New Controlled Skeletal Mesh Component.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture")
	void SetControlledMesh(USkeletalMeshComponent* InControlledMesh);

	/**
	* Set the Retarget Asset.
	* @param InRetargetAsset New IKRetarget Asset.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture")
	void SetRetargetAsset(UIKRetargeter* InRetargetAsset);

	/**
	* Set a Custom Retarget Profile.
	* @param InProfile New Retarget Profile.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture")
	void SetCustomRetargetProfile(FRetargetProfile InProfile);

	/**
	* Get Retarget Profile.
	* @return Current Custom Retarget Profile.
	*/
	UFUNCTION(BlueprintCallable, Category = "Performance Capture")
	FRetargetProfile GetCustomRetargetProfile();
	
protected:
	// Called when the game starts
	virtual void OnRegister() override; //This is the equivalent of OnConstruction in an actor
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void DestroyComponent(bool bPromoteChildren) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
private:
	bool bIsDirty = true;
public:
	
	void InitiateAnimation();

	void SetForceOtherMeshesToFollowControlledMesh(bool bInBool);
};

