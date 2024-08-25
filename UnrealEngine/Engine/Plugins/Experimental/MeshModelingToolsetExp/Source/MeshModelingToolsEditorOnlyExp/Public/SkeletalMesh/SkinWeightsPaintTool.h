// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMeshBrushTool.h"
#include "BaseTools/MeshSurfacePointMeshEditingTool.h"
#include "MeshDescription.h"
#include "DynamicMesh/DynamicVerticesOctree3.h"
#include "BoneWeights.h"
#include "GroupTopology.h"
#include "Misc/Optional.h"
#include "Containers/Map.h"
#include "DynamicMesh/DynamicMeshOctree3.h"
#include "SkeletalMesh/SkeletalMeshEditionInterface.h"
#include "Engine/SkeletalMesh.h"

#include "SkinWeightsPaintTool.generated.h"


struct FMeshDescription;
class USkinWeightsPaintTool;
class UPolygonSelectionMechanic;
class UPersonaEditorModeManagerContext;

namespace UE::Geometry 
{
	template <typename BoneIndexType, typename BoneWeightType> class TBoneWeightsDataSource;
	template <typename BoneIndexType, typename BoneWeightType> class TSmoothBoneWeights;
}

using BoneIndex = int32;
using VertexIndex = int32;

// weight edit mode
UENUM()
enum class EWeightEditMode : uint8
{
	Brush,
	Vertices,
	Bones,
};

// weight color mode
UENUM()
enum class EWeightColorMode : uint8
{
	MinMax,
	Ramp,
};

// brush falloff mode
UENUM()
enum class EWeightBrushFalloffMode : uint8
{
	Surface,
	Volume,
};

// operation type when editing weights
UENUM()
enum class EWeightEditOperation : uint8
{
	Add,
	Replace,
	Multiply,
	Relax
};

// mirror direction mode
UENUM()
enum class EMirrorDirection : uint8
{
	PositiveToNegative,
	NegativeToPositive,
};

namespace SkinPaintTool
{
	struct FSkinToolWeights;

	struct FVertexBoneWeight
	{
		FVertexBoneWeight() : BoneIndex(INDEX_NONE), VertexInBoneSpace(FVector::ZeroVector), Weight(0.0f) {}
		FVertexBoneWeight(int32 InBoneIndex, const FVector& InPosInRefPose, float InWeight) :
			BoneIndex(InBoneIndex), VertexInBoneSpace(InPosInRefPose), Weight(InWeight){}
		
		int32 BoneIndex;
		FVector VertexInBoneSpace;
		float Weight;
	};

	using VertexWeights = TArray<FVertexBoneWeight, TFixedAllocator<UE::AnimationCore::MaxInlineBoneWeightCount>>;

	// data required to preview the skinning deformations as you paint
	struct FSkinToolDeformer
	{
		void Initialize(const USkeletalMeshComponent* SkeletalMeshComponent, const FMeshDescription* Mesh);

		void SetAllVerticesToBeUpdated();

		void UpdateVertexDeformation(USkinWeightsPaintTool* Tool);

		void SetVertexNeedsUpdated(int32 VertexIndex);
		
		// which vertices require updating (partially re-calculated skinning deformation while painting)
		TSet<int32> VerticesWithModifiedWeights;
		// position of all vertices in the reference pose
		TArray<FVector> RefPoseVertexPositions;
		// inverted, component space ref pose transform of each bone
		TArray<FTransform> InvCSRefPoseTransforms;
		// bones transforms used in last deformation update
		TArray<FTransform> PrevBoneTransforms;
		// bone index to bone name
		TArray<FName> BoneNames;
		TMap<FName, BoneIndex> BoneNameToIndexMap;
		// the skeletal mesh to get the current pose from
		const USkeletalMeshComponent* Component;
	};

	// store a sparse set of modifications to a set of vertex weights on a SINGLE bone
	struct FSingleBoneWeightEdits
	{
		int32 BoneIndex;
		TMap<VertexIndex, float> OldWeights;
		TMap<VertexIndex, float> NewWeights;
	};

	// store a sparse set of modifications to a set of vertex weights for a SET of bones
	// with support for merging edits. these are used for transaction history undo/redo.
	struct FMultiBoneWeightEdits
	{
		void MergeSingleEdit(const int32 BoneIndex, const int32 VertexID, const float OldWeight, const float NewWeight);
		void MergeEdits(const FSingleBoneWeightEdits& BoneWeightEdits);
		float GetVertexDeltaFromEdits(const int32 BoneIndex, const int32 VertexIndex);

		// map of bone indices to weight edits made to that bone
		TMap<BoneIndex, FSingleBoneWeightEdits> PerBoneWeightEdits;
	};

