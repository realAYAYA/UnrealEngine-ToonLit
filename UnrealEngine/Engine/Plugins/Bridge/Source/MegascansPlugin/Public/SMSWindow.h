// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once


#include "CoreMinimal.h"
#include "MSSettings.h"

#include "Widgets/SWindow.h"

class FTabManager;

class MEGASCANSPLUGIN_API MegascansSettingsWindow
{
public:
	
	
	static void OpenSettingsWindow(/*const TSharedRef<FTabManager>& TabManager*/);
	static void SaveSettings(const TSharedRef<SWindow>& Window, UMegascansSettings* MegascansSettings);	
	
	
};




