// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/FloatingPropertiesWidgetController.h"
#include "Containers/Ticker.h"

class ILevelEditor;

class FFloatingPropertiesLevelEditorWidgetController : public FFloatingPropertiesWidgetController
{
public:
	static void StaticInit();

	FFloatingPropertiesLevelEditorWidgetController(TSharedRef<ILevelEditor> InLevelEditor);
	virtual ~FFloatingPropertiesLevelEditorWidgetController();

	//~ Begin FFloatingPropertiesWidgetController
	virtual void Init() override;
	//~ End FFloatingPropertiesWidgetController

protected:
	static void RegisterLevelViewportMenuExtensions();

	static void RegisterLevelEditorCommands();

	FTSTicker::FDelegateHandle NextTickTimer;

	void RegisterLevelViewportClientsChanged();

	void UnregisterLevelViewportClientsChanged();

	void OnLevelViewportClientsChanged();
};
