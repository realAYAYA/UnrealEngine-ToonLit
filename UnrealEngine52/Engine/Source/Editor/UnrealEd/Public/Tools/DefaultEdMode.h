// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/LegacyEdModeWidgetHelpers.h"

#include "DefaultEdMode.generated.h"

UCLASS(Transient)
class UNREALED_API UEdModeDefault : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()

public:
	UEdModeDefault();

	virtual bool UsesPropertyWidgets() const override;
	virtual bool UsesToolkits() const override;

protected:
	virtual TSharedRef<FLegacyEdModeWidgetHelper> CreateWidgetHelper() override;
};
