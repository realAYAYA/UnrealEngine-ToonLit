// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMeshBrushTool.h"
#include "BaseTools/MeshSurfacePointMeshEditingTool.h"
#include "Changes/ValueWatcher.h"
#include "Changes/IndexedAttributeChange.h"
#include "DynamicMesh/DynamicVerticesOctree3.h"
#include "MeshDescription.h"
#include "MeshAttributePaintTool.generated.h"


struct FMeshDescription;
class UMeshAttributePaintTool;


/**
 * Maps float values to linear color ramp.
 */
class FFloatAttributeColorMapper
{
public:
	virtual ~FFloatAttributeColorMapper() {}

	FLinearColor LowColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);
	FLinearColor HighColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	virtual FLinearColor ToColor(float Value)
	{
		float t = FMath::Clamp(Value, 0.0f, 1.0f);
		return FLinearColor(
			FMathf::Lerp(LowColor.R, HighColor.R, t),
			FMathf::Lerp(LowColor.G, HighColor.G, t),
			FMathf::Lerp(LowColor.B, HighColor.B, t),
			1.0f);
	}

	template<typename VectorType>
	VectorType ToColor(float Value)
	{
		FLinearColor Color = ToColor(Value);
		return VectorType(Color.R, Color.G, Color.B);
	}
};


/**
 * Abstract interface to a single-channel indexed floating-point attribute
 */
class IMeshVertexAttributeAdapter
{
public:
	virtual ~IMeshVertexAttributeAdapter() {}

	virtual int ElementNum() const = 0;
	virtual float GetValue(int32 Index) const = 0;
	virtual void SetValue(int32 Index, float Value) = 0;
	virtual UE::Geometry::FInterval1f GetValueRange() = 0;
};



/**
 * Abstract interface to a set of single-channel indexed floating-point attributes
 */
class IMeshVertexAttributeSource
{
public:
	virtual ~IMeshVertexAttributeSource() {}

	virtual TArray<FName> GetAttributeList() = 0;
	virtual TUniquePtr<IMeshVertexAttributeAdapter> GetAttribute(FName AttributeName) = 0;
	/** @return number of indices in each attribute */
	virtual int32 GetAttributeElementNum() = 0;
};






/**
 * Tool Builder for Attribute Paint Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshAttributePaintToolBuilder : public UMeshSurfacePointMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	/** Optional color map customization */
	TUniqueFunction<TUniquePtr<FFloatAttributeColorMapper>()> ColorMapFactory;

	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};



UENUM()
enum class EBrushActionMode
{
	Paint,
	FloodFill
};


/**
 * Selected-Attribute settings Attribute Paint Tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshAttributePaintBrushOperationProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = Attribute)
	EBrushActionMode BrushAction = EBrushActionMode::Paint;
};





UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshAttributePaintToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (DisplayName = "Selected Attribute", GetOptions = GetAttributeNames))
	FString Attribute;

	UFUNCTION()
	const TArray<FString>& GetAttributeNames() { return Attributes; };

	TArray<FString> Attributes;

public:
	/**
	* Initialize the internal array of attribute names
	* @param bInitialize if set, selected Attribute will be reset to the first attribute or empty if there are none.
	*/
	void Initialize(const TArray<FName>& AttributeNames, bool bInitialize = false);

	/**
	 * Verify that the attribute selection is valid
	 * @param bUpdateIfInvalid if selection is not valid, use attribute at index 0 or empty if there are no attributes
	 * @return true if selection is in the Attributes array
	 */
	bool ValidateSelectedAttribute(bool bUpdateIfInvalid);

	/**
	 * @return selected attribute index, or -1 if invalid selection
	 */
	int32 GetSelectedAttributeIndex();
};





UENUM()
enum class EMeshAttributePaintToolActions
{
	NoAction
};



UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshAttributePaintEditActions : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UMeshAttributePaintTool> ParentTool;

	void Initialize(UMeshAttributePaintTool* ParentToolIn) { ParentTool = ParentToolIn; }

	void PostAction(EMeshAttributePaintToolActions Action);
};



/**
 * FCommandChange for color map changes
 */
class MESHMODELINGTOOLSEXP_API FMeshAttributePaintChange : public TCustomIndexedValuesChange<float, int32>
{
public:
	virtual FString ToString() const override
	{
		return FString(TEXT("Paint Attribute"));
	}
};



/**
 * UMeshAttributePaintTool paints single-channel float attributes on a MeshDescription.
 * 
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshAttributePaintTool : public UDynamicMeshBrushTool
{
	GENERATED_BODY()

public:
	virtual void SetWorld(UWorld* World);

	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	virtual void Setup() override;
	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }

	// UBaseBrushTool overrides
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;

	virtual void RequestAction(EMeshAttributePaintToolActions ActionType);

	virtual void SetColorMap(TUniquePtr<FFloatAttributeColorMapper> ColorMap);

protected:
	virtual void ApplyStamp(const FBrushStampData& Stamp);


	struct FStampActionData
	{
		TArray<int32> ROIVertices;
		TArray<float> ROIBefore;
		TArray<float> ROIAfter;
	};


	virtual void ApplyStamp_Paint(const FBrushStampData& Stamp, FStampActionData& ActionData);
	virtual void ApplyStamp_FloodFill(const FBrushStampData& Stamp, FStampActionData& ActionData);

	virtual void OnShutdown(EToolShutdownType ShutdownType) override;
	

protected:
	UPROPERTY()
	TObjectPtr<UMeshAttributePaintBrushOperationProperties> BrushActionProps;

	UPROPERTY()
	TObjectPtr<UMeshAttributePaintToolProperties> AttribProps;

	TValueWatcher<int32> SelectedAttributeWatcher;

	//UPROPERTY()
	//UMeshAttributePaintEditActions* AttributeEditActions;

protected:
	UWorld* TargetWorld;

	bool bInRemoveStroke = false;
	bool bInSmoothStroke = false;
	FBrushStampData StartStamp;
	FBrushStampData LastStamp;
	bool bStampPending;

	TUniquePtr<FMeshDescription> EditedMesh;

	double CalculateBrushFalloff(double Distance);
	UE::Geometry::TDynamicVerticesOctree3<FDynamicMesh3> VerticesOctree;
	TArray<int> PreviewBrushROI;
	void CalculateVertexROI(const FBrushStampData& Stamp, TArray<int>& VertexROI);

	TUniquePtr<FFloatAttributeColorMapper> ColorMapper;
	TUniquePtr<IMeshVertexAttributeSource> AttributeSource;

	struct FAttributeData
	{
		FName Name;
		TUniquePtr<IMeshVertexAttributeAdapter> Attribute;
		TArray<float> CurrentValues;
		TArray<float> InitialValues;
	};
	TArray<FAttributeData> Attributes;
	int32 AttributeBufferCount;
	int32 CurrentAttributeIndex;
	UE::Geometry::FInterval1f CurrentValueRange;

	// actions

	bool bHavePendingAction = false;
	EMeshAttributePaintToolActions PendingAction;
	virtual void ApplyAction(EMeshAttributePaintToolActions ActionType);

	bool bVisibleAttributeValid = false;
	int32 PendingNewSelectedIndex = -1;
	
	void InitializeAttributes();
	void StoreCurrentAttribute();
	void UpdateVisibleAttribute();
	void UpdateSelectedAttribute(int32 NewSelectedIndex);

	TUniquePtr<TIndexedValuesChangeBuilder<float, FMeshAttributePaintChange>> ActiveChangeBuilder;
	void BeginChange();
	TUniquePtr<FMeshAttributePaintChange> EndChange();
	void ExternalUpdateValues(int32 AttribIndex, const TArray<int32>& VertexIndices, const TArray<float>& NewValues);
};