	class FMeshSkinWeightsChange : public FToolCommandChange
	{
	public:
		FMeshSkinWeightsChange() : FToolCommandChange()	{ }

		virtual FString ToString() const override
		{
			return FString(TEXT("Paint Skin Weights"));
		}

		virtual void Apply(UObject* Object) override;

		virtual void Revert(UObject* Object) override;

		void AddBoneWeightEdit(const FSingleBoneWeightEdits& BoneWeightEdit);

	private:
		FMultiBoneWeightEdits AllWeightEdits;
	};

	// intermediate storage of the weight maps for duration of tool
	struct FSkinToolWeights
	{
		// copy the initial weight values from the skeletal mesh
		void InitializeSkinWeights(
			const USkeletalMeshComponent* SkeletalMeshComponent,
			FMeshDescription* Mesh);

		// applies an edit to a single vertex weight on a single bone, then normalizes the remaining weights while
		// keeping the edited weight intact (ie, adapts OTHER influences to achieve normalization)
		void EditVertexWeightAndNormalize(
			const int32 BoneIndex,
			const int32 VertexId,
			float NewWeightValue,
			FMultiBoneWeightEdits& WeightEdits);

		void ApplyCurrentWeightsToMeshDescription(FMeshDescription* EditedMesh);
		
		static float GetWeightOfBoneOnVertex(
			const int32 BoneIndex,
			const int32 VertexID,
			const TArray<VertexWeights>& InVertexWeights);

		void SetWeightOfBoneOnVertex(
			const int32 BoneIndex,
			const int32 VertexID,
			const float Weight,
			TArray<VertexWeights>& InOutVertexData);

		void SwapAfterChange();

		float SetCurrentFalloffAndGetMaxFalloffThisStroke(int32 VertexID, float CurrentStrength);

		void ApplyEditsToCurrentWeights(const FMultiBoneWeightEdits& Edits);

		void UpdateIsBoneWeighted(BoneIndex BoneToUpdate);
		
		// double-buffer of the entire weight matrix (stored sparsely for fast deformation)
		// "Pre" is state of weights at stroke start
		// "Current" is state of weights during stroke
		// When stroke is over, PreChangeWeights are synchronized with CurrentWeights
		TArray<VertexWeights> PreChangeWeights;
		TArray<VertexWeights> CurrentWeights;

		// record the current maximum amount of falloff applied to each vertex during the current stroke
		// values range from 0-1, this allows brushes to sweep over the same vertex, and apply only the maximum amount
		// of modification (add/replace/relax etc) that was encountered for the duration of the stroke.
		TArray<float> MaxFalloffPerVertexThisStroke;

		// record which bones have any weight assigned to them
		TArray<bool> IsBoneWeighted;

		// update deformation when vertex weights are modified
		FSkinToolDeformer Deformer;
	};

	struct FSkinMirrorData
	{
		void RegenerateMirrorData(
			const TArray<FName>& BoneNames,
			const TMap<FName, BoneIndex>& BoneNameToIndexMap,
			const FReferenceSkeleton& RefSkeleton,
			const TArray<FVector>& RefPoseVertices,
			EAxis::Type InMirrorAxis,
			EMirrorDirection InMirrorDirection);

		const TMap<int32, int32>& GetBoneMap() const { return BoneMap; };
		const TMap<int32, int32>& GetVertexMap() const { return VertexMap; };
		bool GetAllVerticesMirrored() const {return bAllVerticesMirrored; };
		
	private:
		
		bool bIsInitialized = false;
		bool bAllVerticesMirrored = false;
		TEnumAsByte<EAxis::Type> Axis;
		EMirrorDirection Direction; 
		TMap<int32, int32> BoneMap;
		TMap<int32, int32> VertexMap; // <Target, Source>
	};
}

UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USkinWeightsPaintToolBuilder : public UMeshSurfacePointMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};

// for saveing/restoring the brush settings separately for each brush mode (Add, Replace, etc...)
USTRUCT()
struct FSkinWeightBrushConfig
{
	GENERATED_BODY()
	
	UPROPERTY()
	float Strength = 1.f;

	UPROPERTY()
	float Radius = 20.0f;
	
	UPROPERTY()
	float Falloff = 1.0f;

	UPROPERTY()
	EWeightBrushFalloffMode FalloffMode = EWeightBrushFalloffMode::Surface;
};

// Container for properties displayed in Details panel while using USkinWeightsPaintTool
UCLASS(config = EditorSettings)
class MESHMODELINGTOOLSEDITORONLYEXP_API USkinWeightsPaintToolProperties : public UBrushBaseProperties
{
	GENERATED_BODY()

	USkinWeightsPaintToolProperties();
	
public:

	// brush vs selection modes
	UPROPERTY()
	EWeightEditMode EditingMode;

