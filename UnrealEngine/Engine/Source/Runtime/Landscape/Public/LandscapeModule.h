// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "LandscapeEditorServices.h"

class FLandscapeSceneViewExtension;

/**
* Landscape module interface
*/
class ILandscapeModule : public IModuleInterface
{
public:
	virtual TSharedPtr<FLandscapeSceneViewExtension, ESPMode::ThreadSafe> GetLandscapeSceneViewExtension() const = 0;
	
	virtual void SetLandscapeEditorServices(ILandscapeEditorServices* InLandscapeEditorServices) = 0;
	virtual ILandscapeEditorServices* GetLandscapeEditorServices() const = 0;
};
