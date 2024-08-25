// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/Reply.h"
#include "Widgets/BindingEntry/SMVVMBaseRow.h"
#include "Widgets/SMVVMFieldSelectorMenu.h"


namespace UE::MVVM::BindingEntry
{

/**
 *
 */
class SFunctionParameterRow : public SBaseRow
{
protected:
	virtual TSharedRef<SWidget> BuildRowWidget() override;
	virtual const ANSICHAR* GetTableRowStyle() const override
	{
		return "BindingView.ParameterRow";
	}
};

} // namespace UE::MVVM
