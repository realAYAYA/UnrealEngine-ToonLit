// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class AActor;

class FAvaInteractiveToolsToolPresetBase : public TSharedFromThis<FAvaInteractiveToolsToolPresetBase>
{
public:
	virtual ~FAvaInteractiveToolsToolPresetBase() = default;

	virtual void ApplyPreset(AActor* InActor) const = 0;

	const FText& GetName() { return Name; }

	const FText& GetDescription() { return Description; }

protected:
	FText Name;
	FText Description;
};