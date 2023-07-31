// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/PlacementClickDragToolBase.h"

#include "PlacementPlaceSingleTool.generated.h"

struct FAssetPlacementInfo;

UCLASS(Transient, MinimalAPI)
class UPlacementModePlaceSingleToolBuilder : public UPlacementToolBuilderBase
{
	GENERATED_BODY()

protected:
	virtual UPlacementBrushToolBase* FactoryToolInstance(UObject* Outer) const override;
};

UENUM()
enum class EPlacementScaleToCursorType : uint8
{
	/** Increases scale as the cursor moves outward from the placed asset. */
	Positive,
	/** Scale is unchanged by cursor movement for the placed asset. */
	None,
};

UCLASS(config = EditorPerProjectUserSettings)
class UPlacementModePlaceSingleToolSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** How the cursor movement should apply to scale once an asset is placed. Note that the maximum bound is controlled by scale settings of the mode. */
	UPROPERTY(config, EditAnywhere, Category = "Single Place Settings")
	EPlacementScaleToCursorType ScalingType = EPlacementScaleToCursorType::Positive;

	/** If the tool should automatically select the last placed asset. */
	UPROPERTY(config, EditAnywhere, Category = "Single Place Settings")
	bool bSelectAfterPlacing = false;
};

UCLASS(Transient)
class UPlacementModePlaceSingleTool : public UPlacementClickDragToolBase
{
	GENERATED_BODY()

public:
	UPlacementModePlaceSingleTool();
	~UPlacementModePlaceSingleTool();
	UPlacementModePlaceSingleTool(FVTableHelper& Helper);
	
	constexpr static TCHAR ToolName[] = TEXT("PlaceSingleTool");

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnClickPress(const FInputDeviceRay& Ray) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;

	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& DevicePos) override;

protected:
	void GeneratePlacementData(const FInputDeviceRay& DevicePos);
	void CreatePreviewElements(const FInputDeviceRay& DevicePos);
	void UpdatePreviewElements(const FInputDeviceRay& DevicePos);
	void DestroyPreviewElements();
	void EnterTweakState(TArrayView<const FTypedElementHandle> InElementHandles);
	void ExitTweakState(bool bClearSelectionSet);
	void UpdateElementTransforms(TArrayView<const FTypedElementHandle> InElements, const FTransform& InTransform, bool bLocalTransform = false);
	void NotifyMovementStarted(TArrayView<const FTypedElementHandle> InElements);
	void NotifyMovementEnded(TArrayView<const FTypedElementHandle> InElements);
	void SetupRightClickMouseBehavior();

	UPROPERTY(Transient)
	TObjectPtr<UPlacementModePlaceSingleToolSettings> SinglePlaceSettings;

	TUniquePtr<FAssetPlacementInfo> PlacementInfo;
	TArray<FTypedElementHandle> PreviewElements;
	TArray<FTypedElementHandle> PlacedElements;
	bool bIsTweaking;
};
