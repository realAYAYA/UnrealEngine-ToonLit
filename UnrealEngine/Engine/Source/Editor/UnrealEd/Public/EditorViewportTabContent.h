// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Docking/SDockTab.h"
#include "ViewportTabContent.h"
#include "AssetEditorViewportLayout.h"

class FEditorViewportLayout;
class SDockTab;

/**
 * Represents the content in a viewport tab in an editor.
 * Each SDockTab holding viewports in an editor contains and owns one of these.
 */
class UNREALED_API FEditorViewportTabContent : public FViewportTabContent, public TSharedFromThis<FEditorViewportTabContent>
{
public:
	/** @return The string used to identify the layout of this viewport tab */
	const FString& GetLayoutString() const
	{
		return LayoutString;
	}
	
	const TSharedPtr<FEditorViewportLayout> GetActiveViewportLayout() const
	{
		return ActiveViewportLayout;
	}
	   
	TSharedPtr<FEditorViewportLayout> ConstructViewportLayoutByTypeName(const FName& TypeName, bool bSwitchingLayouts);

	virtual void Initialize(AssetEditorViewportFactoryFunction Func, TSharedPtr<SDockTab> InParentTab, const FString& InLayoutString);

	TSharedPtr<SAssetEditorViewport> CreateSlateViewport(FName InTypeName, const FAssetEditorViewportConstructionArgs& ConstructionArgs) const;

	/**
	* Sets the current layout by changing the contained layout object
	* 
	* @param ConfigurationName		The name of the layout (for the names in namespace EditorViewportConfigurationNames)
	*/
	void SetViewportConfiguration(const FName& ConfigurationName) override;

	TSharedPtr<SEditorViewport> GetFirstViewport();

	void UpdateViewportTabWidget();

	/** Save any configuration required to persist state for this viewport layout */
	void SaveConfig() const;

	/**
	 * Refresh the current layout.
	 * @note Does not save config
	 */
	void RefreshViewportConfiguration();

	/**
	 * Attempt to find a viewport creation factory function in the set of registered factory functions.
	 * 
	 * @param InTypeName The name of the viewport creation factory to find.
	 * @returns the creation function if one is found with the given typename, otherwise nullptr.
	 */
	const AssetEditorViewportFactoryFunction* FindViewportCreationFactory(FName InTypeName) const;

protected:
	virtual TSharedPtr<FEditorViewportLayout> FactoryViewportLayout(bool bIsSwitchingLayouts);
	virtual FName GetLayoutTypeNameFromLayoutString() const;

private:
	using AssetEditorViewportCreationFactories = TMap<FName, AssetEditorViewportFactoryFunction>;
	AssetEditorViewportCreationFactories ViewportCreationFactories;
};
