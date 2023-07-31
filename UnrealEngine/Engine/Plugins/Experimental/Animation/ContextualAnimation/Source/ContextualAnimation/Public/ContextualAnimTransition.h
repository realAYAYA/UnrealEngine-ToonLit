// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "ContextualAnimTransition.generated.h"

class AActor;
class UContextualAnimSceneInstance;

UCLASS(Const, Abstract, BlueprintType, Blueprintable, EditInlineNew)
class CONTEXTUALANIMATION_API UContextualAnimTransition : public UObject
{
	GENERATED_BODY()

public:

	UContextualAnimTransition(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintImplementableEvent, Category = "Defaults")
	bool CanEnterTransition(const UContextualAnimSceneInstance* SceneInstance, const FName& FromSection, const FName& ToSection) const;
};