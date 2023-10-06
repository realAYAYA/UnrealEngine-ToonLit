// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVPBookmarkEditor, Log, Log);


class FVPBookmarkTypeActions;


class FVPBookmarkEditorModule : public IModuleInterface
{
public:
	TSharedPtr<FVPBookmarkTypeActions> BookmarkTypeActions;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
