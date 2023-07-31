// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRewindDebugger.h"
#include "Features/IModularFeatures.h"

// Interface class for adding custom double click handling to components
class REWINDDEBUGGERINTERFACE_API IRewindDebuggerDoubleClickHandler : public IModularFeature
{
public:
	static const FName ModularFeatureName;
	
	// returns the name of a type of UObject for which this menu extender will be called
	virtual FName GetTargetTypeName() const = 0;

	// called when the selected component has been double clicked.  return true if something was opened
	virtual bool HandleDoubleClick(IRewindDebugger* IRewindDebugger) = 0;
};
