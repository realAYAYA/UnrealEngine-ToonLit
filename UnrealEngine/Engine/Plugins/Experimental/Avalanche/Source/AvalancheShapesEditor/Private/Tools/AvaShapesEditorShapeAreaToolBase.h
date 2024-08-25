// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaShapesEditorShapeToolBase.h"
#include "AvaShapesEditorShapeAreaToolBase.generated.h"

class AActor;
class AAvaShapeActor;

UCLASS(Abstract)
class UAvaShapesEditorShapeAreaToolBase : public UAvaShapesEditorShapeToolBase
{
	GENERATED_BODY()

	using UAvaShapesEditorShapeToolBase::SpawnActor;

public:
	UAvaShapesEditorShapeAreaToolBase();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual void OnViewportPlannerUpdate() override;
	virtual void OnViewportPlannerComplete() override;
	virtual void DefaultAction() override;
	//~ End UAvaInteractiveToolsToolBase

protected:
	void UpdateShapeSize(AAvaShapeActor* InShapeActor) const;
	void SelectActor(AActor* InActor) const;

	//~ Begin UAvaInteractiveToolsToolBase
	virtual AActor* SpawnActor(TSubclassOf<AActor> InActorClass, EAvaViewportStatus InViewportStatus, 
		const FVector2f& InViewportPosition, bool bInPreview, FString* InActorLabelOverride = nullptr) const override;
	//~ End UAvaInteractiveToolsToolBase
};
