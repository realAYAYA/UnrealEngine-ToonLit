// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rigs/RigHierarchyPose.h"

#include "ControlRigTestData.generated.h"

class UControlRigTestData;
class UControlRig;

USTRUCT(BlueprintType)
struct CONTROLRIG_API FControlRigTestDataVariable
{
	GENERATED_USTRUCT_BODY()

	FControlRigTestDataVariable()
	{
		Name = NAME_None;
		CPPType = NAME_None;
		Value = FString();
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestDataVariable")
	FName Name;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestDataVariable")
	FName CPPType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestDataVariable")
	FString Value;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FControlRigTestDataFrame
{
	GENERATED_USTRUCT_BODY()

	FControlRigTestDataFrame()
	{
		AbsoluteTime = 0.0;
		DeltaTime = 0.0;
		Variables.Reset();
		Pose.Reset();
	}

	bool Store(UControlRig* InControlRig, bool bInitial = false);
	bool Restore(UControlRig* InControlRig, bool bInitial = false) const;
	bool RestoreVariables(UControlRig* InControlRig) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestDataFrame")
	double AbsoluteTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestDataFrame")
	double DeltaTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestDataFrame")
	TArray<FControlRigTestDataVariable> Variables;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestDataFrame")
	FRigPose Pose;
};

UENUM()
enum class EControlRigTestDataPlaybackMode : uint8
{
	Live,
	ReplayInputs,
	GroundTruth,
	Max UMETA(Hidden),
};

UCLASS(BlueprintType)
class CONTROLRIG_API UControlRigTestData : public UObject
{
	GENERATED_BODY()

public:

	UControlRigTestData()
		: Tolerance(0.001)
		, LastFrameIndex(INDEX_NONE)
		, DesiredRecordingDuration(0.0)
		, TimeAtStartOfRecording(0.0)
		, bIsApplyingOutputs(false)
	{}

	virtual void Serialize(FArchive& Ar) override;

	UFUNCTION(BlueprintCallable, Category = "ControlRigTestData")
	static UControlRigTestData* CreateNewAsset(FString InDesiredPackagePath, FString InBlueprintPathName);

	UFUNCTION(BlueprintPure, Category = "ControlRigTestData")
	FVector2D GetTimeRange(bool bInput = false) const;

	UFUNCTION(BlueprintPure, Category = "ControlRigTestData")
	int32 GetFrameIndexForTime(double InSeconds, bool bInput = false) const;

	UFUNCTION(BlueprintCallable, Category = "ControlRigTestData")
	bool Record(UControlRig* InControlRig, double InRecordingDuration = 0.0);

	UFUNCTION(BlueprintCallable, Category = "ControlRigTestData")
	bool SetupReplay(UControlRig* InControlRig, bool bGroundTruth = true);

	UFUNCTION(BlueprintCallable, Category = "ControlRigTestData")
	void ReleaseReplay();

	UFUNCTION(BlueprintPure, Category = "ControlRigTestData")
	EControlRigTestDataPlaybackMode GetPlaybackMode() const;

	UFUNCTION(BlueprintPure, Category = "ControlRigTestData")
	bool IsReplaying() const;

	UFUNCTION(BlueprintPure, Category = "ControlRigTestData")
	bool IsRecording() const { return DesiredRecordingDuration >= SMALL_NUMBER; }

	UPROPERTY(AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestData")
	FSoftObjectPath ControlRigObjectPath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestData")
	FControlRigTestDataFrame Initial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestData")
	TArray<FControlRigTestDataFrame> InputFrames;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ControlRigTestData")
	TArray<FControlRigTestDataFrame> OutputFrames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ControlRigTestData")
	TArray<int32> FramesToSkip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ControlRigTestData")
	double Tolerance;

private:

	void ClearDelegates(UControlRig* InControlRig);

	mutable int32 LastFrameIndex;
	double DesiredRecordingDuration;
	double TimeAtStartOfRecording;
	TWeakObjectPtr<UControlRig> ReplayControlRig;
	bool bIsApplyingOutputs;
	FDelegateHandle PreConstructionHandle;
	FDelegateHandle PreForwardHandle;
	FDelegateHandle PostForwardHandle;
};