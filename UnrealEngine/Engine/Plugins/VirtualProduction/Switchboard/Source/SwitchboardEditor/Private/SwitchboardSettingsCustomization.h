// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"


struct FSlateBrush;


class FSwitchboardEditorSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	FText GetHealthRowHintText() const;
	FText GetHealthRowButtonText() const;
	const FSlateBrush* GetHealthRowBorderBrush() const;
	const FSlateBrush* GetHealthRowIconBrush() const;

	//~ Begin IDetailCustomization Interface
	void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};


class FSwitchboardProjectSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface
	void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};
