// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "InteractiveToolActivity.h"
#include "InteractiveToolChange.h" //FToolCommandChange
#include "ToolContextInterfaces.h" // FViewCameraState

#include "PolyEditCutFacesActivity.generated.h"

class UPolyEditActivityContext;
class UPolyEditPreviewMesh;
class UCollectSurfacePathMechanic;

UENUM()
enum class EPolyEditCutPlaneOrientation
{
	FaceNormals,
	ViewDirection
};

UCLASS()
class MESHMODELINGTOOLS_API UPolyEditCutProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Cut)
	EPolyEditCutPlaneOrientation Orientation = EPolyEditCutPlaneOrientation::FaceNormals;

	UPROPERTY(EditAnywhere, Category = Cut)
	bool bSnapToVertices = true;
};


/**
 *
 */
UCLASS()
class MESHMODELINGTOOLS_API UPolyEditCutFacesActivity : public UInteractiveToolActivity,
	public IClickBehaviorTarget, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	// IInteractiveToolActivity
	virtual void Setup(UInteractiveTool* ParentTool) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual bool CanStart() const override;
	virtual EToolActivityStartResult Start() override;
	virtual bool IsRunning() const override { return bIsRunning; }
	virtual bool CanAccept() const override;
	virtual EToolActivityEndResult End(EToolShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void Tick(float DeltaTime) override;

	// IClickBehaviorTarget API
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

	// IHoverBehaviorTarget implementation
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override {}
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override {}

protected:
	void Clear();
	void BeginCutFaces();
	void ApplyCutFaces();

	UPROPERTY()
	TObjectPtr<UPolyEditCutProperties> CutProperties;

	UPROPERTY()
	TObjectPtr<UPolyEditPreviewMesh> EditPreview;

	UPROPERTY()
	TObjectPtr<UCollectSurfacePathMechanic> SurfacePathMechanic = nullptr;

	UPROPERTY()
	TObjectPtr<UPolyEditActivityContext> ActivityContext;

	bool bIsRunning = false;
	int32 ActivityStamp = 1;

	FViewCameraState CameraState;

	friend class FPolyEditCutFacesActivityFirstPointChange;
};

/**
 * This should get emitted when setting the first point so that we can undo it.
 */
class MESHMODELINGTOOLS_API FPolyEditCutFacesActivityFirstPointChange : public FToolCommandChange
{
public:
	FPolyEditCutFacesActivityFirstPointChange(int32 CurrentActivityStamp)
		: ActivityStamp(CurrentActivityStamp)
	{};

	virtual void Apply(UObject* Object) override {};
	virtual void Revert(UObject* Object) override;
	virtual bool HasExpired(UObject* Object) const override
	{
		return bHaveDoneUndo || Cast<UPolyEditCutFacesActivity>(Object)->ActivityStamp != ActivityStamp;
	}
	virtual FString ToString() const override
	{
		return TEXT("FPolyEditCutFacesActivityFirstPointChange");
	}

protected:
	int32 ActivityStamp;
	bool bHaveDoneUndo = false;
};