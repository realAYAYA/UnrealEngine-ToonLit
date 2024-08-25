// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FAvaTransitionEditorViewModel;
class FUICommandList;

class FAvaTransitionActions : public TSharedFromThis<FAvaTransitionActions>
{
public:
	explicit FAvaTransitionActions(FAvaTransitionEditorViewModel& InOwner);

	virtual ~FAvaTransitionActions() = default;

	virtual void BindCommands(const TSharedRef<FUICommandList>& InCommandList) = 0;

	bool IsEditMode() const;

protected:
	FAvaTransitionEditorViewModel& Owner;
};
