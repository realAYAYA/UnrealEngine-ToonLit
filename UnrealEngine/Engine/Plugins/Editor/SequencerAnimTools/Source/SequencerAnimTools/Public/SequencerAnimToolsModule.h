// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/UICommandList.h"
#include "Modules/ModuleManager.h"

namespace UE
{
namespace SequencerAnimTools
{


class FSequencerAnimToolsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FUICommandList> CommandBindings;

	//delegates
	void OnMotionTralOptionChanged(FName PropertyName);
};

} // namespace SequencerAnimTools
} // namespace UE
