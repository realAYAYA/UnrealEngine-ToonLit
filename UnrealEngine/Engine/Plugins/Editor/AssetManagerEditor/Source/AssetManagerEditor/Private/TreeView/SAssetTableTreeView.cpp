// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAssetTableTreeView.h"

#include "AssetDefinitionRegistry.h"
#include "AssetManagerEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Set.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/AssetManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "IContentBrowserSingleton.h"
#include "Insights/Common/InsightsStyle.h"
#include "Insights/Common/Log.h"
#include "Insights/Common/Stopwatch.h"
#include "Insights/Filter/ViewModels/FilterConfigurator.h"
#include "Insights/Table/ViewModels/TableCellValueFormatter.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/Table/ViewModels/TreeNodeGrouping.h"
#include "Interfaces/IPluginManager.h"
#include "Logging/MessageLog.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "TreeView/AssetTable.h"
#include "TreeView/AssetTreeNode.h"
#include "TreeView/AssetDependencyGrouping.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SToolTip.h"

#include <limits>
#include <memory>

#define LOCTEXT_NAMESPACE "SAssetTableTreeView"

extern UNREALED_API UEditorEngine* GEditor;

////////////////////////////////////////////////////////////////////////////////////////////////////

SAssetTableTreeView::SAssetTableTreeView()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

