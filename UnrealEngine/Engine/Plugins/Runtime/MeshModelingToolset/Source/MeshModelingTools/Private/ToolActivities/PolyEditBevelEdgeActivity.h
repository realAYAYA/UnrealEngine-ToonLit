// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "GeometryBase.h"
#include "GroupTopology.h" // FGroupTopologySelection
#include "InteractiveToolActivity.h"
#include "FrameTypes.h"
#include "ModelingOperators.h"

#include "PolyEditBevelEdgeActivity.generated.h"

class UPolyEditActivityContext;
class UPolyEditPreviewMesh;
PREDECLARE_GEOMETRY(class FDynamicMesh3);



UCLASS()
class MESHMODELINGTOOLS_API UPolyEditBevelEdgeProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Bevel)
	double BevelDistance = 4.0;
};


/**
 * 
 */
UCLASS()
class MESHMODELINGTOOLS_API UPolyEditBevelEdgeActivity : public UInteractiveToolActivity,
	public UE::Geometry::IDynamicMeshOperatorFactory

{
	GENERATED_BODY()

public:

	// IInteractiveToolActivity
	virtual void Setup(UInteractiveTool* ParentTool) override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual bool CanStart() const override;
	virtual EToolActivityStartResult Start() override;
	virtual bool IsRunning() const override { return bIsRunning; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;
	virtual EToolActivityEndResult End(EToolShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void Tick(float DeltaTime) override;

	// IDynamicMeshOperatorFactory
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UPolyEditBevelEdgeProperties> BevelProperties = nullptr;

protected:

	virtual void BeginBevel();
	virtual void ApplyBevel();
	virtual void EndInternal();

	UPROPERTY()
	TObjectPtr<UPolyEditActivityContext> ActivityContext;

	bool bIsRunning = false;

	UE::Geometry::FGroupTopologySelection ActiveSelection;
};
