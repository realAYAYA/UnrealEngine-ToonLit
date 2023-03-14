// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "Animation/AnimInstance.h"
#include "PreviewCollectionInterface.generated.h"

class UAnimInstance;
class USkeletalMesh;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class ENGINE_API UPreviewCollectionInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/** Preview Collection options. If you have native UDataAsset class that implements this, you can preview all in the animation editor using Additional Mesh section  */
class ENGINE_API IPreviewCollectionInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Returns nodes that needs for them to map */
	UE_DEPRECATED(4.21, "This interface no longer used. Use GetPreviewSkeletalMeshes(TArray<USkeletalMesh*>& OutList, TArray<TSubclassOf<UAnimInstance>>& OutAnimBP).")
	virtual void GetPreviewSkeletalMeshes(TArray<USkeletalMesh*>& OutList) const;

	/** If you want this to set base mesh also, please use this interface. If this returns nullptr, it will just use whatever set up right now */
	virtual USkeletalMesh* GetPreviewBaseMesh() const { return nullptr; }
	/** Returns nodes that needs for them to map */
	virtual void GetPreviewSkeletalMeshes(TArray<USkeletalMesh*>& OutList, TArray<TSubclassOf<UAnimInstance>>& OutAnimBP) const = 0;
};
