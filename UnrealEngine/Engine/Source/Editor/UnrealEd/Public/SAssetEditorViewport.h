// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "SEditorViewport.h"

class FEditorViewportClient;
class FViewportTabContent;
class FMenuBuilder;
struct FAssetEditorViewportConstructionArgs;

class UNREALED_API SAssetEditorViewport : public SEditorViewport
{
public:

	SLATE_BEGIN_ARGS(SAssetEditorViewport)
		{
		}

		SLATE_ATTRIBUTE(FVector2D, ViewportSize);
		SLATE_ARGUMENT(TSharedPtr<FEditorViewportClient>, EditorViewportClient)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs);
	void OnSetViewportConfiguration(FName ConfigurationName);
	bool IsViewportConfigurationSet(FName ConfigurationName) const;

	void GenerateLayoutMenu(FMenuBuilder& MenuBuilder) const;

protected:
	virtual void BindCommands() override;

	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	FName ConfigKey;
	TWeakPtr<FViewportTabContent> ParentTabContent;
};
