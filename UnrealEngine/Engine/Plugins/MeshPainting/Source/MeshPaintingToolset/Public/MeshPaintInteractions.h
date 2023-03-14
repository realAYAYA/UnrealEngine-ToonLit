// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractionMechanic.h"
#include "UObject/Interface.h"
#include "MeshPaintInteractions.generated.h"

class IMeshPaintComponentAdapter;
class AActor;
class UMeshComponent;
class UMeshToolManager;

UINTERFACE()
class MESHPAINTINGTOOLSET_API UMeshPaintSelectionInterface : public UInterface
{
	GENERATED_BODY()
};

class MESHPAINTINGTOOLSET_API IMeshPaintSelectionInterface
{
	GENERATED_BODY()
public:
	virtual bool IsMeshAdapterSupported(TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter) const PURE_VIRTUAL(IMeshPaintSelectionInterface::IsMeshAdapterSupported, return false;);
	virtual bool AllowsMultiselect() const PURE_VIRTUAL(IMeshPaintSelectionInterface::AllowsMultiselect, return false;);
};


UCLASS()
class MESHPAINTINGTOOLSET_API UMeshPaintSelectionMechanic : public UInteractionMechanic
{
	GENERATED_BODY()

public:
	FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos, bool bIsFallbackClick = false);
	void OnClicked(const FInputDeviceRay& ClickPos);
	void SetAddToSelectionSet(const bool bInNewSelectionType)
	{
		bAddToSelectionSet = bInNewSelectionType;
	}
protected:
	bool FindClickedComponentsAndCacheAdapters(const FInputDeviceRay& ClickPos);

protected:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> CachedClickedComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CachedClickedActors;
	bool bAddToSelectionSet;
};
