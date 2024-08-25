// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Behavior/IAvaTransitionBehavior.h"
#include "Templates/SharedPointerFwd.h"
#include "UObject/WeakObjectPtr.h"

class SWidget;
class UAvaTransitionTree;
class UAvaTransitionTreeEditorData;

namespace UE::AvaTransitionEditor
{
	TSharedPtr<SWidget> CreateTransitionLayerPicker(UAvaTransitionTreeEditorData* InEditorData, bool bInCompileOnLayerPicked);

	void ValidateTree(UAvaTransitionTree& InTransitionTree);

	uint32 CalculateTreeHash(UAvaTransitionTree& InTransitionTree);

	bool PickTransitionTreeAsset(const FText& InDialogTitle, UAvaTransitionTree*& OutTransitionTree);

	void ToggleTransitionTreeEnabled(TWeakObjectPtr<UAvaTransitionTree> InTransitionTreeWeak);

	bool IsTransitionTreeEnabled(TWeakObjectPtr<UAvaTransitionTree> InTransitionTreeWeak);
}