	// custom brush modes and falloff types
	UPROPERTY()
	EWeightEditOperation BrushMode;

	// weight color properties
	UPROPERTY(EditAnywhere, Category = WeightColors)
	EWeightColorMode ColorMode;
	UPROPERTY(EditAnywhere, Category = WeightColors)
	TArray<FLinearColor> ColorRamp;
	UPROPERTY(EditAnywhere, Category = WeightColors)
	FLinearColor MinColor;
	UPROPERTY(EditAnywhere, Category = WeightColors)
	FLinearColor MaxColor;
	bool bColorModeChanged = false;

	// weight editing arguments
	UPROPERTY(Config)
	TEnumAsByte<EAxis::Type> MirrorAxis = EAxis::X;
	UPROPERTY(Config)
	EMirrorDirection MirrorDirection = EMirrorDirection::PositiveToNegative;
	UPROPERTY(Config)
	float FloodValue = 1.f;
	UPROPERTY(Config)
	float PruneValue = 0.01;

	// save/restore user specified settings for each tool mode
	FSkinWeightBrushConfig& GetBrushConfig();
	TMap<EWeightEditOperation, FSkinWeightBrushConfig*> BrushConfigs;
	UPROPERTY(Config)
	FSkinWeightBrushConfig BrushConfigAdd;
	UPROPERTY(Config)
	FSkinWeightBrushConfig BrushConfigReplace;
	UPROPERTY(Config)
	FSkinWeightBrushConfig BrushConfigMultiply;
	UPROPERTY(Config)
	FSkinWeightBrushConfig BrushConfigRelax;

	// pointer back to paint tool
	TObjectPtr<USkinWeightsPaintTool> WeightTool;
};

