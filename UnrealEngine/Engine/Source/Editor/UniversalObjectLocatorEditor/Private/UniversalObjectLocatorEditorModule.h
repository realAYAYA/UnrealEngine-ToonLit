// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UniversalObjectLocatorFwd.h"
#include "IUniversalObjectLocatorEditorModule.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

namespace UE::UniversalObjectLocator
{

class ILocatorEditor;

class FUniversalObjectLocatorEditorModule
	: public IUniversalObjectLocatorEditorModule
{
public:

	void StartupModule() override;

	void ShutdownModule() override;

	void RegisterLocatorEditor(FName LocatorName, TSharedPtr<ILocatorEditor> LocatorEditor) override;

	void UnregisterLocatorEditor(FName LocatorName) override;

	TMap<FName, TSharedPtr<ILocatorEditor>> LocatorEditors;
};

} // namespace UE::UniversalObjectLocator


