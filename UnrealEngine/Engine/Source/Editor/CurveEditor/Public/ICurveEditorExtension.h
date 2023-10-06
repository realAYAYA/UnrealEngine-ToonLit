// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Templates/SharedPointer.h"

class FUICommandList;
class FExtender;

class ICurveEditorExtension
{
public:
	virtual ~ICurveEditorExtension() {}

	/**
	 * Allows the editors to bind commands.
	 *
	 * @param CommandBindings The existing command bindings to map to.
	*/
	virtual void BindCommands(TSharedRef<FUICommandList> CommandBindings) = 0;
};