SAssetTableTreeView::~SAssetTableTreeView()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::Construct(const FArguments& InArgs, TSharedPtr<FAssetTable> InTablePtr)
{
	this->OnSelectionChanged = InArgs._OnSelectionChanged;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistry = &AssetRegistryModule.Get();
	AssetManager = &UAssetManager::Get();
	EditorModule = &IAssetManagerEditorModule::Get();

	STableTreeView::ConstructWidget(InTablePtr);

	CreateGroupings();
	CreateSortings();

	RegistrySourceTimeText->SetText(LOCTEXT("RegistrySourceTimeText_None", "No registry loaded."));

	RequestOpenRegistry();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::Reset()
{
	//...
	UE::Insights::STableTreeView::Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UE::Insights::STableTreeView::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!bIsUpdateRunning)
	{
		if (bNeedsToOpenRegistry)
		{
			bNeedsToOpenRegistry = false;
			OpenRegistry();
		}
		if (bNeedsToRefreshAssets && !AssetRegistry->IsLoadingAssets())
		{
			bNeedsToRefreshAssets = false;
			RefreshAssets();
		}
		if (bNeedsToRebuild)
		{
			bNeedsToRebuild = false;
			RebuildTree(true);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SAssetTableTreeView::IsRunning() const
{
	return STableTreeView::IsRunning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

double SAssetTableTreeView::GetAllOperationsDuration()
{
	return STableTreeView::GetAllOperationsDuration();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SAssetTableTreeView::GetCurrentOperationName() const
{
	return STableTreeView::GetCurrentOperationName();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAssetTableTreeView::ConstructHeaderArea(TSharedRef<SVerticalBox> InWidgetContent)
{
	InWidgetContent->AddSlot()
		.VAlign(VAlign_Center)
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(this, &SAssetTableTreeView::GetOpenRegistryToolTipText)
				.IsEnabled(this, &SAssetTableTreeView::CanChangeRegistry)
				.OnClicked(this, &SAssetTableTreeView::OnClickedOpenRegistry)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.FolderOpen"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				ConstructSearchBox()
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				ConstructFilterConfiguratorButton()
			]
		];

	InWidgetContent->AddSlot()
		.VAlign(VAlign_Center)
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(RegistrySourceTimeText, STextBlock)
			]
		];

	InWidgetContent->AddSlot()
		.VAlign(VAlign_Center)
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				ConstructHierarchyBreadcrumbTrail()
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Preset", "Preset:"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(150.0f)
				[
					SAssignNew(PresetComboBox, SComboBox<TSharedRef<UE::Insights::ITableTreeViewPreset>>)
					.ToolTipText(this, &SAssetTableTreeView::ViewPreset_GetSelectedToolTipText)
					.OptionsSource(GetAvailableViewPresets())
					.OnSelectionChanged(this, &SAssetTableTreeView::ViewPreset_OnSelectionChanged)
					.OnGenerateWidget(this, &SAssetTableTreeView::ViewPreset_OnGenerateWidget)
					[
						SNew(STextBlock)
						.Text(this, &SAssetTableTreeView::ViewPreset_GetSelectedText)
					]
				]
			]
		];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::InitAvailableViewPresets()
{
	//////////////////////////////////////////////////
	// Default View

	//class FDefaultViewPreset : public UE::Insights::ITableTreeViewPreset
	//{
	//public:
	//	virtual FText GetName() const override
	//	{
	//		return LOCTEXT("Default_PresetName", "Default");
	//	}
	//	virtual FText GetToolTip() const override
	//	{
	//		return LOCTEXT("Default_PresetToolTip", "Default View\nConfigure the tree view to show default asset info.");
	//	}
	//	virtual FName GetSortColumn() const override
	//	{
	//		return UE::Insights::FTable::GetHierarchyColumnId();
	//	}
	//	virtual EColumnSortMode::Type GetSortMode() const override
	//	{
	//		return EColumnSortMode::Type::Ascending;
	//	}
	//	virtual void SetCurrentGroupings(const TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InOutCurrentGroupings) const override
	//	{
	//		InOutCurrentGroupings.Reset();

	//		check(InAvailableGroupings[0]->Is<UE::Insights::FTreeNodeGroupingFlat>());
	//		InOutCurrentGroupings.Add(InAvailableGroupings[0]);
	//	}
	//	virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
	//	{
	//		InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                         !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                   true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeColumnId,          true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,  !true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,  !true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId,!true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,               true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
	//	}
	//};
	//AvailableViewPresets.Add(MakeShared<FDefaultViewPreset>());

	////////////////////////////////////////////////////
	//// GameFeaturePlugin, Type, Dependency View
	//
	//class FGameFeaturePluginTypeDependencyView : public UE::Insights::ITableTreeViewPreset
	//{
	//public:
	//	virtual FText GetName() const override
	//	{
	//		return LOCTEXT("GFPTypeDepView_PresetName", "Dependency Analysis");
	//	}

	//	virtual FText GetToolTip() const override
	//	{
	//		return LOCTEXT("GFPTypeDepView_PresetToolTip", "Dependency Analysis View\nConfigure the tree view to show a breakdown of assets by Game Feature Plugin, Type, and Dependencies.");
	//	}
	//	virtual FName GetSortColumn() const override
	//	{
	//		return UE::Insights::FTable::GetHierarchyColumnId();
	//	}
	//	virtual EColumnSortMode::Type GetSortMode() const override
	//	{
	//		return EColumnSortMode::Type::Ascending;
	//	}
	//	virtual void SetCurrentGroupings(const TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InOutCurrentGroupings) const override
	//	{
	//		InOutCurrentGroupings.Reset();

	//		check(InAvailableGroupings[0]->Is<UE::Insights::FTreeNodeGroupingFlat>());
	//		InOutCurrentGroupings.Add(InAvailableGroupings[0]);

	//		const TSharedPtr<UE::Insights::FTreeNodeGrouping>* GameFeaturePluginGrouping = InAvailableGroupings.FindByPredicate(
	//			[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
	//			{
	//				return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
	//					Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::PluginNameColumnId;
	//			});
	//		if (GameFeaturePluginGrouping)
	//		{
	//			InOutCurrentGroupings.Add(*GameFeaturePluginGrouping);
	//		}

	//		const TSharedPtr<UE::Insights::FTreeNodeGrouping>* PrimaryTypeGrouping = InAvailableGroupings.FindByPredicate(
	//			[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
	//			{
	//				return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
	//					Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::TypeColumnId;
	//			});
	//		if (PrimaryTypeGrouping)
	//		{
	//			InOutCurrentGroupings.Add(*PrimaryTypeGrouping);
	//		}

	//		const TSharedPtr<UE::Insights::FTreeNodeGrouping>* DependencyGrouping = InAvailableGroupings.FindByPredicate(
	//			[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
	//			{
	//				return Grouping->Is<FAssetDependencyGrouping>();
	//			});
	//		if (DependencyGrouping)
	//		{
	//			InOutCurrentGroupings.Add(*DependencyGrouping);
	//		}
	//	}
	//	virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
	//	{
	//		InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                         !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                  !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeColumnId,          true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId,!true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,              !true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
	//	}
	//};
	//AvailableViewPresets.Add(MakeShared<FGameFeaturePluginTypeDependencyView>());

	////////////////////////////////////////////////////
	//// Path Breakdown View

	//class FAssetPathViewPreset : public UE::Insights::ITableTreeViewPreset
	//{
	//public:
	//	virtual FText GetName() const override
	//	{
	//		return LOCTEXT("Path_PresetName", "Path");
	//	}
	//	virtual FText GetToolTip() const override
	//	{
	//		return LOCTEXT("Path_PresetToolTip", "Path Breakdown View\nConfigure the tree view to show a breakdown of assets by their path.");
	//	}
	//	virtual FName GetSortColumn() const override
	//	{
	//		return UE::Insights::FTable::GetHierarchyColumnId();
	//	}
	//	virtual EColumnSortMode::Type GetSortMode() const override
	//	{
	//		return EColumnSortMode::Type::Ascending;
	//	}
	//	virtual void SetCurrentGroupings(const TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InOutCurrentGroupings) const override
	//	{
	//		InOutCurrentGroupings.Reset();

	//		check(InAvailableGroupings[0]->Is<UE::Insights::FTreeNodeGroupingFlat>());
	//		InOutCurrentGroupings.Add(InAvailableGroupings[0]);

	//		const TSharedPtr<UE::Insights::FTreeNodeGrouping>* PathGrouping = InAvailableGroupings.FindByPredicate(
	//			[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
	//			{
	//				return Grouping->Is<UE::Insights::FTreeNodeGroupingByPathBreakdown>() &&
	//					   Grouping->As<UE::Insights::FTreeNodeGroupingByPathBreakdown>().GetColumnId() == FAssetTableColumns::PathColumnId;
	//			});
	//		if (PathGrouping)
	//		{
	//			InOutCurrentGroupings.Add(*PathGrouping);
	//		}
	//	}
	//	virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
	//	{
	//		InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                         !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                   true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeColumnId,          true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId,!true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,               true, 100.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
	//		InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
	//	}
	//};
	//AvailableViewPresets.Add(MakeShared<FAssetPathViewPreset>());

	////////////////////////////////////////////////////
	//// Primary Type Breakdown View

	class FPrimaryTypeViewPreset : public UE::Insights::ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("PrimaryType_PresetName", "Group By Primary Type");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("PrimaryType_PresetToolTip", "Primary Type Breakdown View\nConfigure the tree view to show a breakdown of assets by their primary type.");
		}
		virtual FName GetSortColumn() const override
		{
			return FAssetTableColumns::StagedCompressedSizeColumnId;
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Descending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<UE::Insights::FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* PrimaryTypeGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::PrimaryTypeColumnId;
				});
			if (PrimaryTypeGrouping)
			{
				InOutCurrentGroupings.Add(*PrimaryTypeGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId, true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,               true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FPrimaryTypeViewPreset>());


	////////////////////////////////////////////////////
	//// Class Type Breakdown View

	class FClassTypeViewPreset : public UE::Insights::ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("ClassType_PresetName", "Group By Class Type");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("ClassType_PresetToolTip", "Class Type Breakdown View\nConfigure the tree view to show a breakdown of assets by their class type.");
		}
		virtual FName GetSortColumn() const override
		{
			return FAssetTableColumns::StagedCompressedSizeColumnId;
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Descending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<UE::Insights::FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* PrimaryTypeGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::NativeClassColumnId;
				});
			if (PrimaryTypeGrouping)
			{
				InOutCurrentGroupings.Add(*PrimaryTypeGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId, true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,               true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FClassTypeViewPreset>());

	////////////////////////////////////////////////////
	//// Asset Type Breakdown View

	class FAssetTypeViewPreset : public UE::Insights::ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("AssetType_PresetName", "Group By Asset Type");
		}
		virtual FText GetToolTip() const override
		{
			return LOCTEXT("AssetType_PresetToolTip", "Asset Type Breakdown View\nConfigure the tree view to show a breakdown of assets by their asset type.");
		}
		virtual FName GetSortColumn() const override
		{
			return FAssetTableColumns::StagedCompressedSizeColumnId;
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Descending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<UE::Insights::FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* PrimaryTypeGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::TypeColumnId;
				});
			if (PrimaryTypeGrouping)
			{
				InOutCurrentGroupings.Add(*PrimaryTypeGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId, true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,               true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FAssetTypeViewPreset>());

	//////////////////////////////////////////////////
	// Plugin Group View

	class FPluginView : public UE::Insights::ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("PluginGroupView_PresetName", "Group By Plugin");
		}

		virtual FText GetToolTip() const override
		{
			return LOCTEXT("PluginGroupView_PresetToolTip", "Group By Plugin\nGroup assets by plugin.");
		}
		virtual FName GetSortColumn() const override
		{
			return FAssetTableColumns::StagedCompressedSizeColumnId;
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Descending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<UE::Insights::FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* PrimaryGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::PluginNameColumnId;
				});
			if (PrimaryGrouping)
			{
				InOutCurrentGroupings.Add(*PrimaryGrouping);
			}

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* SecondaryGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::TypeColumnId;
				});
			if (SecondaryGrouping)
			{
				InOutCurrentGroupings.Add(*SecondaryGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });	
			InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                         !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                  !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                  !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId,!true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,               true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                   !true, 200.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FPluginView>());

	//////////////////////////////////////////////////
	// Plugin Dependency View

	class FPluginDependencyView : public UE::Insights::ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("PluginDepView_PresetName", "Plugin Dependency Analysis");
		}

		virtual FText GetToolTip() const override
		{
			return LOCTEXT("PluginDepView_PresetToolTip", "Plugin Dependency Analysis View\nConfigure the tree view to show a breakdown of assets by Game Feature Plugin, showing also the dependencies between plugins.");
		}
		virtual FName GetSortColumn() const override
		{
			return FAssetTableColumns::StagedCompressedSizeColumnId;
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Descending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<UE::Insights::FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* PrimaryGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FPluginDependencyGrouping>();
				});
			if (PrimaryGrouping)
			{
				InOutCurrentGroupings.Add(*PrimaryGrouping);
			}

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* SecondaryGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::TypeColumnId;
				});
			if (SecondaryGrouping)
			{
				InOutCurrentGroupings.Add(*SecondaryGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                         !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                  !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                  !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId,!true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,               true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};
	AvailableViewPresets.Add(MakeShared<FPluginDependencyView>());

	

	//////////////////////////////////////////////////

	SelectedViewPreset = AvailableViewPresets[0];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SWidget> SAssetTableTreeView::ConstructFooter()
{
	return
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SAssetTableTreeView::GetFooterLeftText)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SAssetTableTreeView::GetFooterCenterText1)
			.ColorAndOpacity(FSlateColor(EStyleColor::White25))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 2.0f, 2.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SAssetTableTreeView::GetFooterCenterText2)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f, 2.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SAssetTableTreeView::GetFooterRightText1)
		];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::InternalCreateGroupings()
{
	UE::Insights::STableTreeView::InternalCreateGroupings();

	AvailableGroupings.RemoveAll(
		[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
		{
			if (Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValue>())
			{
				const FName ColumnId = Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValue>().GetColumnId();
				if (ColumnId == FAssetTableColumns::CountColumnId)
				{
					return true;
				}
			}
			else if (Grouping->Is<UE::Insights::FTreeNodeGroupingByPathBreakdown>())
			{
				const FName ColumnId = Grouping->As<UE::Insights::FTreeNodeGroupingByPathBreakdown>().GetColumnId();
				if (ColumnId != FAssetTableColumns::PathColumnId)
				{
					return true;
				}
			}
			return false;
		});

	// Add custom groupings...
	int32 Index = 1; // after the Flat ("All") grouping

	AvailableGroupings.Insert(MakeShared<FAssetDependencyGrouping>(), Index++);
	AvailableGroupings.Insert(MakeShared<FPluginDependencyGrouping>(), Index++);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static void WriteDependencyLine(const FAssetTable& AssetTable, const TMap<int32, TArray<int32>>& RouteMap, int32 RowIndex, FAnsiStringBuilderBase* ReusableLineBuffer, FArchive* OutDependencyFile, FString DependencyType)
{
	const FAssetTableRow& Row = AssetTable.GetAssetChecked(RowIndex);
	ReusableLineBuffer->Appendf("%s%s,%s,%s,%s", *WriteToAnsiString<512>(Row.GetPath()),
												*WriteToAnsiString<64>(Row.GetName()), 
												*WriteToAnsiString<64>(Row.GetType()),
												*WriteToAnsiString<32>(LexToString(Row.GetStagedCompressedSize())),
												*WriteToAnsiString<16>(*DependencyType));

	if (const TArray<int32>* Route = RouteMap.Find(RowIndex))
	{
		ReusableLineBuffer->AppendChar(',');
		if (Route->Num() > 0)
		{
			for (int32 DependencyIndex : *Route)
			{
				const FAssetTableRow& DependencyRow = AssetTable.GetAssetChecked(DependencyIndex);
				ReusableLineBuffer->Appendf("%s%s->", *WriteToAnsiString<512>(DependencyRow.GetPath()),
					*WriteToAnsiString<64>(DependencyRow.GetName()));
			}
			ReusableLineBuffer->RemoveSuffix(2); // Remove the last ->
		}
	}
	ReusableLineBuffer->Append(LINE_TERMINATOR);
	OutDependencyFile->Serialize(ReusableLineBuffer->GetData(), ReusableLineBuffer->Len());
	ReusableLineBuffer->Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::ExportDependencyData() const
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		FString DefaultFileName;
		if (SelectedIndices.Num() > 1)
		{
			DefaultFileName = "Batch Dependency Export.csv";
		}
		else
		{
			int32 RootIndex = *SelectedIndices.CreateConstIterator();
			DefaultFileName = FString::Printf(TEXT("%s Dependencies.csv"), GetAssetTable()->GetAssetChecked(RootIndex).GetName());
		}

		TArray<FString> SaveFileNames;
		const bool bFileSelected = DesktopPlatform->SaveFileDialog(
			ParentWindowHandle,
			LOCTEXT("ExportDependencyData_SaveFileDialogTitle", "Save dependency data as...").ToString(),
			FPaths::ProjectLogDir(),
			DefaultFileName,
			TEXT("Comma-Separated Values (*.csv)|*.csv"),
			EFileDialogFlags::None,
			SaveFileNames);
		
		if (!bFileSelected)
		{
			return;
		}
		ensure(SaveFileNames.Num() == 1);
		FString OutputFileName = SaveFileNames[0];


		TSet<int32> ExternalDependencies;
		TMap<int32, TArray<int32>> RouteMap;
		FAssetTableRow::ComputeTotalSizeExternalDependencies(*GetAssetTable(), SelectedIndices, &ExternalDependencies, &RouteMap);

		TSet<int32> UniqueDependencies;
		TSet<int32> SharedDependencies;
		FAssetTableRow::ComputeDependencySizes(*GetAssetTable(), SelectedIndices, &UniqueDependencies, &SharedDependencies);

		FString TimeSuffix = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));

		TAnsiStringBuilder<4096> StringBuilder;
		TUniquePtr<FArchive> DependencyFile(IFileManager::Get().CreateFileWriter(*OutputFileName));

		StringBuilder.Appendf("Asset,Asset Type,Self Size,Dependency Type,Dependency Chain\n");
		{
			for (int32 RootIndex : SelectedIndices)
			{
				WriteDependencyLine(*GetAssetTable(), RouteMap, RootIndex, &StringBuilder, DependencyFile.Get(), TEXT("Root"));
			}

			for (int32 DependencyIndex : UniqueDependencies)
			{
				WriteDependencyLine(*GetAssetTable(), RouteMap, DependencyIndex, &StringBuilder, DependencyFile.Get(), TEXT("Unique"));
			}

			for (int32 DependencyIndex : SharedDependencies)
			{
				WriteDependencyLine(*GetAssetTable(), RouteMap, DependencyIndex, &StringBuilder, DependencyFile.Get(), TEXT("Shared"));
			}

			for (int32 DependencyIndex : ExternalDependencies)
			{
				WriteDependencyLine(*GetAssetTable(), RouteMap, DependencyIndex, &StringBuilder, DependencyFile.Get(), TEXT("External"));
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TArray<FAssetData> SAssetTableTreeView::GetAssetDataForSelection() const
{
	TArray<FAssetData> Assets;
	for (int32 SelectionIndex : SelectedIndices)
	{
		const FSoftObjectPath& SoftObjectPath = GetAssetTable()->GetAssetChecked(SelectionIndex).GetSoftObjectPath();
		FAssetData AssetData;
		if (IsRegistrySourceValid())
		{
			AssetData = *RegistrySource.GetOwnedRegistryState()->GetAssetByObjectPath(SoftObjectPath);
		}
		Assets.Add(AssetData);
	}

	return Assets;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::ExtendMenu(FMenuBuilder& MenuBuilder)
{
	FCanExecuteAction HasSelectionAndCanExecute = FCanExecuteAction::CreateLambda([this]()
		{
			return GetAssetTable() && (SelectedIndices.Num() > 0);
		});

	FCanExecuteAction HasSelectionAndRegistrySourceAndCanExecute = FCanExecuteAction::CreateLambda([this]()
		{
			return IsRegistrySourceValid() && GetAssetTable() && (SelectedIndices.Num() > 0);
		});

	MenuBuilder.BeginSection("Asset", LOCTEXT("ContextMenu_Section_Asset", "Asset"));
	{
		//////////////////////////////////////////////////////////////////////////
		/// EditSelectedAssets

		FUIAction EditSelectedAssets;
		EditSelectedAssets.CanExecuteAction = HasSelectionAndCanExecute;
		EditSelectedAssets.ExecuteAction = FExecuteAction::CreateLambda([this]()
			{
				TArray<FSoftObjectPath> AssetPaths;
				for (int32 SelectionIndex : SelectedIndices)
				{
					AssetPaths.Add(GetAssetTable()->GetAssetChecked(SelectionIndex).GetSoftObjectPath());
				}

				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorsForAssets(AssetPaths);
			});
		MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ContextMenu_EditAssetsLabel", "Edit..."),
				LOCTEXT("ContextMenu_EditAssets", "Opens the selected asset in the relevant editor."),
				FSlateIcon(),
				EditSelectedAssets,
				NAME_None,
				EUserInterfaceActionType::Button
			);

		//////////////////////////////////////////////////////////////////////////
		/// FindInContentBrowser

		FUIAction FindInContentBrowser;
		FindInContentBrowser.CanExecuteAction = HasSelectionAndRegistrySourceAndCanExecute;
		FindInContentBrowser.ExecuteAction = FExecuteAction::CreateLambda([this]()
			{
				FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().SyncBrowserToAssets(GetAssetDataForSelection());
		});
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_FindInContentBrowserLabel", "Find in Content Browser..."),
			LOCTEXT("ContextMenu_FindInContentBrowser", "Browses to the associated asset and selects it in the most recently used Content Browser (summoning one if necessary)."),
			FSlateIcon(),
			FindInContentBrowser,
			NAME_None,
			EUserInterfaceActionType::Button
		);

		//////////////////////////////////////////////////////////////////////////
		/// OpenReferenceViewer

		FUIAction OpenReferenceViewer;
		OpenReferenceViewer.CanExecuteAction = HasSelectionAndCanExecute;
		OpenReferenceViewer.ExecuteAction = FExecuteAction::CreateLambda([this]()
			{
				TArray<FAssetIdentifier> AssetIdentifiers;
				IAssetManagerEditorModule::ExtractAssetIdentifiersFromAssetDataList(GetAssetDataForSelection(), AssetIdentifiers);

				if (AssetIdentifiers.Num() > 0)
				{
					IAssetManagerEditorModule::Get().OpenReferenceViewerUI(AssetIdentifiers);
				}
			});
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_OpenReferenceViewerLabel", "Reference Viewer..."),
			LOCTEXT("ContextMenu_OpenReferenceViewer", "Launches the reference viewer showing the selected assets' references."),
			FSlateIcon(),
			OpenReferenceViewer,
			NAME_None,
			EUserInterfaceActionType::Button
		);

		//////////////////////////////////////////////////////////////////////////
		/// ExportDependencies

		FUIAction ExportDependenciesAction;
		ExportDependenciesAction.CanExecuteAction = HasSelectionAndCanExecute;

		ExportDependenciesAction.ExecuteAction = FExecuteAction::CreateLambda([this]()
			{
				ExportDependencyData();
			});

		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_ExportDependenciesLabel", "Export Dependencies..."),
			LOCTEXT("ContextMenu_ExportDependencies", "Exports dependency CSVs for the selected asset."),
			FSlateIcon(),
			ExportDependenciesAction,
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::RequestOpenRegistry()
{
	bNeedsToOpenRegistry = true;
	FooterLeftTextStoredPreOpen = FooterLeftText;
	FooterLeftText = LOCTEXT("FooterLeftTextFmt_OpenRegistry_Filtered", "Opening registry... please wait...");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::OpenRegistry()
{
	RegistrySource.SourceName = FAssetManagerEditorRegistrySource::CustomSourceName;
	if (EditorModule->PopulateRegistrySource(&RegistrySource))
	{
		IFileManager& FileManager = IFileManager::Get();
		const FString RegistryFilePath = FileManager.GetFilenameOnDisk(*FileManager.ConvertToAbsolutePathForExternalAppForRead(*RegistrySource.SourceFilename));
		RegistrySourceTimeText->SetText(FText::Format(LOCTEXT("RegistrySourceTimeText", "Loaded: {0} from {1}"), 
			FText::FromString(RegistryFilePath),
			FText::FromString(RegistrySource.SourceTimestamp)));

		RequestRefreshAssets();
	}
	else if (!IsRegistrySourceValid())
	{
		FooterLeftText = LOCTEXT("FooterLeftTextFmt_OpenRegistry_Failed", "No registry selected or load failed.");
		RegistrySourceTimeText->SetText(LOCTEXT("RegistrySourceTimeText_None", "No registry loaded."));
	}
	else
	{
		FooterLeftText = FooterLeftTextStoredPreOpen;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SAssetTableTreeView::GetOpenRegistryToolTipText() const
{
	return FText::Format(LOCTEXT("OpenRegistryButtonToolTip", "Opens a new development asset registry.\nCurrent: {0}\nTimestamp: {1}"),
		FText::FromString(RegistrySource.SourceFilename),
		FText::FromString(RegistrySource.SourceTimestamp));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SAssetTableTreeView::CanChangeRegistry() const
{
	return !bIsUpdateRunning && !bNeedsToOpenRegistry && !bNeedsToRefreshAssets && !bNeedsToRebuild;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SAssetTableTreeView::OnClickedOpenRegistry()
{
	CancelCurrentAsyncOp();
	RequestOpenRegistry();
	return FReply::Handled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::RequestRefreshAssets()
{
	bNeedsToRefreshAssets = true;
	FooterLeftText = LOCTEXT("FooterLeftTextFmt_RefreshAssets_Filtered", "Pre-analyzing disk sizes... please wait...");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SAssetTableTreeView::GetFooterLeftText() const
{
	return FooterLeftText;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SAssetTableTreeView::GetFooterCenterText1() const
{
	return FooterCenterText1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SAssetTableTreeView::GetFooterCenterText2() const
{
	return FooterCenterText2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SAssetTableTreeView::GetFooterRightText1() const
{
	return FooterRightText1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::CalculateBaseAndMarginalCostForSelection(TSet<int32>& SelectionSetIndices, int64* OutTotalSizeMultiplyUsed, int64* OutTotalSizeSingleUse) const
{
	if (!ensure((OutTotalSizeMultiplyUsed != nullptr) && (OutTotalSizeSingleUse != nullptr)))
	{
		return;
	}

	TSet<const TCHAR*, TStringPointerSetKeyFuncs_DEPRECATED<const TCHAR*>> SelectedPlugins;
	for (int32 ItemIndex : SelectionSetIndices)
	{
		SelectedPlugins.Add(GetAssetTable()->GetAssetChecked(ItemIndex).GetPluginName());
	}

	TMap<int32, int32> NumItemsReferencingDependency;
	for (int32 ItemRowIndex : SelectionSetIndices)
	{
		TSet<int32> Dependencies = FAssetTableRow::GatherAllReachableNodes(TArray<int32>{ItemRowIndex}, * GetAssetTable(), TSet<int32>{}, SelectedPlugins);
		for (int32 DependencyRowIndex : Dependencies)
		{
			if (SelectionSetIndices.Contains(DependencyRowIndex))
			{
				// Don't count assets we've selected, we'll handle those separately
				continue;
			}
			if (int32* Count = NumItemsReferencingDependency.Find(DependencyRowIndex))
			{
				(*Count)++;
			}
			else
			{
				NumItemsReferencingDependency.Add(DependencyRowIndex, 1);
			}
		}
	}

	for (TPair<int32, int32> ReferenceCountPair : NumItemsReferencingDependency)
	{
		if (ReferenceCountPair.Value > 1)
		{
			*OutTotalSizeMultiplyUsed += GetAssetTable()->GetAssetChecked(ReferenceCountPair.Key).GetStagedCompressedSize();
		}
		else if (ReferenceCountPair.Value == 1)
		{
			*OutTotalSizeSingleUse += GetAssetTable()->GetAssetChecked(ReferenceCountPair.Key).GetStagedCompressedSize();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::TreeView_OnSelectionChanged(UE::Insights::FTableTreeNodePtr SelectedItem, ESelectInfo::Type SelectInfo)
{
	TArray<UE::Insights::FTableTreeNodePtr> SelectedNodes;
	const int32 NumSelectedNodes = TreeView->GetSelectedItems(SelectedNodes);
	int32 NumSelectedAssets = 0;
	FAssetTreeNodePtr NewSelectedAssetNode;
	int32 NewlySelectedAssetRowIndex = -1;
	TSet<int32> SelectionSetIndices;

	for (const UE::Insights::FTableTreeNodePtr& Node : SelectedNodes)
	{
		if (Node->Is<FAssetTreeNode>() && Node->As<FAssetTreeNode>().IsValidAsset())
		{
			NewSelectedAssetNode = StaticCastSharedPtr<FAssetTreeNode>(Node);
			NewlySelectedAssetRowIndex = NewSelectedAssetNode->GetRowIndex();
			SelectionSetIndices.Add(NewlySelectedAssetRowIndex);
			++NumSelectedAssets;
		}
	}

	const int32 FilteredAssetCount = FilteredNodesPtr->Num();
	const int32 VisibleAssetCount = GetAssetTable()->GetVisibleAssetCount();

	if (NumSelectedAssets == 0)
	{
		if (FilteredAssetCount != VisibleAssetCount)
		{
			FooterLeftText = FText::Format(LOCTEXT("FooterLeftTextFmt_NoSelected_Filtered", "{0} / {1} assets"), FText::AsNumber(FilteredAssetCount), FText::AsNumber(VisibleAssetCount));
		}
		else
		{
			FooterLeftText = FText::Format(LOCTEXT("FooterLeftTextFmt_NoSelected_NoFiltered", "{0} assets"), FText::AsNumber(VisibleAssetCount));
		}
		FooterCenterText1 = FText();
		FooterCenterText2 = FText();
		FooterRightText1 = FText();
	}
	else if (NumSelectedAssets == 1)
	{
		if (FilteredAssetCount != VisibleAssetCount)
		{
			FooterLeftText = FText::Format(LOCTEXT("FooterLeftTextFmt_1Selected_Filtered", "{0}/{1} assets (1 selected)"), FText::AsNumber(FilteredAssetCount), FText::AsNumber(VisibleAssetCount));
		}
		else
		{
			FooterLeftText = FText::Format(LOCTEXT("FooterLeftTextFmt_1Selected_NoFiltered", "{0} assets (1 selected)"), FText::AsNumber(VisibleAssetCount));
		}
		const FAssetTableRow& AssetTableRow = GetAssetTable()->GetAssetChecked(NewlySelectedAssetRowIndex);
		FooterCenterText1 = FText::FromString(AssetTableRow.GetPath());
		FooterCenterText2 = FText::FromString(AssetTableRow.GetName());
		FooterRightText1 = FText::Format(LOCTEXT("FooterRightFmt", "Self: {0}    Unique: {1}    Shared: {2}    External: {3}"),
			FText::AsMemory(AssetTableRow.GetStagedCompressedSize()),
			FText::AsMemory(AssetTableRow.GetOrComputeTotalSizeUniqueDependencies(*GetAssetTable(), NewlySelectedAssetRowIndex)),
			FText::AsMemory(AssetTableRow.GetOrComputeTotalSizeSharedDependencies(*GetAssetTable(), NewlySelectedAssetRowIndex)),
			FText::AsMemory(AssetTableRow.GetOrComputeTotalSizeExternalDependencies(*GetAssetTable(), NewlySelectedAssetRowIndex)));
	}
	else
	{
		bool AllSelectedNodesAreSameType = true;
		{
			// We use the Type() attribute and not NativeClass to be a bit more generous when considering, e.g., 
			// whether a FortWeaponRangedItemDefinition is comparable to a FortWeaponMeleeDualWieldItemDefinition
			const TCHAR* FirstType = nullptr;
			bool FirstIndex = true;
			for (int32 SelectedNodeIndex : SelectionSetIndices)
			{
				const FAssetTableRow& AssetTableRow = GetAssetTable()->GetAssetChecked(SelectedNodeIndex);
				const TCHAR* Type = AssetTableRow.GetType();
				if (FirstIndex)
				{
					FirstType = Type;
					FirstIndex = false;
				}
				else if (Type != FirstType)
				{
					AllSelectedNodesAreSameType = false;
					break;
				}
			}
		}


		int64 TotalExternalDependencySize = FAssetTableRow::ComputeTotalSizeExternalDependencies(*GetAssetTable(), SelectionSetIndices);
		FAssetTableDependencySizes Sizes = FAssetTableRow::ComputeDependencySizes(*GetAssetTable(), SelectionSetIndices, nullptr, nullptr);

		int64 TotalSelfSize = 0;
		for (int32 Index : SelectionSetIndices)
		{
			TotalSelfSize += GetAssetTable()->GetAssetChecked(Index).GetStagedCompressedSize();
		}

		FText BaseAndMarginalCost;
		if (AllSelectedNodesAreSameType)
		{
			int64 TotalSizeMultiplyUsed = 0;
			int64 TotalSizeSingleUse = 0;
			CalculateBaseAndMarginalCostForSelection(SelectionSetIndices, &TotalSizeMultiplyUsed, &TotalSizeSingleUse);

			BaseAndMarginalCost = FText::Format(LOCTEXT("FooterLeft_BaseAndMarginalCost", " -- Base Cost: {0}  Per Asset Cost: {1}"),
				FText::AsMemory(TotalSizeMultiplyUsed),
				FText::AsMemory((TotalSelfSize + TotalSizeSingleUse) / SelectionSetIndices.Num()));
		}
		else
		{
			BaseAndMarginalCost = LOCTEXT("FooterLeft_BaseAndMarginalCost_MultipleTypesError", " -- Multiple types selected");
		}

		if (FilteredAssetCount != VisibleAssetCount)
		{
			FooterLeftText = FText::Format(LOCTEXT("FooterLeftTextFmt_ManySelected_Filtered", "{0} / {1} assets ({2} selected{3})"), FText::AsNumber(FilteredAssetCount), FText::AsNumber(VisibleAssetCount), FText::AsNumber(NumSelectedAssets), BaseAndMarginalCost);
		}
		else
		{
			FooterLeftText = FText::Format(LOCTEXT("FooterLeftTextFmt_ManySelected_NoFiltered", "{0} assets ({1} selected{2})"), FText::AsNumber(VisibleAssetCount), FText::AsNumber(NumSelectedAssets), BaseAndMarginalCost);
		}
		FooterCenterText1 = FText();
		FooterCenterText2 = FText();

		FooterRightText1 = FText::Format(LOCTEXT("FooterRightFmt", "Self: {0}    Unique: {1}    Shared: {2}    External: {3}"),
			FText::AsMemory(TotalSelfSize),
			FText::AsMemory(Sizes.UniqueDependenciesSize),
			FText::AsMemory(Sizes.SharedDependenciesSize),
			FText::AsMemory(TotalExternalDependencySize));
	}

	if (NumSelectedAssets != 1)
	{
		NewSelectedAssetNode.Reset();
	}

	if (SelectedAssetNode != NewSelectedAssetNode)
	{
		SelectedAssetNode = NewSelectedAssetNode;
	}

	SelectedIndices = SelectionSetIndices;

	if (OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Execute(SelectedNodes);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::QuickPopulateAssetTableRow(FAssetTableRow& OutRow, const FAssetData& AssetData, FAssetTable& AssetTable) const
{
	OutRow.SoftObjectPath = AssetData.GetSoftObjectPath();

	// Asset Type
	FString AssetType;
	if (UAssetDefinitionRegistry* AssetDefinitionRegistry = UAssetDefinitionRegistry::Get())
	{
		const UAssetDefinition* AssetDefinition = AssetDefinitionRegistry->GetAssetDefinitionForAsset(AssetData);
		if (AssetDefinition)
		{
			AssetType = AssetDefinition->GetAssetDisplayName().ToString();
		}
	}
	if (AssetType.IsEmpty() && IsRegistrySourceValid())
	{
		if (!EditorModule->GetStringValueForCustomColumn(AssetData, FName(TEXT("Type")), AssetType, &RegistrySource))
		{
			AssetType = AssetData.AssetClassPath.ToString();
		}
	}
	OutRow.Type = AssetTable.StoreStr(AssetType);

	// Asset Name
	OutRow.Name = AssetTable.StoreStr(AssetData.AssetName.ToString());

	// Path (without package or asset name)
	FString ObjectPathString = AssetData.GetObjectPathString();
	int Index;
	ObjectPathString.FindLastChar(TEXT('/'), Index);
	if (Index != INDEX_NONE)
	{
		ObjectPathString = ObjectPathString.Left(Index + 1);
	}
	OutRow.Path = AssetTable.StoreStr(ObjectPathString);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::PopulateAssetTableRow(FAssetTableRow& OutRow, const FAssetData& AssetData, FAssetTable& AssetTable) const
{
	ensure(IsRegistrySourceValid());
	QuickPopulateAssetTableRow(OutRow, AssetData, AssetTable);

	FString Str;

	if (EditorModule->GetStringValueForCustomColumn(AssetData, FPrimaryAssetId::PrimaryAssetTypeTag, Str, &RegistrySource))
	{
		OutRow.PrimaryType = AssetTable.StoreStr(Str);
	}
	if (EditorModule->GetStringValueForCustomColumn(AssetData, FPrimaryAssetId::PrimaryAssetNameTag, Str, &RegistrySource))
	{
		OutRow.PrimaryName = AssetTable.StoreStr(Str);
	}

	EditorModule->GetIntegerValueForCustomColumn(AssetData, IAssetManagerEditorModule::StageChunkCompressedSizeName, OutRow.StagedCompressedSize, &RegistrySource);
	EditorModule->GetIntegerValueForCustomColumn(AssetData, IAssetManagerEditorModule::TotalUsageName, OutRow.TotalUsageCount, &RegistrySource);

	if (EditorModule->GetStringValueForCustomColumn(AssetData, IAssetManagerEditorModule::ChunksName, Str, &RegistrySource))
	{
		OutRow.Chunks = AssetTable.StoreStr(Str);
	}

	if (EditorModule->GetStringValueForCustomColumn(AssetData, IAssetManagerEditorModule::PluginName, Str, &RegistrySource))
	{
		OutRow.PluginName = AssetTable.StoreStr(Str);
	}

	OutRow.NativeClass = AssetTable.StoreStr(AssetData.AssetClassPath.GetAssetName().ToString());

	// Sets the color based on asset type.
	const uint32 AssetTypeHash = GetTypeHash(FStringView(OutRow.Type));
	OutRow.Color = USlateThemeManager::Get().GetColor((EStyleColor)((uint32)EStyleColor::AccentBlue + AssetTypeHash % 8));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// Refresh list of assets for the tree view
void SAssetTableTreeView::RefreshAssets()
{
	TSharedPtr<FAssetTable> AssetTable = GetAssetTable();
	if (!AssetTable.IsValid())
	{
		return;
	}

	UE_LOG(LogInsights, Log, TEXT("[AssetTree] Build asset table..."));

	UE::Insights::FStopwatch Stopwatch;
	Stopwatch.Start();

	// Clears all tree nodes (that references the previous assets).
	AssetTable->SetVisibleAssetCount(0);
	RebuildTree(true);

	// Now is safe to clear the previous assets.
	AssetTable->ClearAllData();

	TMap<FString, int64> PluginToSizeMap;

	typedef TSet<const TCHAR*, TStringPointerSetKeyFuncs_DEPRECATED<const TCHAR*>> DeprecatedTCharSetType;
	TMap<const TCHAR*, DeprecatedTCharSetType, FDefaultSetAllocator, TStringPointerMapKeyFuncs_DEPRECATED<const TCHAR*, DeprecatedTCharSetType>> DiscoveredPluginDependencyEdges;

	if (IsRegistrySourceValid())
	{
		TMap<FAssetData, int32> AssetToIndexMap;

		TArray<FAssetData> SourceAssets;

		RegistrySource.GetOwnedRegistryState()->GetAllAssets(TSet<FName>(), SourceAssets);

		for (int32 SourceAssetIndex = 0; SourceAssetIndex < SourceAssets.Num(); SourceAssetIndex++)
		{
			TArray<FAssetData> AssetsInSourcePackage;
			AssetRegistry->GetAssetsByPackageName(SourceAssets[SourceAssetIndex].PackageName, AssetsInSourcePackage, /*bIncludeOnlyOnDiskAssets*/ true); // Only use on disk assets to avoid creating FAssetData for everything in memory

			for (FAssetData& SourceAsset : AssetsInSourcePackage)
			{
				if (AssetToIndexMap.Find(SourceAsset) == nullptr)
				{
					FAssetTableRow AssetRow;
					PopulateAssetTableRow(AssetRow, SourceAsset, *AssetTable);
					AssetTable->AddAsset(AssetRow);
					AssetToIndexMap.Add(SourceAsset, AssetTable->GetTotalAssetCount() - 1);

					if (int64* PluginSize = PluginToSizeMap.Find(AssetRow.GetPluginName()))
					{
						(*PluginSize) += AssetRow.GetStagedCompressedSize();
					}
					else
					{
						PluginToSizeMap.Add(AssetRow.GetPluginName(), AssetRow.GetStagedCompressedSize());
					}
				}
			}
		}

		int32 EntryIndex = AssetTable->GetAssets().Num();

		TSet<FName> AlreadyProcessedPackages;
		// Go over all SourceAssets. We will add more to the end as we find we need to add them.
		for (int32 SourceAssetIndex = 0; SourceAssetIndex < SourceAssets.Num(); SourceAssetIndex++)
		{
			// There are potentially multiple assets in a package, each with an FAssetData
			// We are only dealing in package dependencies, though, so don't process the same package twice
			// But we will need to conceptually assign all the package dependencies to all assets in the package below
			if (AlreadyProcessedPackages.Contains(SourceAssets[SourceAssetIndex].PackageName))
			{
				continue;
			}
			else
			{
				AlreadyProcessedPackages.Add(SourceAssets[SourceAssetIndex].PackageName);
			}

			FAssetIdentifier CurrentIdentifier(SourceAssets[SourceAssetIndex].PackageName);
			// We'll have to add the dependencies to all these entries
			TArray<FAssetData> AssetsInSourcePackage;
			AssetRegistry->GetAssetsByPackageName(SourceAssets[SourceAssetIndex].PackageName, AssetsInSourcePackage, /*bIncludeOnlyOnDiskAssets*/ true); // Only use on disk assets to avoid creating FAssetData for everything in memory

			// Get the dependencies
			TArray<FAssetIdentifier> DependencyList;
			RegistrySource.GetDependencies(CurrentIdentifier, DependencyList, UE::AssetRegistry::EDependencyCategory::Package);

			TArray<int32> IndicesOfDependenciesInRowTableToAddToCurrentSourceAssetRow;

			for (const FAssetIdentifier& Dependency : DependencyList)
			{
				TArrayView<FAssetData const* const> AssetsInDependencyPackage = RegistrySource.GetOwnedRegistryState()->GetAssetsByPackageName(Dependency.PackageName);
				for (FAssetData const* const DependencyAsset : AssetsInDependencyPackage)
				{
					if (int32* DependencyIndex = AssetToIndexMap.Find(*DependencyAsset))
					{
						// Do we already know about the asset? If so, great.
						IndicesOfDependenciesInRowTableToAddToCurrentSourceAssetRow.Add(*DependencyIndex);
					}
					else
					{
						// We don't know about this asset yet. Create a new row for it and add it to the source list for further dependency analysis
						FAssetTableRow NewRow;
						PopulateAssetTableRow(NewRow, *DependencyAsset, *AssetTable);
						AssetTable->AddAsset(NewRow);
						IndicesOfDependenciesInRowTableToAddToCurrentSourceAssetRow.Add(AssetTable->GetTotalAssetCount() - 1);
						AssetToIndexMap.Add(*DependencyAsset, AssetTable->GetTotalAssetCount() - 1);
						SourceAssets.Add(*DependencyAsset);
						if (int64* PluginSize = PluginToSizeMap.Find(NewRow.GetPluginName()))
						{
							(*PluginSize) += NewRow.GetStagedCompressedSize();
						}
						else
						{
							PluginToSizeMap.Add(NewRow.GetPluginName(), NewRow.GetStagedCompressedSize());
						}
					}
				}
			}

			if (IndicesOfDependenciesInRowTableToAddToCurrentSourceAssetRow.Num())
			{
				// We found some dependencies. Let's add them.
				for (const FAssetData& SourceAssetData : AssetsInSourcePackage)
				{
					int32* RowIndex = AssetToIndexMap.Find(SourceAssetData);
					if (RowIndex == nullptr)
					{
						UE_LOG(LogInsights, Warning, TEXT("Failed to find asset %s in package %s, source asset index %d. Asset registry loading was %s"), *SourceAssetData.AssetName.ToString(), *SourceAssetData.PackageName.ToString(), SourceAssetIndex, AssetRegistry->IsLoadingAssets() ? TEXT("INCOMPLETE") : TEXT("complete"));
					}
					if (ensure(RowIndex != nullptr))
					{
						const TCHAR* CurrentPlugin = AssetTable->GetAssetChecked(*RowIndex).GetPluginName();
						DeprecatedTCharSetType DependencyPlugins;

						for (int32 Index : IndicesOfDependenciesInRowTableToAddToCurrentSourceAssetRow)
						{
							AssetTable->GetAsset(*RowIndex)->Dependencies.AddUnique(Index);

							const TCHAR* DependencyPlugin = AssetTable->GetAsset(Index)->GetPluginName();
							if (DependencyPlugin != CurrentPlugin)
							{
								DependencyPlugins.Add(DependencyPlugin);
							}
						}


						DeprecatedTCharSetType& DiscoveredPluginDependencyList = DiscoveredPluginDependencyEdges.FindOrAdd(CurrentPlugin);
						for (const TCHAR* DependencyPlugin : DependencyPlugins)
						{
							DiscoveredPluginDependencyList.Add(DependencyPlugin);
						}
					}
				}

				for (int32 Index : IndicesOfDependenciesInRowTableToAddToCurrentSourceAssetRow)
				{
					// Now for each of those dependencies, add this source asset row as a referencer
					if (FAssetTableRow* DependentRow = AssetTable->GetAsset(Index))
					{
						for (const FAssetData& SourceAssetData : AssetsInSourcePackage)
						{
							int32* RowIndex = AssetToIndexMap.Find(SourceAssetData);
							if (ensure(RowIndex != nullptr))
							{
								DependentRow->Referencers.AddUnique(*RowIndex);
							}
						}
					}
				}
			}
			AssetTable->SetVisibleAssetCount(AssetTable->GetAssets().Num());
		}
	}

	// Setup plugin infos and dependencies. This will eventually be replaced by data from the asset registry
	for (TPair<FString, int64>& PluginEntry : PluginToSizeMap)
	{
		const TCHAR* StoredPluginName = AssetTable->StoreStr(PluginEntry.Key);
		FAssetTablePluginInfo& PluginInfo = AssetTable->GetOrCreatePluginInfo(StoredPluginName);
		PluginInfo.Size = PluginEntry.Value;
		if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginEntry.Key))
		{
			TArray<FPluginReferenceDescriptor> PluginReferences;
			IPluginManager::Get().GetPluginDependencies(PluginEntry.Key, PluginReferences);
			for (const FPluginReferenceDescriptor& ReferenceDescriptor : PluginReferences)
			{
				const TCHAR* StoredReferencePluginName = AssetTable->StoreStr(ReferenceDescriptor.Name);
				int32 DependencyIndex = AssetTable->GetIndexForPlugin(StoredReferencePluginName);
				if (DependencyIndex == -1)
				{
					DependencyIndex = AssetTable->GetNumPlugins();
					AssetTable->GetOrCreatePluginInfo(StoredReferencePluginName);
				}
				// Note that we can't use PluginInfo directly because the above code could have grown the array 
				// and that would invalidate the reference
				AssetTable->GetOrCreatePluginInfo(StoredPluginName).PluginDependencies.Add(DependencyIndex);
			}
		}
		else
		{
			UE_LOG(LogInsights, Warning, TEXT("Could not find plugin %s in plugin manager."), PluginInfo.PluginName);
		}
	}

	// Discovered version
	for (const TPair<const TCHAR*, DeprecatedTCharSetType>& DiscoveredDependencies : DiscoveredPluginDependencyEdges)
	{
		const TCHAR* StoredPluginName = AssetTable->StoreStr(DiscoveredDependencies.Key);
		FAssetTablePluginInfo& PluginInfo = AssetTable->GetOrCreatePluginInfo(StoredPluginName);

		for (const TCHAR* DependencyName : DiscoveredDependencies.Value)
		{
			int32 DependencyIndex = AssetTable->GetIndexForPlugin(DependencyName);
			ensureAlways(DependencyIndex != -1); // These were created above.
			PluginInfo.DiscoveredPluginDependencies.Add(DependencyIndex);
		}
	}

	// TODO: Once we have the dependency data from the uplugin files in the cook we can 
	// compare them to the discovered dependencies to see if there are plugins we're dependending on
	// that, perhaps, we shouldn't be. That said, we shouldn't storer the discovered dependencies on the 
	// plugin infos at that point.

	//// Analyze difference
	//UE_LOG(LogInsights, Warning, TEXT("Registry Deps-------"));
	//for (int32 PluginIndex = 0; PluginIndex < AssetTable->GetNumPlugins(); PluginIndex++)
	//{
	//	const FAssetTablePluginInfo& PluginInfo = AssetTable->GetPluginInfoByIndex(PluginIndex);
	//	UE_LOG(LogInsights, Warning, TEXT("Plugin: %s"), PluginInfo.PluginName);
	//	for (int32 DependencyIndex : PluginInfo.PluginDependencies)
	//	{
	//		UE_LOG(LogInsights, Warning, TEXT("\t%s"), AssetTable->GetPluginInfoByIndex(DependencyIndex).PluginName);
	//	}
	//}
	//UE_LOG(LogInsights, Warning, TEXT("Discovered Deps-------"));
	//for (int32 PluginIndex = 0; PluginIndex < AssetTable->GetNumPlugins(); PluginIndex++)
	//{
	//	const FAssetTablePluginInfo& PluginInfo = AssetTable->GetPluginInfoByIndex(PluginIndex);
	//	UE_LOG(LogInsights, Warning, TEXT("Plugin: %s"), PluginInfo.PluginName);
	//	for (int32 DependencyIndex : PluginInfo.DiscoveredPluginDependencies)
	//	{
	//		UE_LOG(LogInsights, Warning, TEXT("\t%s"), AssetTable->GetPluginInfoByIndex(DependencyIndex).PluginName);
	//	}
	//}

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	UE_LOG(LogInsights, Log, TEXT("[AssetTree] Asset table rebuilt in %.4fs (%d visible + %d hidden = %d assets)"),
		TotalTime, AssetTable->GetVisibleAssetCount(), AssetTable->GetHiddenAssetCount(), AssetTable->GetTotalAssetCount());

	const FAssetTableStringStore& StringStore = AssetTable->GetStringStore();
	UE_LOG(LogInsights, Log, TEXT("[AssetTree] String store: %d strings (%lld bytes) --> %d strings (%lld bytes | %lld bytes) %.0f%%"),
		StringStore.GetNumInputStrings(), StringStore.GetTotalInputStringSize(),
		StringStore.GetNumStrings(), StringStore.GetTotalStringSize(), StringStore.GetAllocatedSize(),
		(double)StringStore.GetAllocatedSize() * 100.0 / (double)StringStore.GetTotalInputStringSize());

	RequestRebuildTree();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::RequestRebuildTree()
{
	bNeedsToRebuild = true;
	FooterLeftText = LOCTEXT("FooterLeftTextFmt_RebuildTree_Filtered", "Rebuilding tree... please wait...");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::RebuildTree(bool bResync)
{
	if (!bResync)
	{
		// There are no incremental updates.
		return;
	}

	UE::Insights::FStopwatch Stopwatch;
	Stopwatch.Start();

	UE::Insights::FStopwatch SyncStopwatch;
	SyncStopwatch.Start();

	CancelCurrentAsyncOp();

	const int32 PreviousNodeCount = TableRowNodes.Num();
	TableRowNodes.Empty();

	TSharedPtr<FAssetTable> AssetTable = GetAssetTable();
	const int32 VisibleAssetCount = AssetTable.IsValid() ? AssetTable->GetVisibleAssetCount() : 0;

	if (VisibleAssetCount > 0)
	{
		UE_LOG(LogInsights, Log, TEXT("[AssetTree] Creating %d asset nodes (previously: %d nodes)..."), VisibleAssetCount, PreviousNodeCount);

		TableRowNodes.Reserve(VisibleAssetCount);

		ensure(TableRowNodes.Num() == 0);
		for (int32 AssetIndex = 0; AssetIndex < VisibleAssetCount; ++AssetIndex)
		{
			const FAssetTableRow* Asset = AssetTable->GetAsset(AssetIndex);

			FName NodeName(Asset->GetName());
			FAssetTreeNodePtr NodePtr = MakeShared<FAssetTreeNode>(NodeName, AssetTable, AssetIndex);
			TableRowNodes.Add(NodePtr);
		}
		ensure(TableRowNodes.Num() == VisibleAssetCount);
	}
	else
	{
		UE_LOG(LogInsights, Log, TEXT("[AssetTree] Resetting tree (previously: %d nodes)..."), PreviousNodeCount);
	}

	SyncStopwatch.Stop();

	UE_LOG(LogInsights, Log, TEXT("[AssetTree] Update tree..."));
	UpdateTree();
	TreeView->RebuildList();
	TreeView->ClearSelection();
	TreeView_OnSelectionChanged(nullptr, ESelectInfo::Type::Direct);

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	const double SyncTime = SyncStopwatch.GetAccumulatedTime();
	UE_LOG(LogInsights, Log, TEXT("[AssetTree] Tree view rebuilt in %.4fs (sync: %.4fs + update: %.4fs) --> %d asset nodes"),
		TotalTime, SyncTime, TotalTime - SyncTime, TableRowNodes.Num());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
