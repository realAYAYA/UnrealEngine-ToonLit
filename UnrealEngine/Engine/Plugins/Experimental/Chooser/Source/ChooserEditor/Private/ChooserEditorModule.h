// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyAccessChainCustomization.h"
#include "Modules/ModuleInterface.h"

namespace UE::ChooserEditor
{

class FModule : public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

}