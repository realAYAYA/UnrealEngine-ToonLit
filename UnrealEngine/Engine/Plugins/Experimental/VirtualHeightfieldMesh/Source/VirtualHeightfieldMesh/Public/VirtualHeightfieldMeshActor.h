// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VirtualHeightfieldMeshActor.generated.h"

UCLASS(hidecategories = (Actor, Collision, Cooking, Input, LOD, Replication), MinimalAPI)
class AVirtualHeightfieldMesh : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	/** Component for rendering the big mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = VirtualTexture, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVirtualHeightfieldMeshComponent> VirtualHeightfieldMeshComponent;

#if WITH_EDITORONLY_DATA
	/** Box for visualizing virtual texture extents. */
	UPROPERTY(Transient)
	TObjectPtr<class UBoxComponent> Box = nullptr;
#endif // WITH_EDITORONLY_DATA

protected:
	//~ Begin UObject Interface.
	virtual bool NeedsLoadForServer() const override { return false; }
	//~ End UObject Interface.
};
