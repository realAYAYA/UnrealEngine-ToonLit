// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWorkspacePicker.h"
#include "AnimNextWorkspace.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "SPrimaryButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SWorkspacePicker"

namespace UE::AnimNext::Editor
{

void SWorkspacePicker::Construct(const FArguments& InArgs)
{
	WorkspaceAssets = InArgs._WorkspaceAssets;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this, OnAssetSelected = InArgs._OnAssetSelected](const FAssetData& InAssetData)
	{
		OnAssetSelected.ExecuteIfBound(InAssetData);

		if(TSharedPtr<SWindow> Window = WeakWindow.Pin())
		{
			Window->RequestDestroyWindow();
		}
	});
	AssetPickerConfig.Filter.ClassPaths.Add(UAnimNextWorkspace::StaticClass()->GetClassPathName());
	AssetPickerConfig.Filter.bRecursiveClasses = true;
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda([this](const FAssetData& InAssetData)
	{
		return !WorkspaceAssets.IsEmpty() && !WorkspaceAssets.Contains(InAssetData);
	});

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(400.0f)
		.HeightOverride(400.0f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.0f)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ChooseExistingWorkspaceToDetails", "Selected asset is part of multiple workspaces.\nPlease select the workspace you want to open the asset with"))
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(5.0f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.0f)
			.HAlign(HAlign_Fill)
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("CreateNewWorkspace", "Create New Workspace"))
				.OnClicked_Lambda([this, OnNewAsset = InArgs._OnNewAsset]()
				{
					OnNewAsset.ExecuteIfBound();

					if(TSharedPtr<SWindow> Window = WeakWindow.Pin())
					{
						Window->RequestDestroyWindow();
					}
					return FReply::Handled();
				})
			]
		]
	];
}

void SWorkspacePicker::ShowModal()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
	.Title(LOCTEXT("ChooseExistingWorkspaceToOpen", "Choose Existing Workspace to Open"))
	.SizingRule(ESizingRule::Autosized)
	.SupportsMaximize(false)
	.SupportsMinimize(false)
	[
		AsShared()
	];

	WeakWindow = Window;

	FSlateApplication::Get().AddModalWindow(Window, FGlobalTabmanager::Get()->GetRootWindow());
}

}

#undef LOCTEXT_NAMESPACE