// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Templates/SharedPointer.h"

class SWidget;

struct AVALANCHEEDITORCORE_API FAvaUserInputDataTypeBase : public TSharedFromThis<FAvaUserInputDataTypeBase>
{
	friend class SAvaUserInputDialog;

	FAvaUserInputDataTypeBase();

	virtual ~FAvaUserInputDataTypeBase() = default;

	virtual TSharedRef<SWidget> CreateInputWidget() = 0;

protected:
	DECLARE_DELEGATE(FOnCommit)
	FOnCommit OnCommit;

	void OnUserCommit();
};
