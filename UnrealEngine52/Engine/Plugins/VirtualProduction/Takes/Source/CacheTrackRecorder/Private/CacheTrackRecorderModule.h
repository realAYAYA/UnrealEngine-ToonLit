// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Delegates/IDelegateInstance.h"
#include "UObject/GCObject.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Modules/ModuleInterface.h"
#include "Widgets/Input/SCheckBox.h"

class FCacheTrackRecorderModule : public IModuleInterface
{
public:
	FCacheTrackRecorderModule();
	
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void OnEditorClose();
};
