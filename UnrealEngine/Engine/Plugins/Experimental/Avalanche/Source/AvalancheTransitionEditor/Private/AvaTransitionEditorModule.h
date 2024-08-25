// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaTransitionEditorModule.h"

class UAvaTransitionTree;
class UStateTree;

class FAvaTransitionEditorModule : public IAvaTransitionEditorModule
{
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	//~ Begin IAvaTransitionEditorModule
	virtual FOnBuildDefaultTransitionTree& GetOnBuildDefaultTransitionTree() override;
	virtual FOnCompileTransitionTree& GetOnCompileTransitionTree() override;
	virtual void GenerateTransitionTreeOptionsMenu(UToolMenu* InMenu, IAvaTransitionBehavior* InTransitionBehavior) override;
	//~ End IAvaTransitionEditorModule

	void ValidateStateTree(UAvaTransitionTree* InTransitionTree);

	void OnPostCompile(const UStateTree& InStateTree);

	FOnBuildDefaultTransitionTree OnBuildDefaultTransitionTree;

	FOnCompileTransitionTree OnCompileTransitionTree;

	FDelegateHandle OnPostCompileHandle;
};
