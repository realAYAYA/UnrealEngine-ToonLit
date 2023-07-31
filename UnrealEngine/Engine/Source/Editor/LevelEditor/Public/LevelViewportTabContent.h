// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LevelViewportLayout.h"
#include "EditorViewportTabContent.h"

class ILevelEditor;
class FEditorViewportLayout;

/**
 * Represents the content in a viewport tab in the level editor.
 * Each SDockTab holding viewports in the level editor contains and owns one of these.
 */
class LEVELEDITOR_API FLevelViewportTabContent : public FEditorViewportTabContent
{
public:
	~FLevelViewportTabContent();

	/** Starts the tab content object and creates the initial layout based on the layout string */
	virtual void Initialize(AssetEditorViewportFactoryFunction Func, TSharedPtr<SDockTab> InParentTab, const FString& InLayoutString) override;

	virtual void BindViewportLayoutCommands(FUICommandList& InOutCommandList, FName ViewportConfigKey) override;

protected:
	virtual TSharedPtr<FEditorViewportLayout> FactoryViewportLayout(bool bIsSwitchingLayouts) override;
	virtual FName GetLayoutTypeNameFromLayoutString() const override;

	void OnLayoutStartChange(bool bSwitchingLayouts);
	void OnLayoutChanged();

private:
	void OnUIActionSetViewportConfiguration(FName InConfigurationName);
	FName GetViewportTypeWithinLayout(FName InConfigKey) const;
	void OnUIActionSetViewportTypeWithinLayout(FName InConfigKey, FName InLayoutType);
	bool IsViewportTypeWithinLayoutEqual(FName InConfigName, FName InLayoutType) const;
	bool IsViewportConfigurationChecked(FName InLayoutType) const;
};
