// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCommonEditorViewportToolbarBase.h"

/// Common code for toolbars in the Cloth Editor
class CHAOSCLOTHASSETEDITOR_API SChaosClothAssetEditorViewportToolBarBase : public SCommonEditorViewportToolbarBase
{
protected:
	TSharedRef<SWidget> GenerateClothViewportOptionsMenu() const;
	TSharedRef<SWidget> GenerateFOVMenu() const;
	float OnGetFOVValue() const;
};

