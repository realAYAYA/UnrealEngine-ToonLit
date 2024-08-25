// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPhysicsControlOperatorEditorInterface.h"
#include "Containers/Ticker.h"

class FSpawnTabArgs;
class SOperatorEditorTabWidget;
class SDockTab;

class FPhysicsControlOperatorEditor : public IPhysicsControlOperatorEditorInterface
{
public:

	virtual ~FPhysicsControlOperatorEditor() {}

	virtual void OpenOperatorNamesTab() override;
	virtual void CloseOperatorNamesTab() override;
	virtual void ToggleOperatorNamesTab() override;
	virtual bool IsOperatorNamesTabOpen() override;
	virtual void RequestRefresh() override;

	void Startup();
	void Shutdown();

	void OnTabClosed(TSharedRef<SDockTab> DockTab);

private:
	TSharedRef<SDockTab> OnCreateTab(const FSpawnTabArgs& SpawnTabArgs);

	TSharedPtr<SOperatorEditorTabWidget> PersistantTabWidget;
	TSharedPtr<SDockTab> OperatorNamesTab;
};