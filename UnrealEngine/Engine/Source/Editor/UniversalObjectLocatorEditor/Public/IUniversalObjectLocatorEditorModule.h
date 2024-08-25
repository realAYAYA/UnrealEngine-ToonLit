// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointerFwd.h"

class FName;

namespace UE::UniversalObjectLocator
{

class ILocatorEditor;

class IUniversalObjectLocatorEditorModule
	: public IModuleInterface
{
public:

	virtual void RegisterLocatorEditor(FName Name, TSharedPtr<ILocatorEditor> LocatorEditor) = 0;
	virtual void UnregisterLocatorEditor(FName Name) = 0;
};


} // namespace UE::UniversalObjectLocator