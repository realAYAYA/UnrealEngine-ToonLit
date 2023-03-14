// Copyright Epic Games, Inc. All Rights Reserved.

#include "Menus.h"
#include "ResourcesIDs.h"
#include "Utils/Error.h"
#include "Commander.h"

BEGIN_NAMESPACE_UE_AC

// Add menu to the menu bar and also add an item to palette menu
inline static void RegisterMenu(GSErrCode* IOGSErr, short InMenuResId, short InMenuHelpResId, APIMenuCodeID InMenuCode,
								GSFlags InMenuFlags)
{
	if (*IOGSErr == NoError)
	{
		*IOGSErr =
			ACAPI_Register_Menu(LocalizeResId(InMenuResId), LocalizeResId(InMenuHelpResId), InMenuCode, InMenuFlags);
	}
}

#define RegisterMenuAndHelp(IOGSErr, MenuId, MenuCode, MenuFlags) \
	RegisterMenu(IOGSErr, MenuId, MenuId##Help, MenuCode, MenuFlags)

// Add menu to the menu bar and also add an item to palette menu
GSErrCode FMenus::Register()
{
	GSErrCode GSErr = NoError;

	RegisterMenuAndHelp(&GSErr, kStrListMenuDatasmith, MenuCode_Palettes, MenuFlag_Default);

#if !UE_AC_NO_MENU
	RegisterMenuAndHelp(&GSErr, kStrListMenuItemSnapshot, MenuCode_UserDef, MenuFlag_Default);
	#if AUTO_SYNC
	RegisterMenuAndHelp(&GSErr, kStrListMenuItemAutoSync, MenuCode_UserDef, MenuFlag_Default);
	#endif
	RegisterMenuAndHelp(&GSErr, kStrListMenuItemConnections, MenuCode_UserDef, MenuFlag_Default);
	RegisterMenuAndHelp(&GSErr, kStrListMenuItemExport, MenuCode_UserDef, MenuFlag_Default);
	RegisterMenuAndHelp(&GSErr, kStrListMenuItemMessages, MenuCode_UserDef, MenuFlag_Default);
	RegisterMenuAndHelp(&GSErr, kStrListMenuItemPalette, MenuCode_UserDef, MenuFlag_SeparatorBefore);
	RegisterMenuAndHelp(&GSErr, kStrListMenuItemAbout, MenuCode_UserDef, MenuFlag_Default);
#endif

	return GSErr;
}

// Enable handlers of menu items
GSErrCode FMenus::Initialize()
{
	GSErrCode GSErr = NoError;

#if !UE_AC_NO_MENU
	for (short IndexMenu = kStrListMenuItemSnapshot; IndexMenu <= kStrListMenuItemAbout && GSErr == NoError;
		 IndexMenu++)
	{
		GSErr = ACAPI_Install_MenuHandler(LocalizeResId(IndexMenu), MenuCommandHandler);
	}
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FMenus::Initialize - ACAPI_Install_MenuHandler error=%s\n", GetErrorName(GSErr));
	}
#endif

	GSErr = ACAPI_Install_MenuHandler(LocalizeResId(kStrListMenuDatasmith), MenuCommandHandler);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FMenus::Initialize - ACAPI_Install_MenuHandler error=%s\n", GetErrorName(GSErr));
	}

	return GSErr;
}

