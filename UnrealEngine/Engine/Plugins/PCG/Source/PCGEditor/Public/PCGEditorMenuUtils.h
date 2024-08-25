// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ToolMenus.h"
#include "AssetRegistry/AssetData.h"

namespace PCGEditorMenuUtils
{
	PCGEDITOR_API FToolMenuSection& CreatePCGSection(UToolMenu* Menu);
	PCGEDITOR_API void CreateOrUpdatePCGAssetFromMenu(UToolMenu* Menu, TArray<FAssetData>& Assets);
}