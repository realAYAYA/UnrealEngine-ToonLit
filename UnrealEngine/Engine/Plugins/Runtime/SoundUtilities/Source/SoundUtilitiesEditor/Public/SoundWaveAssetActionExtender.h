// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FSoundWaveAssetActionExtender
{
public:
	static void RegisterMenus();
	static void GetExtendedActions(const struct FToolMenuContext& MenuContext);
	static void ExecuteCreateSimpleSound(const struct FToolMenuContext& MenuContext);
};

