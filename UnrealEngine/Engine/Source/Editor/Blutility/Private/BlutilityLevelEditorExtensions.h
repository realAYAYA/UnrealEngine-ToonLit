// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Integrate Blutility actions associated with level editor functionality (e.g. Actor editing)
class FBlutilityLevelEditorExtensions
{
public:
	static void InstallHooks();
	static void RegisterMenus();
	static void RemoveHooks();
};
