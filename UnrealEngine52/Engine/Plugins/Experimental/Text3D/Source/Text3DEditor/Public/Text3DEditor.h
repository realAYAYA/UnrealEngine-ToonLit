// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"


class FText3DEditorModule final : public IModuleInterface
{
public:
	FText3DEditorModule() = default;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TUniquePtr<FSlateStyleSet> StyleSet;
};