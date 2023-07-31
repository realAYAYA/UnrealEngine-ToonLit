// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"
#include "DG.h"
#include "Utils/ShellOpenDocument.h"

BEGIN_NAMESPACE_UE_AC

class FPaletteWindow;

class FPalette
{
  public:
	// Palette data saved in preferences
	struct SPrefs
	{
		bool bShown;
		bool bHiddenByUser;
		bool bHiddenByAC;
		bool bIsDocked;
	};

	// Toggle visibility of palette
	static void ShowFromUser();

	// Switch to another window
	static void WindowChanged();

	// AutoSync status changed
	static void AutoSyncChanged();

	// Delete palette
	static void Delete();

	static void Register();

	static void Unregister();

	static Int32 PaletteRefId();

	static GS::Guid PaletteGuid;

  private:
	// Constructor
	FPalette();

	// Destructor
	~FPalette();

	// Create Palette and set CurrentPalette
	static void Create();

	// Save palette state to preferences
	void Save2Pref();

	short DlgCallBack(short message, short dialID, short item, DGMessageData MsgData);

	GSErrCode PaletteControlCallBack(Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param);

	void ShowHide(bool ByUserFromMenu, bool BeginHide);

	static void SetPaletteMenuTexts(bool PaletteIsOn, bool PaletteIsVisible);

	static GSErrCode __ACENV_CALL APIPaletteControlCallBack(Int32 referenceID, API_PaletteMessageID messageID,
															GS::IntPtr /*param*/);

	static short DGCALLBACK CntlDlgCallBack(short message, short dialID, short item, DGUserData userData,
											DGMessageData MsgData);

	static FPalette* CurrentPalette;
	FPaletteWindow*	 Window = nullptr;
};

END_NAMESPACE_UE_AC
