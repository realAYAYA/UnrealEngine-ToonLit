// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTabSpawner.h"

class FAvaSequencerTabSpawner : public FAvaTabSpawner
{
public:
	static FName GetTabID();

	explicit FAvaSequencerTabSpawner(const TSharedRef<IAvaEditor>& InEditor, FName InTabId, bool bInIsDrawerTab = false);

	//~ Begin IAvaTabSpawner
	virtual TSharedRef<SWidget> CreateTabBody() override;
	//~ End IAvaTabSpawner

	TSharedRef<SWidget> CreateDrawerDockButton(TSharedRef<IAvaEditor> InEditor) const;

private:
	bool bIsDrawerTab;
};
