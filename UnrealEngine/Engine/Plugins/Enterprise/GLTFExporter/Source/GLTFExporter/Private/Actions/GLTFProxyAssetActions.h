// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

struct FToolMenuSection;
struct FToolMenuContext;

class FGLTFProxyAssetActions
{
public:

	static FName MenuName;
	static FName SectionName;
	static FName EntryName;

	static void AddMenuEntry();
	static void RemoveMenuEntry();

private:

	static void OnConstructMenu(FToolMenuSection& MenuSection);
	static void OnExecuteAction(const FToolMenuContext& MenuContext);
};

#endif
