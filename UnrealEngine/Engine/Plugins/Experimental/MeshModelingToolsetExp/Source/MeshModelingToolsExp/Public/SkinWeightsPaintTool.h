// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMeshBrushTool.h"
#include "BaseTools/MeshSurfacePointMeshEditingTool.h"
#include "Changes/ValueWatcher.h"
#include "MeshDescription.h"
#include "DynamicMesh/DynamicVerticesOctree3.h"
#include "BoneContainer.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "Misc/Optional.h"
#include "Containers/Map.h"
#include "TargetInterfaces/MeshDescriptionCommitter.h"

#include "SkinWeightsPaintTool.generated.h"


struct FMeshDescription;
class USkinWeightsPaintTool;
class IMeshDescriptionCommitter;



class MESHMODELINGTOOLSEXP_API FMeshSkinWeightsChange : public FToolCommandChange
{
public:
	FMeshSkinWeightsChange(const FName& InBoneName) :
		FToolCommandChange(),
		BoneName(InBoneName)
	{ }

	virtual FString ToString() const override
	{
		return FString(TEXT("Paint Skin Weights"));
	}

	void Apply(UObject* Object) override;

	void Revert(UObject* Object) override;

	void UpdateValues(const TArray<int32>& Indices, const TArray<float>& OldValues, const TArray<float>& NewValues);

private:
	FName BoneName;
	TMap<int32, float> OldWeights;
	TMap<int32, float> NewWeights;
};


/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API USkinWeightsPaintToolBuilder : public UMeshSurfacePointMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};


UCLASS()
class MESHMODELINGTOOLSEXP_API USkinWeightsPaintToolProperties : 
	public UInteractiveToolPropertySet,
	public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category = Skeleton)
	FBoneReference CurrentBone;

	// IBoneReferenceSkeletonProvider
	USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;

	TObjectPtr<USkeletalMesh> SkeletalMesh;
};


/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API USkinWeightsPaintTool : public UDynamicMeshBrushTool
{
	GENERATED_BODY()

public:
	void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	void Setup() override;
	void Render(IToolsContextRenderAPI* RenderAPI) override;

	bool HasCancel() const override { return true; }
	bool HasAccept() const override { return true; }
	bool CanAccept() const override { return true; }

	// UBaseBrushTool overrides
	bool HitTest(const FRay& Ray, FHitResult& OutHit) override;
	void OnBeginDrag(const FRay& Ray) override;
	void OnUpdateDrag(const FRay& Ray) override;
	void OnEndDrag(const FRay& Ray) override;
	bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;

protected:

	virtual void ApplyStamp(const FBrushStampData& Stamp);

	void OnShutdown(EToolShutdownType ShutdownType) override;
	void OnTick(float DeltaTime) override;

	double CalculateBrushFalloff(double Distance) const;
	void CalculateVertexROI(const FBrushStampData& Stamp, TArray<int>& VertexROI);

	UPROPERTY()
	TObjectPtr<USkinWeightsPaintToolProperties> ToolProps;

	TValueWatcher<FBoneReference> CurrentBoneWatcher;

	bool bInRemoveStroke = false;
	bool bInSmoothStroke = false;
	FBrushStampData StartStamp;
	FBrushStampData LastStamp;
	bool bStampPending;

	TUniquePtr<FMeshDescription> EditedMesh;

	FBoneContainer BoneContainer;

	UE::Geometry::TDynamicVerticesOctree3<FDynamicMesh3> VerticesOctree;
	TArray<int> PreviewBrushROI;

	using BoneInfluenceMapType = TMap<FName, TArray<float>>;
	BoneInfluenceMapType SkinWeightsMap;
	FName CurrentBone = NAME_None;
	TOptional<FName> PendingCurrentBone;


	bool bVisibleWeightsValid = false;

	static FVector4f WeightToColor(float Value);
	
	void InitializeSkinWeights();
	void UpdateBoneVisualization();
	void UpdateCurrentBone(const FName &BoneName);

	struct FBonePositionInfo
	{
		FName BoneName;
		int32 ParentBoneIndex;
		FVector Position;
		float Radius;
		TMap<FName, int32> ChildBones;
	};
	TArray<FBonePositionInfo> BonePositionInfos;
	float MaxDrawRadius = 0.0f;

	void UpdateBonePositionInfos(float MinRadius);
	void RenderBonePositions(FPrimitiveDrawInterface *PDI);

	TUniquePtr<FMeshSkinWeightsChange> ActiveChange;

	void BeginChange();
	TUniquePtr<FMeshSkinWeightsChange> EndChange();

	friend class FMeshSkinWeightsChange;
	void ExternalUpdateValues(const FName &BoneName, const TMap<int32, float>& IndexValues);

	void UpdateEditedSkinWeightsMesh();
};
