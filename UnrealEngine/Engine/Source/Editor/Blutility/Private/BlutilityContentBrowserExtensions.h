// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Integrate Blutility actions associated with existing engine types (e.g., Texture2D) into the content browser
class FBlutilityContentBrowserExtensions
{
public:
	static void InstallHooks();
	static void RegisterMenus();
	static void RemoveHooks();
};
