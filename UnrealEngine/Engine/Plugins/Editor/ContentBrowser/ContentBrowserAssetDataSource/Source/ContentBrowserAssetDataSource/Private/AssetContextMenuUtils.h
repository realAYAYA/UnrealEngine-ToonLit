// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/UIAction.h"

struct FToolMenuEntry;
struct FToolMenuSection;
struct FToolUIActionChoice;

class UToolMenu;
template<typename ObjectType> class TAttribute;
template<typename FuncType> class TFunction;

namespace UE::ContentBrowserAssetDataSource::Private
{
	DECLARE_DELEGATE_RetVal(bool, FIsAsyncProcessingActive);

	FToolMenuEntry& AddAsyncMenuEntry(
		FToolMenuSection& Section,
		FName Name,
		const TAttribute<FText>& Label,
		const TAttribute<FText>& ToolTip,
		const FToolUIActionChoice& InAction,
		const FIsAsyncProcessingActive& IsAsyncProcessingActive
	);
}