// Ename or disable menu item
void FMenus::SetMenuItemStatus(short InMenu, short InItem, bool InSet, GSFlags InFlag)
{
#if !UE_AC_NO_MENU
	API_MenuItemRef ItemRef;
	Zap(&ItemRef);
	ItemRef.menuResID = LocalizeResId(InMenu);
	ItemRef.itemIndex = InItem;
	GSFlags	  ItemFlags = 0;
	GSErrCode GSErr = ACAPI_Interface(APIIo_GetMenuItemFlagsID, &ItemRef, &ItemFlags);
	if (GSErr == NoError)
	{
		if (InSet)
		{
			ItemFlags |= InFlag;
		}
		else
		{
			ItemFlags &= ~InFlag;
		}
		GSErr = ACAPI_Interface(APIIo_SetMenuItemFlagsID, &ItemRef, &ItemFlags);
		if (GSErr != NoError)
		{
			UE_AC_DebugF("FMenus::SetMenuItemStatus - APIIo_SetMenuItemFlagsID error=%s\n", GetErrorName(GSErr));
		}
	}
	else
	{
		UE_AC_DebugF("FMenus::SetMenuItemStatus - APIIo_GetMenuItemFlagsID error=%s\n", GetErrorName(GSErr));
	}
#else
	(void)InMenu; // No unused warnings
	(void)InItem;
	(void)InSet;
	(void)InFlag;
#endif
}

// Change the text of an item
void FMenus::SetMenuItemText(short InMenu, short InItem, const GS::UniString& ItemStr)
{
#if !UE_AC_NO_MENU
	API_MenuItemRef ItemRef;
	Zap(&ItemRef);
	ItemRef.menuResID = LocalizeResId(InMenu);
	ItemRef.itemIndex = InItem;
	GSErrCode GSErr =
		ACAPI_Interface(APIIo_SetMenuItemTextID, &ItemRef, nullptr, const_cast< GS::UniString* >(&ItemStr));
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FMenus::SetMenuItemText - APIIo_SetMenuItemTextID error=%s\n", GetErrorName(GSErr));
	}
#else
	(void)InMenu; // No unused warnings
	(void)InItem;
	(void)ItemStr;
#endif
}

// AutoSync status changed
void FMenus::AutoSyncChanged()
{
	static const GS::UniString StartAutoSync(GetGSName(kName_StartAutoSync));
	static const GS::UniString PauseAutoSync(GetGSName(kName_PauseAutoSync));
#if AUTO_SYNC
	SetMenuItemText(kStrListMenuItemAutoSync, 1, FCommander::IsAutoSyncEnabled() ? PauseAutoSync : StartAutoSync);
	SetMenuItemStatus(kStrListMenuItemAutoSync, 1, FCommander::IsAutoSyncEnabled(), API_MenuItemChecked);
#endif
}

// Menu command handler
GSErrCode __ACENV_CALL FMenus::MenuCommandHandler(const API_MenuParams* MenuParams) noexcept
{
	return TryFunctionCatchAndAlert("FMenus::DoMenuCommand",
									[&MenuParams]() -> GSErrCode { return FMenus::DoMenuCommand(*MenuParams); });
}

// Process menu command
GSErrCode FMenus::DoMenuCommand(const API_MenuParams& MenuParams)
{
	short MenuId = MenuParams.menuItemRef.menuResID - LocalizeResId(0);
	if (MenuParams.menuItemRef.itemIndex != 1)
	{
		UE_AC_DebugF("FMenus::DoMenuCommand - Menu %d, Item is %d\n", MenuId, MenuParams.menuItemRef.itemIndex);
	}

	switch (MenuId)
	{
		case kStrListMenuItemSnapshot:
			FCommander::DoSnapshot();
			break;
#if AUTO_SYNC
		case kStrListMenuItemAutoSync:
			FCommander::ToggleAutoSync();
			break;
#endif
		case kStrListMenuItemConnections:
			FCommander::ShowConnectionsDialog();
			break;
		case kStrListMenuItemExport:
			FCommander::Export3DToFile();
			break;
		case kStrListMenuItemMessages:
			FCommander::ShowMessagesDialog();
			break;
		case kStrListMenuItemPalette:
			FCommander::ShowHidePalette();
			break;
		case kStrListMenuItemAbout:
			FCommander::ShowAboutOf();
			break;
		case kStrListMenuDatasmith:
			FCommander::ShowHidePalette();
			break;
	}
	return GS::NoError;
}

END_NAMESPACE_UE_AC
