// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class SActorEditorContext : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SActorEditorContext) {}
	SLATE_ARGUMENT(UWorld*, World)
	SLATE_END_ARGS()

	UNREALED_API void Construct(const FArguments& InArgs);
	UNREALED_API ~SActorEditorContext();

	static UNREALED_API bool IsVisible(UWorld* InWorld);
private:

	void OnEditorMapChange(uint32 MapChangeFlags = 0) { Rebuild(); }
	void Rebuild();
	
	bool bIsContextExpanded;
	UWorld* World;
};
