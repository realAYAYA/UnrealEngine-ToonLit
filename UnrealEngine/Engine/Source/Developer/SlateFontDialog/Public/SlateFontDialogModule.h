// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISlateFontDialogModule.h"

class FSlateFontDialogModule : public ISlateFontDialogModule
{
public:
	bool OpenFontDialog(FString& OutFontName, float& OutHeight, EFontImportFlags& OutFlags);
};
