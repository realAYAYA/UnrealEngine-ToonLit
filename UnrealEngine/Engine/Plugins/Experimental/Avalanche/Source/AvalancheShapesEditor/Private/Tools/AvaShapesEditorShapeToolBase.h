// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaInteractiveToolsActorToolBase.h"
#include "AvaShapeFactory.h"
#include "AvaShapeParametricMaterial.h"
#include "Templates/SubclassOf.h"
#include "AvaShapesEditorShapeToolBase.generated.h"

class AAvaShapeActor;
class UAvaShapeDynamicMeshBase;

UCLASS(Abstract)
class UAvaShapesEditorShapeToolBase : public UAvaInteractiveToolsActorToolBase
{
	GENERATED_BODY()

public:
	using UAvaInteractiveToolsActorToolBase::SpawnActor;

	static constexpr float DefaultDim = 100;

	UAvaShapesEditorShapeToolBase();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual bool OnBegin() override;
	//~ End UAvaInteractiveToolsToolBase

protected:
	template<typename InMeshClass
		UE_REQUIRES(std::derived_from<InMeshClass, UAvaShapeDynamicMeshBase>)>
	static UAvaShapeFactory* CreateFactory()
	{
		UAvaShapeFactory* Factory = CreateActorFactory<UAvaShapeFactory>();
		Factory->SetMeshClass(InMeshClass::StaticClass());
		return Factory;
	}

	UPROPERTY()
	TSubclassOf<UAvaShapeDynamicMeshBase> ShapeClass = nullptr;

	FAvaShapeParametricMaterial DefaultMaterialParams;

	virtual void InitShape(UAvaShapeDynamicMeshBase* InShape) const;

	virtual void SetShapeSize(AAvaShapeActor* InShapeActor, const FVector2D& InShapeSize) const;

	//~ Begin UAvaInteractiveToolsToolBase
	virtual bool UseIdentityLocation() const override;
	virtual bool UseIdentityRotation() const override;

	virtual AActor* SpawnActor(TSubclassOf<AActor> InActorClass, EAvaViewportStatus InViewportStatus, 
		const FVector2f& InViewportPosition, bool bInPreview, FString* InActorLabelOverride = nullptr) const override;
	//~ End UAvaInteractiveToolsToolBase

	virtual FString GetActorNameOverride() const;
};
