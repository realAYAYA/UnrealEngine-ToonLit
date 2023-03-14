// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "BoxTypes.h"
#include "InteractiveToolActivity.h"
#include "ToolActivities/PolyEditActivityUtil.h"
#include "ToolContextInterfaces.h" // FViewCameraState

#include "PolyEditPlanarProjectionUVActivity.generated.h"

class UPolyEditActivityContext;
class UPolyEditPreviewMesh;
class UCollectSurfacePathMechanic;

UCLASS()
class MESHMODELINGTOOLS_API UPolyEditSetUVProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = PlanarProjectUV)
	bool bShowMaterial = false;
};


/**
 *
 */
UCLASS()
class MESHMODELINGTOOLS_API UPolyEditPlanarProjectionUVActivity : public UInteractiveToolActivity,
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
	void BeginSetUVs();
	void UpdateSetUVS();
	void ApplySetUVs();

	UPROPERTY()
	TObjectPtr<UPolyEditSetUVProperties> SetUVProperties;

	UPROPERTY()
	TObjectPtr<UPolyEditPreviewMesh> EditPreview;

	UPROPERTY()
	TObjectPtr<UCollectSurfacePathMechanic> SurfacePathMechanic = nullptr;

	UPROPERTY()
	TObjectPtr<UPolyEditActivityContext> ActivityContext;

	bool bIsRunning = false;

	bool bPreviewUpdatePending = false;
	UE::Geometry::PolyEditActivityUtil::EPreviewMaterialType CurrentPreviewMaterial;
	UE::Geometry::FAxisAlignedBox3d ActiveSelectionBounds;
	FViewCameraState CameraState;
};
