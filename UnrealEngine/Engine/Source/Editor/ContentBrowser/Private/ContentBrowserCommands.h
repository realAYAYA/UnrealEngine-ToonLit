// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FUICommandInfo;

class FContentBrowserCommands
	: public TCommands<FContentBrowserCommands>
{
public:

	/** Default constructor. */
	FContentBrowserCommands()
		: TCommands<FContentBrowserCommands>(TEXT("ContentBrowser"), NSLOCTEXT( "ContentBrowser", "ContentBrowser", "Content Browser" ), NAME_None, FAppStyle::GetAppStyleSetName() )
	{ }

public:

	//~ TCommands interface

	virtual void RegisterCommands() override;

public:

	TSharedPtr<FUICommandInfo> CreateNewFolder;
	TSharedPtr<FUICommandInfo> OpenAssetsOrFolders;
	TSharedPtr<FUICommandInfo> PreviewAssets;
	TSharedPtr<FUICommandInfo> SaveSelectedAsset;
	TSharedPtr<FUICommandInfo> SaveAllCurrentFolder;
	TSharedPtr<FUICommandInfo> ResaveAllCurrentFolder;
	TSharedPtr<FUICommandInfo> CopySelectedAssetPath;
	TSharedPtr<FUICommandInfo> EditPath;
};
