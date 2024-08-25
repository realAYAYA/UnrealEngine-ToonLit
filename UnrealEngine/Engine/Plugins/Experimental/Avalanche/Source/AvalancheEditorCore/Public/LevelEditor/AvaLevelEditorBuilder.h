// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaEditorBuilder.h"

class AVALANCHEEDITORCORE_API FAvaLevelEditorBuilder : public FAvaEditorBuilder
{
protected:
	//~ Begin FAvaEditorBuilder
	virtual TSharedRef<IAvaEditor> CreateEditor() override;
	//~ End FAvaEditorBuilder
};
