// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWorkspaceView.h"
#include "AnimNextWorkspace.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "SAssetDropTarget.h"
#include "ScopedTransaction.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "SWorkspaceView"

namespace UE::AnimNext::Editor
{

void SWorkspaceView::Construct(const FArguments& InArgs, UAnimNextWorkspace* InWorkspace)
{
	Workspace = InWorkspace;
	OnAssetsOpened = InArgs._OnAssetsOpened;

	UICommandList = MakeShared<FUICommandList>();

	UICommandList->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SWorkspaceView::HandleDelete),
		FCanExecuteAction::CreateSP(this, &SWorkspaceView::HasValidSelection));

	Workspace->ModifiedDelegate.AddSP(this, &SWorkspaceView::HandleWorkspaceModified);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.OnAssetsActivated = FOnAssetsActivated::CreateLambda([this](const TArray<FAssetData>& InAssets, EAssetTypeActivationMethod::Type InActivationMethod)
	{
		if( InActivationMethod == EAssetTypeActivationMethod::DoubleClicked ||
			InActivationMethod == EAssetTypeActivationMethod::Opened)
		{
			OnAssetsOpened.ExecuteIfBound(InAssets);
		}
	});
	AssetPickerConfig.RefreshAssetViewDelegates.Add(&RefreshAssetViewDelegate);
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.Filter = MakeARFilter();
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda([this](const FAssetData& InAsset)
	{
		return !Workspace->Assets.Contains(TSoftObjectPtr<UObject>(InAsset.GetSoftObjectPath()));
	});
	AssetPickerConfig.AssetShowWarningText = LOCTEXT("EmptyWorkspaceText", "Workspace is empty.\nDrag-drop to add assets to this workspace.");

	ChildSlot
	[
		SNew(SAssetDropTarget)
		.bSupportsMultiDrop(true)
		.OnAssetsDropped_Lambda([this](const FDragDropEvent& InEvent, TArrayView<FAssetData> InAssets)
		{
			FScopedTransaction Transaction(LOCTEXT("AddAssets", "Add assets to workspace"));

			Workspace->AddAssets(InAssets);
		})
		.OnAreAssetsAcceptableForDropWithReason_Lambda([](TArrayView<FAssetData> InAssets, FText& OutText)
		{
			for(const FAssetData& Asset : InAssets)
			{
				if(UAnimNextWorkspace::IsAssetSupported(Asset))
				{
					return true;
				}
			}

			OutText = LOCTEXT("AssetsUnsupportedInWorkspace", "Assets are not supported by this workspace");
			return false;
		})
		.Content()
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
	];
}

FARFilter SWorkspaceView::MakeARFilter()
{
	FARFilter Filter;
	Filter.ClassPaths = UAnimNextWorkspace::GetSupportedAssetClassPaths();
	Filter.bRecursiveClasses = true;

	return Filter;
}

FReply SWorkspaceView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList.IsValid() && UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SWorkspaceView::HandleDelete()
{
	if(GetCurrentSelectionDelegate.IsBound())
	{
		TArray<FAssetData> Selection = GetCurrentSelectionDelegate.Execute();

		if(Selection.Num() > 0)
		{
			FScopedTransaction Transaction(LOCTEXT("RemoveAssets", "Remove assets from workspace"));

			Workspace->RemoveAssets(Selection);
		}
	}
}

bool SWorkspaceView::HasValidSelection() const 
{
	if(GetCurrentSelectionDelegate.IsBound())
	{
		TArray<FAssetData> Selection = GetCurrentSelectionDelegate.Execute();
		return Selection.Num() > 0;
	}
	return false;
}

void SWorkspaceView::HandleWorkspaceModified(UAnimNextWorkspace* InWorkspace)
{
	check(InWorkspace == Workspace);

	RefreshAssetViewDelegate.ExecuteIfBound(true);
}

}

#undef LOCTEXT_NAMESPACE