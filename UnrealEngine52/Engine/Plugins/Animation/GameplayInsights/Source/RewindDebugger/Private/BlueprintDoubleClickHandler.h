// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IRewindDebuggerDoubleClickHandler.h"

class FBlueprintDoubleClickHandler : public IRewindDebuggerDoubleClickHandler
{
	virtual FName GetTargetTypeName() const override;
	virtual bool HandleDoubleClick(IRewindDebugger* RewindDebugger) override;
};