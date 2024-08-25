// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SWidget;

namespace UE::AnimNext::Editor
{

struct FParameterPickerArgs;

class IModule : public IModuleInterface
{
public:
	// Create a parameter picker
	virtual TSharedRef<SWidget> CreateParameterPicker(const FParameterPickerArgs& InArgs) = 0;
};

}