// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaEditorExtension.h"

class URemoteControlTrackerComponent;
class URemoteControlPreset;

class FAvaRCExtension: public FAvaEditorExtension
{
public:
	UE_AVA_INHERITS(FAvaRCExtension, FAvaEditorExtension);

	virtual ~FAvaRCExtension() override = default;

	URemoteControlPreset* GetRemoteControlPreset() const;

	//~ Begin IAvaEditorExtension
	virtual void Activate() override;
	virtual void Deactivate() override;
	virtual void ExtendLevelEditorLayout(FLayoutExtender& InExtender) const override;
	virtual void ExtendToolbarMenu(UToolMenu& InMenu) override;
	//~ End IAvaEditorExtension

protected:
	void OpenRemoteControlTab() const;

	void CloseRemoteControlTab() const;
};
