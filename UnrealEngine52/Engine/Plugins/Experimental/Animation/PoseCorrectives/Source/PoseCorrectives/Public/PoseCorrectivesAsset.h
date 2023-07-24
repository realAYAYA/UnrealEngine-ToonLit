// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class of pose correctives asset
 *
 */

#include "RBF/RBFSolver.h"

#include "PoseCorrectivesAsset.generated.h"


class USkeletalMesh;


USTRUCT()
struct FPoseCorrective
{
	GENERATED_USTRUCT_BODY()

	// Driver data

	// Local space pose
	UPROPERTY()
	TArray<FTransform> PoseLocal;

	// Indices of bones that this particular corrective is being driven by
	UPROPERTY()
	TSet<int32> DriverBoneIndices;

	// Curve data
	UPROPERTY()
	TArray<float> CurveData;

	// Indices of driver curves that this particular corrective is using
	UPROPERTY()
	TSet<int32> DriverCurveIndices;

		
	// Corrective data

	// Corrective transform 
	UPROPERTY()
	TArray<FTransform> CorrectivePoseLocal;

	// Indices of bones that this particular corrective will apply to
	UPROPERTY()
	TSet<int32> CorrectiveBoneIndices;

	// Delta to CurveData
	UPROPERTY()
	TArray<float> CorrectiveCurvesDelta;

	// Indices of curves that this particular corrective will apply to
	UPROPERTY()
	TSet<int32> CorrectiveCurveIndices;

#if WITH_EDITORONLY_DATA
	// Group used to set indices
	UPROPERTY()
	FName GroupName;
#endif
};


// Predefines what bones/curves are used by the pose in the rbf/algorithm
USTRUCT()
struct POSECORRECTIVES_API FPoseGroupDefinition
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FName> DriverBones;

	UPROPERTY()
	TArray<FName> DriverCurves;

	UPROPERTY()
	TArray<FName> CorrectiveBones;

	UPROPERTY()
	TArray<FName> CorrectiveCurves;
};


/*
 * Pose Correctives Asset
 */
UCLASS(BlueprintType)
class POSECORRECTIVES_API UPoseCorrectivesAsset : public UObject
{
	GENERATED_UCLASS_BODY()

private:
	/** Animation Pose Data*/
	UPROPERTY()
	TMap<FName, FPoseCorrective> PoseCorrectives;

#if WITH_EDITORONLY_DATA
	// Predefined groups stored here in editor mode for configuring groups in UI and applying to pose but runtime driver/corrective bones live with pose
	UPROPERTY()
	TMap<FName, FPoseGroupDefinition> GroupDefinitions;
#endif // WITH_EDITORONLY_DATA

public:
	UPROPERTY(EditAnywhere, Category = Rigs)
	TObjectPtr<USkeletalMesh> TargetMesh = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Rigs)
	TObjectPtr<USkeletalMesh> SourcePreviewMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = Rigs)
	TObjectPtr<UObject> ControlRigBlueprint = nullptr;

	UPROPERTY(EditAnywhere, Category = Rigs)
	FVector SourceMeshOffset = {-100, 0, 0};

	UPROPERTY(EditAnywhere, Category = Rigs, meta = (UIMin = "0.01", UIMax = "10.0"))
	float SourceMeshScale = 1.0f;
#endif

	// Default rbf params
	UPROPERTY(EditAnywhere, Category = Solve)
	FRBFParams RBFParams;

	TArray<FName> GetBoneNames() const;
	int32 GetBoneIndex(const FName& BoneName) const; 
	TArray<FName> GetCurveNames() const;
	
	const FRBFParams& GetRBFParams() const { return RBFParams; };
	TArray<FPoseCorrective> GetCorrectives() const;
	
#if WITH_EDITOR
	bool AddGroup(const FName& Name);
	void RemoveGroup(const FName& Name);
	void AddCorrective(USkeletalMeshComponent* SourceMeshComponent, USkeletalMeshComponent* TargetMeshComponent, const FName& CorrectiveName);
	bool DeleteCorrective(const FName& CorrectiveName);
	bool RenameCorrective(const FName& CurrentName, const FName& NewName);
	void UpdateGroupForCorrective(const FName& CorrectiveName, const FName& GroupName);
	void UpdateGroupForCorrective(FPoseCorrective* PoseCorrective, const FPoseGroupDefinition* GroupDefinition);
	void UpdateCorrectivesByGroup(const FName& GroupName);

	FPoseCorrective* FindCorrective(const FName& CorrectiveName);
	FPoseGroupDefinition* FindGroupDefinition(const FName& GroupName);

	TArray<FName> GetCorrectiveNames() const;
	TArray<FName> GetGroupNames() const;
	

private: 
	DECLARE_MULTICAST_DELEGATE(FOnCorrectivesListChangedMulticaster)
	FOnCorrectivesListChangedMulticaster OnCorrectivesListChanged;

public:
	typedef FOnCorrectivesListChangedMulticaster::FDelegate FOnCorrectivesListChanged;

	FDelegateHandle RegisterOnCorrectivesListChanged(const FOnCorrectivesListChanged& Delegate)
	{
		return OnCorrectivesListChanged.Add(Delegate);
	}
	void UnregisterOnCorrectivesListChanged(FDelegateHandle Handle)
	{
		OnCorrectivesListChanged.Remove(Handle);
	}
#endif

	friend class SPoseCorrectivesGroupsListRow;
	friend class SPoseCorrectivesGroups;
};
