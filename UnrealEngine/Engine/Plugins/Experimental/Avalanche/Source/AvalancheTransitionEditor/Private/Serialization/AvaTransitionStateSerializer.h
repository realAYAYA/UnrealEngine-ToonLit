// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"

class FString;
class UAvaTransitionTreeEditorData;
class UStateTreeState;

class FAvaTransitionStateSerializer
{
public:
	static FString ExportText(UAvaTransitionTreeEditorData& InEditorData, TConstArrayView<UStateTreeState*> InStates);

	static bool ImportText(const FString& InText, UAvaTransitionTreeEditorData& InEditorData, UStateTreeState* InState, TArray<UStateTreeState*>& OutNewStates, UObject*& OutParent);
};
