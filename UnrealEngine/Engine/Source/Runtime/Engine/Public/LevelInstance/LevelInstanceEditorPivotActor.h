// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelInstance/LevelInstanceEditorPivotInterface.h"
#include "LevelInstanceEditorPivotActor.generated.h"

UCLASS(transient, notplaceable, hidecategories=(Rendering,Replication,Collision,Partition,Input,HLOD,Actor,Cooking))
class ENGINE_API ALevelInstancePivot : public AActor, public ILevelInstanceEditorPivotInterface
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
public:
	virtual bool CanDeleteSelectedActor(FText& OutReason) const override { return false; }
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual bool ShouldExport() override { return false; }
private:
	virtual bool ActorTypeSupportsDataLayer() const { return false; }
#endif
};