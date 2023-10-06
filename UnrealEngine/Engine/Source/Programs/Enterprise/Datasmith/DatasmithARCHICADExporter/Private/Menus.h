// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

BEGIN_NAMESPACE_UE_AC

#define UE_AC_NO_MENU 1 // 1 = Dont create menu

class FMenus
{
  public:
	// Add menu to the menu bar and also add an item to palette menu
	static GSErrCode Register();

	// Enable handlers of menu items
	static GSErrCode Initialize();

	// Ename or disable menu item
	static void SetMenuItemStatus(short InMenu, short InItem, bool InSet, GSFlags InFlag);

	// Change the text of an item
	static void SetMenuItemText(short InMenu, short InItem, const GS::UniString& ItemStr);

	// AutoSync status changed
	static void AutoSyncChanged();

  private:
	// Menu command handler
	static GSErrCode __ACENV_CALL MenuCommandHandler(const API_MenuParams* MenuParams) noexcept;

	// Process menu command
	static GSErrCode DoMenuCommand(const API_MenuParams& InMenuParams);
};

END_NAMESPACE_UE_AC
