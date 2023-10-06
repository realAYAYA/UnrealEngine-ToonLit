// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSubsystem.h"
#include "Containers/ArrayView.h"
#include "AnimSubsystem_BlendSpaceGraph.generated.h"

class UBlendSpace;

USTRUCT()
struct FAnimSubsystem_BlendSpaceGraph : public FAnimSubsystem
{
	GENERATED_BODY()

	friend class UAnimBlueprintExtension_BlendSpaceGraph;

	/** Access the property access library */
	TArrayView<const TObjectPtr<UBlendSpace>> GetBlendSpaces() const { return BlendSpaces; }

private:
	// Any internal blendspaces we host
	UPROPERTY()
	TArray<TObjectPtr<UBlendSpace>> BlendSpaces;
};
