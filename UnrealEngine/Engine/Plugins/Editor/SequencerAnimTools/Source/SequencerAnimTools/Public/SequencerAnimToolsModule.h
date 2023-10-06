// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class FUICommandList;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Framework/Commands/UICommandList.h"
#include "Modules/ModuleManager.h"
#endif
