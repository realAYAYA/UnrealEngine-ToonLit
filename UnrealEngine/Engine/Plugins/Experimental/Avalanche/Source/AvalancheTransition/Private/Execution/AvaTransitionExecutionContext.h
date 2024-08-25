// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "StateTreeExecutionContext.h"

struct FAvaTransitionExecutionContext : FStateTreeExecutionContext
{
	FAvaTransitionExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData);

	void SetSceneDescription(FString&& InSceneDescription);

protected:
	//~ Begin FStateTreeExecutionContext
	virtual FString GetInstanceDescription() const override;
	//~ End FStateTreeExecutionContext

private:
	FString SceneDescription;
};