// An interactive tool for painting and editing skin weights.
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USkinWeightsPaintTool : public UDynamicMeshBrushTool, public ISkeletalMeshEditingInterface
{
	GENERATED_BODY()

public:

	// UBaseBrushTool overrides
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override { return true; }
	virtual bool HitTest(const FRay& Ray, FHitResult& OutHit) override;
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;

	void Init(const FToolBuilderState& InSceneState);
	
	// UInteractiveTool
	virtual void Setup() override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	// IInteractiveToolCameraFocusAPI
	virtual bool SupportsWorldSpaceFocusBox() override { return true; }
	virtual FBox GetWorldSpaceFocusBox() override;

	// using when ToolChange is applied via Undo/Redo
	void ExternalUpdateWeights(const int32 BoneIndex, const TMap<int32, float>& IndexValues);

	// weight editing operations (selection based)
	void MirrorWeights(EAxis::Type Axis, EMirrorDirection Direction);
	void FloodWeights(const float Weight, const EWeightEditOperation FloodMode);
	void PruneWeights(const float Threshold);
	void AverageWeights();
	void NormalizeWeights();
	
	// method to set weights directly (numeric input, for example)
	void SetBoneWeightOnVertices(
		BoneIndex Bone,
		const float Weight,
		const TArray<VertexIndex>& VerticesToEdit,
		const bool bShouldTransact);

	// toggle brush / selection mode
	void ToggleEditingMode();

	// edit selection
	TObjectPtr<UPolygonSelectionMechanic> GetSelectionMechanic();

	// get a list of currently selected vertices
	void GetSelectedVertices(TArray<int32>& OutVertexIndices) const;

	// get the average weight value of each influence on the given vertices
	void GetInfluences(const TArray<int32>& VertexIndices, TArray<BoneIndex>& OutBoneIndices);

	// get the average weight value of a single bone on the given vertices
	float GetAverageWeightOnBone(const BoneIndex InBoneIndex, const TArray<int32>& VertexIndices);

	// convert an index to a name
	FName GetBoneNameFromIndex(BoneIndex InIndex) const;

	// HOW TO EDIT WEIGHTS WITH UNDO/REDO:
	//
	// Live Edits:
	// For multiple weight editing operations that need to be grouped into a single transaction, like dragging a slider or
	// dragging a brush, you must call BeginChange(), then ApplyWeightsEditsToMesh(bShouldTransact=false), then EndChange().
	// All the edits are stored into the "ActiveChange" and applied as a single transaction. Deformations and vertex
	// colors will be updated throughout the duration of the change.
	//
	// Single Edits:
	// For all one-and-done edits, you can call ApplyWeightEditsToMesh(bShouldTransact=True).
	// It will Begin/End the change and create a transaction for it.
	//
	void BeginChange();
	void EndChange(const FText& TransactionLabel);
	void ApplyWeightEditsToMesh(
		const SkinPaintTool::FMultiBoneWeightEdits& WeightEdits,
		const FText& TransactionLabel,
		const bool bShouldTransact);

	// called whenever the selection is modified
	DECLARE_MULTICAST_DELEGATE(FOnSelectionChanged);
	FOnSelectionChanged OnSelectionChanged;

	// called whenever the weights are modified
	DECLARE_MULTICAST_DELEGATE(FOnWeightsChanged);
	FOnWeightsChanged OnWeightsChanged;

protected:

	virtual void ApplyStamp(const FBrushStampData& Stamp);
	void OnShutdown(EToolShutdownType ShutdownType) override;
	void OnTick(float DeltaTime) override;

	// stamp
	float CalculateBrushFalloff(float Distance) const;
	void CalculateVertexROI(
		const FBrushStampData& Stamp,
		TArray<VertexIndex>& VertexIDs,
		TArray<float>& VertexFalloffs);
	float CalculateBrushStrengthToUse(EWeightEditOperation EditMode) const;
	bool bInvertStroke = false;
	bool bSmoothStroke = false;
	FBrushStampData StartStamp;
	FBrushStampData LastStamp;
	bool bStampPending;
	int32 TriangleUnderStamp;
	FVector StampLocalPos;

	// modify vertex weights according to the specified operation,
	// generating bone weight edits to be stored in a transaction
	void EditWeightOfBoneOnVertices(
		EWeightEditOperation EditOperation,
		const BoneIndex Bone,
		const TArray<int32>& VerticesToEdit,
		const TArray<float>& VertexFalloffs,
		const float UseStrength,
		SkinPaintTool::FMultiBoneWeightEdits& InOutWeightEdits);
	// same as EditWeightOfVertices() but specific to relaxation (topology aware operation)
	void RelaxWeightOnVertices(
		TArray<int32> VerticesToEdit,
		TArray<float> VertexFalloffs,
		const float UseStrength,
		SkinPaintTool::FMultiBoneWeightEdits& InOutWeightEdits);

	// used to accelerate mesh queries
	UE::Geometry::TDynamicVerticesOctree3<FDynamicMesh3> VerticesOctree;
	UE::Geometry::FDynamicMeshOctree3 TrianglesOctree;
	TFuture<void> TriangleOctreeFuture;
	TArray<int32> TrianglesToReinsert;

	// tool properties
	UPROPERTY()
	TObjectPtr<USkinWeightsPaintToolProperties> WeightToolProperties;
	virtual void OnPropertyModified(UObject* ModifiedObject, FProperty* ModifiedProperty) override;
	
	// the currently edited mesh description
	TUniquePtr<FMeshDescription> EditedMesh;

	// storage of vertex weights per bone 
	SkinPaintTool::FSkinToolWeights Weights;

	// cached mirror data
	SkinPaintTool::FSkinMirrorData MirrorData;

	// storage for weight edits in the current transaction
	TUniquePtr<SkinPaintTool::FMeshSkinWeightsChange> ActiveChange;

	// Smooth weights data source and operator
	TUniquePtr<UE::Geometry::TBoneWeightsDataSource<int32, float>> SmoothWeightsDataSource;
	TUniquePtr<UE::Geometry::TSmoothBoneWeights<int32, float>> SmoothWeightsOp;

	// vertex colors updated when switching current bone or editing weights
	void UpdateCurrentBoneVertexColors();
	FVector4f WeightToColor(float Value) const;
	bool bVisibleWeightsValid = false;

	// which bone are we currently painting?
	void UpdateCurrentBone(const FName &BoneName);
	FName CurrentBone = NAME_None;
	TOptional<FName> PendingCurrentBone;
	TArray<FName> SelectedBoneNames;
	TArray<BoneIndex> SelectedBoneIndices;

	// determines the set of vertices to operate on, using selection as the priority
	void GetVerticesToEdit(TArray<VertexIndex>& OutVertexIndices) const;
	
	BoneIndex GetBoneIndexFromName(const FName BoneName) const;

	// ISkeletalMeshEditionInterface
	virtual void HandleSkeletalMeshModified(const TArray<FName>& InBoneNames, const ESkeletalMeshNotifyType InNotifyType) override;

	// polygon selection mechanic
	UPROPERTY()
	TObjectPtr<UPolygonSelectionMechanic> PolygonSelectionMechanic;
	TUniquePtr<UE::Geometry::FDynamicMeshAABBTree3> MeshSpatial = nullptr;
	TUniquePtr<UE::Geometry::FTriangleGroupTopology> SelectionTopology = nullptr;

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshEditorContextObjectBase> EditorContext = nullptr;
	UPROPERTY()
	TWeakObjectPtr<UPersonaEditorModeManagerContext> PersonaModeManagerContext = nullptr;

	friend SkinPaintTool::FSkinToolDeformer;
};
