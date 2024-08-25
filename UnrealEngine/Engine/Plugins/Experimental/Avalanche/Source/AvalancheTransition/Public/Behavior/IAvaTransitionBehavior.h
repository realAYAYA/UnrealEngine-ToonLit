// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionEnums.h"
#include "Containers/ArrayView.h"
#include "Delegates/Delegate.h"
#include "InstancedStruct.h"
#include "UObject/Interface.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakInterfacePtr.h"
#include "IAvaTransitionBehavior.generated.h"

class UAvaTransitionTree;
class UObject;
enum class EStateTreeRunStatus : uint8;
struct FAvaTransitionScene;
struct FStateTreeReference;

UINTERFACE(MinimalAPI, NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UAvaTransitionBehavior : public UInterface
{
	GENERATED_BODY()
};

class IAvaTransitionBehavior
{
	GENERATED_BODY()

public:
	virtual UObject& AsUObject() = 0;

	/** Gets the underlying Transition Tree that this Transition Behavior runs */
	virtual UAvaTransitionTree* GetTransitionTree() const = 0;

	/** Gets the underlying State Tree Reference that this Transition Behavior runs */
	virtual const FStateTreeReference& GetStateTreeReference() const = 0;
};
