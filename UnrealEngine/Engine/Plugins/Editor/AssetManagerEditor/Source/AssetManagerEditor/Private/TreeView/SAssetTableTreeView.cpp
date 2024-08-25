// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAssetTableTreeView.h"

#include "AssetDefinitionRegistry.h"
#include "AssetManagerEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Set.h"
#include "ContentBrowserModule.h"
#include "CookMetadata.h"
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
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Serialization/ArrayReader.h"
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
	AssetManager = &UAssetManager::Get();
	EditorModule = &IAssetManagerEditorModule::Get();

	STableTreeView::ConstructWidget(InTablePtr);

	CreateGroupings();
	CreateSortings();

	RegistryInfoText->SetText(LOCTEXT("RegistrySourceTimeText_None", "No registry loaded."));

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
		if (bNeedsToRefreshAssets)
		{
			bNeedsToRefreshAssets = false;
			RefreshAssets();
		}
		if (bNeedsToRebuild)
		{
			bNeedsToRebuild = false;
			RebuildTree(true);
			if (bNeedsToRebuildColumns)
			{
				// This resets the column information only. It does NOT nuke the table data.
				GetAssetTable()->Reset();
				RebuildColumns();
				bNeedsToRebuildColumns = false;
			}
			ApplyViewPreset(*SelectedViewPreset);
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
				SAssignNew(RegistryInfoText, STextBlock)
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
			return LOCTEXT("PluginDepView_PresetToolTip", "Plugin Dependency Analysis View\nConfigure the tree view to show a breakdown of assets by Plugin, showing also the dependencies between plugins.");
		}
		virtual FName GetSortColumn() const override
		{
			return FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId;
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
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginInclusiveSizeColumnId,	        true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                  !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                  !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,  !true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,  !true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId,!true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,              !true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                  !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};

	//////////////////////////////////////////////////
	// Plugin, Type, Dependency View
	
	class FPluginTypeDependencyView : public UE::Insights::ITableTreeViewPreset
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("PluginTypeDepView_PresetName", "Asset Dependency Analysis");
		}

		virtual FText GetToolTip() const override
		{
			return LOCTEXT("PluginTypeDepView_PresetToolTip", "Asset Dependency Analysis View\nConfigure the tree view to show a breakdown of assets by Plugin, Type, and Dependencies.");
		}
		virtual FName GetSortColumn() const override
		{
			return UE::Insights::FTable::GetHierarchyColumnId();
		}
		virtual EColumnSortMode::Type GetSortMode() const override
		{
			return EColumnSortMode::Type::Ascending;
		}
		virtual void SetCurrentGroupings(const TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InAvailableGroupings, TArray<TSharedPtr<UE::Insights::FTreeNodeGrouping>>& InOutCurrentGroupings) const override
		{
			InOutCurrentGroupings.Reset();

			check(InAvailableGroupings[0]->Is<UE::Insights::FTreeNodeGroupingFlat>());
			InOutCurrentGroupings.Add(InAvailableGroupings[0]);

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* GameFeaturePluginGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::PluginNameColumnId;
				});
			if (GameFeaturePluginGrouping)
			{
				InOutCurrentGroupings.Add(*GameFeaturePluginGrouping);
			}

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

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* DependencyGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<FAssetDependencyGrouping>();
				});
			if (DependencyGrouping)
			{
				InOutCurrentGroupings.Add(*DependencyGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                  !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId,!true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,              !true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};
	 
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
			return FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId;
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

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* TypeGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::TypeColumnId;
				});
			if (TypeGrouping)
			{
				InOutCurrentGroupings.Add(*TypeGrouping);
			}

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* PluginNameGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::PluginNameColumnId;
				});
			if (PluginNameGrouping)
			{
				InOutCurrentGroupings.Add(*PluginNameGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId, true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,              !true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};

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
			return FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId;
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

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* SecondaryTypeGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::PluginNameColumnId;
				});
			if (SecondaryTypeGrouping)
			{
				InOutCurrentGroupings.Add(*SecondaryTypeGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId, true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,              !true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};


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
			return FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId;
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

			const TSharedPtr<UE::Insights::FTreeNodeGrouping>* SecondaryTypeGrouping = InAvailableGroupings.FindByPredicate(
				[](TSharedPtr<UE::Insights::FTreeNodeGrouping>& Grouping)
				{
					return Grouping->Is<UE::Insights::FTreeNodeGroupingByUniqueValueCString>() &&
						Grouping->As<UE::Insights::FTreeNodeGroupingByUniqueValueCString>().GetColumnId() == FAssetTableColumns::PluginNameColumnId;
				});
			if (SecondaryTypeGrouping)
			{
				InOutCurrentGroupings.Add(*SecondaryTypeGrouping);
			}
		}
		virtual void GetColumnConfigSet(TArray<UE::Insights::FTableColumnConfig>& InOutConfigSet) const override
		{
			InOutConfigSet.Add({ UE::Insights::FTable::GetHierarchyColumnId(),              true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::CountColumnId,                         true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                         !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId, true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,              !true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};


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
			return FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId;
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
			InOutConfigSet.Add({ FAssetTableColumns::StagedCompressedSizeRequiredInstallColumnId,          true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginInclusiveSizeColumnId,	        true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TypeColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NameColumnId,                          true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PathColumnId,                         !true, 400.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryTypeColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PrimaryNameColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeUniqueDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeSharedDependenciesColumnId,   true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalSizeExternalDependenciesColumnId, true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::TotalUsageCountColumnId,              !true, 100.0f });
			InOutConfigSet.Add({ FAssetTableColumns::ChunksColumnId,                       !true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::NativeClassColumnId,                   true, 200.0f });
			InOutConfigSet.Add({ FAssetTableColumns::PluginNameColumnId,                    true, 200.0f });
		}
	};

	//////////////////////////////////////////////////

	AvailableViewPresets.Add(MakeShared<FPluginDependencyView>());
	AvailableViewPresets.Add(MakeShared<FPluginTypeDependencyView>());
	AvailableViewPresets.Add(MakeShared<FAssetTypeViewPreset>());
	AvailableViewPresets.Add(MakeShared<FPluginView>());
	AvailableViewPresets.Add(MakeShared<FPrimaryTypeViewPreset>());
	AvailableViewPresets.Add(MakeShared<FClassTypeViewPreset>());

	/////////////////////////////////////////////////

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
												*WriteToAnsiString<32>(LexToString(Row.GetStagedCompressedSizeRequiredInstall())),
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
		if (SelectedAssetIndices.Num() > 1)
		{
			DefaultFileName = "Batch Dependency Export.csv";
		}
		else
		{
			int32 RootIndex = *SelectedAssetIndices.CreateConstIterator();
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
		FAssetTableRow::ComputeTotalSizeExternalDependencies(*GetAssetTable(), SelectedAssetIndices, &ExternalDependencies, &RouteMap);

		TSet<int32> UniqueDependencies;
		TSet<int32> SharedDependencies;
		FAssetTableRow::ComputeDependencySizes(*GetAssetTable(), SelectedAssetIndices, &UniqueDependencies, &SharedDependencies);

		FString TimeSuffix = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));

		TAnsiStringBuilder<4096> StringBuilder;
		TUniquePtr<FArchive> DependencyFile(IFileManager::Get().CreateFileWriter(*OutputFileName));

		StringBuilder.Appendf("Asset,Asset Type,Self Size,Dependency Type,Dependency Chain\n");
		{
			for (int32 RootIndex : SelectedAssetIndices)
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
	for (int32 SelectionIndex : SelectedAssetIndices)
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
			return GetAssetTable() && (SelectedAssetIndices.Num() > 0);
		});

	FCanExecuteAction HasSelectionAndRegistrySourceAndCanExecute = FCanExecuteAction::CreateLambda([this]()
		{
			return IsRegistrySourceValid() && GetAssetTable() && (SelectedAssetIndices.Num() > 0);
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
				for (int32 SelectionIndex : SelectedAssetIndices)
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

bool HashRegistryFile(const FString& FilePath, uint64* HashOut)
{
	check(HashOut != nullptr);

	bool Success = false;
	FArrayReader SerializedAssetData;
	if (FFileHelper::LoadFileToArray(SerializedAssetData, *FilePath))
	{
		*HashOut = UE::Cook::FCookMetadataState::ComputeHashOfDevelopmentAssetRegistry(MakeMemoryView(static_cast<TArray<uint8>&>(SerializedAssetData)));
		Success = true;
	}
	return Success;	
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool LoadCookMetadata(const FString& FilePath, UE::Cook::FCookMetadataState& OutMetadataState)
{
	const bool Success = OutMetadataState.ReadFromFile(FilePath);
	if (!Success)
	{
		OutMetadataState.Reset();
	}
	return Success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::OpenRegistry()
{
	// Ideally, we will load two files: a DevelopmentAssetRegistry.bin and a matching .ucookmeta file. However, it may happen that no ucookmeta is available
	// We will pop a dialog that asks the user to select either a .bin or a .ucookmeta. If they pick the .bin, we will attempt to find an appropriate ucookmeta
	// and vice versa.

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	extern const TCHAR* GetDevelopmentAssetRegistryFilename();
	const TCHAR* DevelopmentAssetRegistryCanonicalFilename = GetDevelopmentAssetRegistryFilename();
	const TCHAR* CookMetadataCanonicalFilename = *UE::Cook::GetCookMetadataFilename();
	const FString CookMetadataExtension = FPaths::GetExtension(CookMetadataCanonicalFilename);
	const FString DevelopmentAssetRegistryExtension = FPaths::GetExtension(DevelopmentAssetRegistryCanonicalFilename);;
	const FText Title = LOCTEXT("LoadAssetRegistryOrCookMetadata", "Load DevelopmentAssetRegistry or CookMetadata");
	const FString FileTypes = FString::Printf(TEXT("%s|*.%s|%s|*.%s"), 
		CookMetadataCanonicalFilename, *CookMetadataExtension, DevelopmentAssetRegistryCanonicalFilename, *DevelopmentAssetRegistryExtension);

	TArray<FString> OutFilenames;

	bool CanceledOpenDialog = !DesktopPlatform->OpenFileDialog(
		ParentWindowWindowHandle,
		Title.ToString(),
		TEXT(""),
		UE::Cook::GetCookMetadataFilename(),
		FileTypes,
		EFileDialogFlags::None,
		OutFilenames
	);

	if (CanceledOpenDialog)
	{
		FooterLeftText = FooterLeftTextStoredPreOpen;
		return;
	}

	FString RegistryFilename;
	FString MetadataFilename;

	bool FoundOtherFileByInference = false;

	UE::Cook::FCookMetadataState MetadataTemporaryStorage;
	ECheckFilesExistAndHashMatchesResult HashCheckResult = ECheckFilesExistAndHashMatchesResult::Unknown;

	if (OutFilenames.Num() == 1)
	{
		const FString& Filename = OutFilenames[0];
		FString FileExtension = FPaths::GetExtension(Filename);
		bool IsRegistryFile = FileExtension.Equals(DevelopmentAssetRegistryExtension, ESearchCase::IgnoreCase);
		bool IsMetadataFile = FileExtension.Equals(CookMetadataExtension, ESearchCase::IgnoreCase);

		// Try to automatically find the corresponding file
		const FString Directory = FPaths::GetPath(Filename);
		const FString InitialBaseFilename = FPaths::GetBaseFilename(Filename);
		FString NewFilenameToTry;
		if (IsRegistryFile)
		{
			RegistryFilename = Filename;
			NewFilenameToTry = Directory / InitialBaseFilename.Replace(TEXT("DevelopmentAssetRegistry"), TEXT("CookMetadata"));
			NewFilenameToTry += TEXT(".ucookmeta");
			if (IFileManager::Get().FileExists(*NewFilenameToTry))
			{
				MetadataFilename = NewFilenameToTry;
				FoundOtherFileByInference = true;
			}
		}
		else if (IsMetadataFile)
		{
			MetadataFilename = Filename;
			NewFilenameToTry = Directory / InitialBaseFilename.Replace(TEXT("CookMetadata"), TEXT("DevelopmentAssetRegistry"));
			NewFilenameToTry += TEXT(".bin");
			if (IFileManager::Get().FileExists(*NewFilenameToTry))
			{
				RegistryFilename = NewFilenameToTry;
				FoundOtherFileByInference = true;
			}
		}

		// We think we found the other file by inference. Let's check the hashes and see  if they match. If not, we'll prompt for the matching file
		if (FoundOtherFileByInference)
		{
			HashCheckResult = CheckFilesExistAndHashMatches(MetadataFilename, RegistryFilename, MetadataTemporaryStorage);
			FoundOtherFileByInference = HashCheckResult == ECheckFilesExistAndHashMatchesResult::Okay;
		}

		if (!FoundOtherFileByInference)
		{
			TArray<FString> OutFollowupFilenames;
			const FText FollowupTitle = IsRegistryFile ? LOCTEXT("LoadCookMetadata", "Load CookMetadata") : LOCTEXT("LoadAssetRegistry", "Load DevelopmentAssetRegistry");
			const FString FollowupFileTypes = FString::Printf(TEXT("%s|*.%s"), 
				IsRegistryFile ? CookMetadataCanonicalFilename : DevelopmentAssetRegistryCanonicalFilename, 
				IsRegistryFile ? *CookMetadataExtension : *DevelopmentAssetRegistryExtension);
			FString DefaultFilename = IsRegistryFile ? CookMetadataCanonicalFilename : DevelopmentAssetRegistryCanonicalFilename;
		
			CanceledOpenDialog = !DesktopPlatform->OpenFileDialog(
				ParentWindowWindowHandle,
				FollowupTitle.ToString(),
				TEXT(""),
				DefaultFilename,
				FollowupFileTypes,
				EFileDialogFlags::None,
				OutFollowupFilenames
			);

			if (CanceledOpenDialog && !IsRegistryFile)
			{
				FooterLeftText = FooterLeftTextStoredPreOpen;
				return;
			}

			if (OutFollowupFilenames.Num() == 1)
			{
				const FString& FollowupFilename = OutFollowupFilenames[0];
				const FString FollowupFileExtension = FPaths::GetExtension(FollowupFilename);
				bool FollowupIsRegistryFile = FollowupFileExtension.Equals(DevelopmentAssetRegistryExtension, ESearchCase::IgnoreCase);
				bool FollowupIsMetadataFile = FollowupFileExtension.Equals(CookMetadataExtension, ESearchCase::IgnoreCase);

				if (IsRegistryFile && FollowupIsMetadataFile)
				{
					MetadataFilename = FollowupFilename;
				}
				else if (IsMetadataFile && FollowupIsRegistryFile)
				{
					RegistryFilename = FollowupFilename;
				}
				else if (!IsMetadataFile && !IsRegistryFile && FollowupIsRegistryFile)
				{
					// Weird case where the user picked a bad metadata file (not ending in .ucookmeta) but a valid registry
					// Just treat this as though they only picked the registry file and report a missing metadata file.
					RegistryFilename = FollowupFilename;
				}
			}
		}
	}


	// We didn't find the other file by inference, at least not successfully, so we need to check again using the new filename from the user
	if (!FoundOtherFileByInference)
	{
		HashCheckResult = CheckFilesExistAndHashMatches(MetadataFilename, RegistryFilename, MetadataTemporaryStorage);
	}

	// We can continue loading if:
	// (a) We have both files and the hashes match
	// (b) We only got the development asset registry file
	const bool MetadataFileExists = FPaths::FileExists(*MetadataFilename);
	const bool RegistryFileExists = FPaths::FileExists(*RegistryFilename);
	bool ShouldTryFullLoad = (HashCheckResult == ECheckFilesExistAndHashMatchesResult::Okay) || (RegistryFileExists && !MetadataFileExists);
	
	bool Success = false;
	if (ShouldTryFullLoad)
	{
		// It's okay to stomp the SourceName since it will always be either uninitialized or CustomSourceName
		RegistrySource.SourceName = FAssetManagerEditorRegistrySource::CustomSourceName;
		if (EditorModule->PopulateRegistrySource(&RegistrySource, &RegistryFilename))
		{
			RequestRefreshAssets();
			Success = true;
		}
	}

	if (Success && (HashCheckResult == ECheckFilesExistAndHashMatchesResult::Okay))
	{
		CookMetadata = std::move(MetadataTemporaryStorage);
		UpdateRegistryInfoTextPostLoad(HashCheckResult);
	}
	else if (Success)
	{
		CookMetadata.Reset();
		UpdateRegistryInfoTextPostLoad(ECheckFilesExistAndHashMatchesResult::CookMetadataDoesNotExist);
	}
	else
	{
		ECheckFilesExistAndHashMatchesResult StatusResult = ECheckFilesExistAndHashMatchesResult::Unknown;

		if (ShouldTryFullLoad)
		{
			StatusResult = ECheckFilesExistAndHashMatchesResult::FailedToLoadRegistry;
		}
		else if (CanceledOpenDialog)
		{
			FooterLeftText = FooterLeftTextStoredPreOpen;
		}
		else if (!RegistryFileExists)
		{
			StatusResult = ECheckFilesExistAndHashMatchesResult::RegistryDoesNotExist;
		}
		else
		{
			StatusResult = HashCheckResult;
		}

		if (!CanceledOpenDialog)
		{
			CookMetadata.Reset();
			RegistrySource.ClearRegistry();
			UpdateRegistryInfoTextPostLoad(StatusResult);
			ClearTableAndTree();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

SAssetTableTreeView::ECheckFilesExistAndHashMatchesResult SAssetTableTreeView::CheckFilesExistAndHashMatches(const FString& MetadataFilename, const FString& RegistryFilename, UE::Cook::FCookMetadataState& MetadataTemporaryStorage)
{
	const bool MetadataFileExists = FPaths::FileExists(*MetadataFilename);
	const bool RegistryFileExists = FPaths::FileExists(*RegistryFilename);
	if (RegistryFileExists && MetadataFileExists)
	{
		uint64 RegistryFileActualHash = -1;
		bool GotActualRegistryFileHash = HashRegistryFile(RegistryFilename, &RegistryFileActualHash);
		if (GotActualRegistryFileHash)
		{
			uint64 RegistryFileStoredHash = -1;
			if (LoadCookMetadata(MetadataFilename, MetadataTemporaryStorage))
			{
				bool HashesMatch = MetadataTemporaryStorage.GetAssociatedDevelopmentAssetRegistryHashPostWriteback() == RegistryFileActualHash;
				if (!HashesMatch)
				{
					HashesMatch = MetadataTemporaryStorage.GetAssociatedDevelopmentAssetRegistryHash() == RegistryFileActualHash;
				}
				return HashesMatch ? ECheckFilesExistAndHashMatchesResult::Okay : ECheckFilesExistAndHashMatchesResult::HashesDoNotMatch;
			}
			else
			{
				return ECheckFilesExistAndHashMatchesResult::FailedToLoadCookMetadata;
			}
		}
		else
		{
			return ECheckFilesExistAndHashMatchesResult::FailedToHashRegistry;
		}
	}	
	else
	{
		if (!RegistryFileExists)
		{
			return ECheckFilesExistAndHashMatchesResult::RegistryDoesNotExist;
		}
		else
		{
			return ECheckFilesExistAndHashMatchesResult::CookMetadataDoesNotExist;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::UpdateRegistryInfoTextPostLoad(ECheckFilesExistAndHashMatchesResult StatusResult)
{
	if (StatusResult == ECheckFilesExistAndHashMatchesResult::Okay || StatusResult == ECheckFilesExistAndHashMatchesResult::CookMetadataDoesNotExist)
	{
		IFileManager& FileManager = IFileManager::Get();
		const FString RegistryFilePath = FileManager.GetFilenameOnDisk(*FileManager.ConvertToAbsolutePathForExternalAppForRead(*RegistrySource.SourceFilename));

		if (!CookMetadata.IsValid())
		{
			RegistryInfoText->SetText(FText::Format(
				LOCTEXT("RegistrySourceInfoText_Loaded_RegistryOnly", "Loaded: {0} from {1}. No cook metadata available."),
				FText::FromString(RegistryFilePath),
				FText::FromString(RegistrySource.SourceTimestamp)));
		}
		else
		{
			FString HordeJobIdString = (CookMetadata.GetHordeJobId().Len() > 0) ? CookMetadata.GetHordeJobId() : TEXT("<Unavailable>");
			RegistryInfoText->SetText(FText::Format(
				LOCTEXT("RegistrySourceInfoText_Loaded_WithMetadata", 
					"Loaded: {0} from build {1} on {2} with HordeJobId {3}. Platform: {4} (Sizes are {5})"),
				FText::FromString(RegistryFilePath),
				FText::FromString(CookMetadata.GetBuildVersion()),
				FText::FromString(RegistrySource.SourceTimestamp),
				FText::FromString(HordeJobIdString),
				FText::FromString(CookMetadata.GetPlatform()),
				CookMetadata.GetSizesPresentAsText()
			));
		}
	}
	else
	{
		FText InfoText;
		switch (StatusResult)
		{
		case ECheckFilesExistAndHashMatchesResult::RegistryDoesNotExist:
			InfoText = LOCTEXT("RegistrySourceInfoText_OpenRegistry_RegistryDoesNotExist", "Selected asset registry does not exist or is invalid.");
			break;
		case ECheckFilesExistAndHashMatchesResult::FailedToLoadCookMetadata:
			InfoText = LOCTEXT("RegistrySourceInfoText_OpenRegistry_CouldNotLoadMetadata", "Unable to load cook metadata.");
			break;
		case ECheckFilesExistAndHashMatchesResult::FailedToHashRegistry:
			InfoText = LOCTEXT("RegistrySourceInfoText_OpenRegistry_CouldNotHashAssetRegistry", "Unable to load registry to hash it.");
			break;
		case ECheckFilesExistAndHashMatchesResult::HashesDoNotMatch:
			InfoText = LOCTEXT("RegistrySourceInfoText_OpenRegistry_HashesDoNotMatch", "Hash of asset registry does not match either hash provided by the cook metadata file.");
			break;
		case ECheckFilesExistAndHashMatchesResult::FailedToLoadRegistry:
			InfoText = LOCTEXT("RegistrySourceInfoText_OpenRegistry_DevARLoadFailed", "Selected asset registry could not be loaded/parsed.");
			break;
		case ECheckFilesExistAndHashMatchesResult::Unknown:
			InfoText = LOCTEXT("RegistrySourceInfoText_OpenRegistry_Failed", "No registry selected or load failed. Reason unknown.");
			break;
		default:
			ensureAlwaysMsgf(false, TEXT("Unexpected status result in UpdateRegistryInfoTextPostLoad"));
			InfoText = LOCTEXT("RegistrySourceInfoText_OpenRegistry_UnknownError", "Internal error.");
			break;
		};
		RegistryInfoText->SetText(InfoText);
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
			*OutTotalSizeMultiplyUsed += GetAssetTable()->GetAssetChecked(ReferenceCountPair.Key).GetStagedCompressedSizeRequiredInstall();
		}
		else if (ReferenceCountPair.Value == 1)
		{
			*OutTotalSizeSingleUse += GetAssetTable()->GetAssetChecked(ReferenceCountPair.Key).GetStagedCompressedSizeRequiredInstall();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::TreeView_OnSelectionChanged(UE::Insights::FTableTreeNodePtr SelectedItem, ESelectInfo::Type SelectInfo)
{
	TArray<UE::Insights::FTableTreeNodePtr> SelectedNodes;
	const int32 NumSelectedNodes = TreeView->GetSelectedItems(SelectedNodes);
	int32 NewlySelectedAssetRowIndex = -1;
	int32 NewlySelectedPluginIndex = -1;
	SelectedAssetIndices.Empty();
	SelectedPluginIndices.Empty();

	for (const UE::Insights::FTableTreeNodePtr& Node : SelectedNodes)
	{
		if (Node->Is<FPluginSimpleGroupNode>())
		{
			// A plugin or its wrapper is selected
			const FPluginSimpleGroupNode& PluginNode = Node->As<FPluginSimpleGroupNode>();
			NewlySelectedPluginIndex = PluginNode.GetPluginIndex();
			SelectedPluginIndices.Add(NewlySelectedPluginIndex);
		}
		else if (Node->Is<FAssetTreeNode>())
		{
			if (Node->As<FAssetTreeNode>().IsValidAsset())
			{
				NewlySelectedAssetRowIndex = StaticCastSharedPtr<FAssetTreeNode>(Node)->GetRowIndex();
				SelectedAssetIndices.Add(NewlySelectedAssetRowIndex);
			}
		}
	}

	int32 NumSelectedAssets = SelectedAssetIndices.Num();
	int32 NumSelectedPlugins = SelectedPluginIndices.Num();

	const int32 FilteredAssetCount = FilteredNodesPtr->Num();
	const int32 VisibleAssetCount = GetAssetTable()->GetVisibleAssetCount();

	if (NumSelectedAssets == 0)
	{
		FooterCenterText1 = FText();
		FooterCenterText2 = FText();

		if (NumSelectedPlugins > 0)
		{
			FooterLeftText = FText::Format(LOCTEXT("FootLeftTextFmt_PluginsSelected", "{0} Plugins ({1} Selected)"),
								FText::AsNumber(GetAssetTable()->GetNumPlugins()),
								FText::AsNumber(NumSelectedPlugins));

			int64 TotalSelfSize = 0;
			int64 TotalInclusiveSize = 0;
			FAssetTablePluginInfo::ComputeTotalSelfAndInclusiveSizes(*GetAssetTable(), SelectedPluginIndices, TotalSelfSize, TotalInclusiveSize);
			FooterRightText1 = FText::Format(LOCTEXT("FooterRightTextFmt_PluginsSelected", "Self: {0} Inclusive: {1}"),
								FText::AsMemory(TotalSelfSize),
								FText::AsMemory(TotalInclusiveSize));

		}
		else
		{
			if (FilteredAssetCount != VisibleAssetCount)
			{
				FooterLeftText = FText::Format(LOCTEXT("FooterLeftTextFmt_NoSelected_Filtered", "{0} / {1} assets"), FText::AsNumber(FilteredAssetCount), FText::AsNumber(VisibleAssetCount));
			}
			else
			{
				FooterLeftText = FText::Format(LOCTEXT("FooterLeftTextFmt_NoSelected_NoFiltered", "{0} assets"), FText::AsNumber(VisibleAssetCount));
			}
			FooterRightText1 = FText();
		}
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
			FText::AsMemory(AssetTableRow.GetStagedCompressedSizeRequiredInstall()),
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
			for (int32 SelectedNodeIndex : SelectedAssetIndices)
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


		int64 TotalExternalDependencySize = FAssetTableRow::ComputeTotalSizeExternalDependencies(*GetAssetTable(), SelectedAssetIndices);
		FAssetTableDependencySizes Sizes = FAssetTableRow::ComputeDependencySizes(*GetAssetTable(), SelectedAssetIndices, nullptr, nullptr);

		int64 TotalSelfSize = 0;
		for (int32 Index : SelectedAssetIndices)
		{
			TotalSelfSize += GetAssetTable()->GetAssetChecked(Index).GetStagedCompressedSizeRequiredInstall();
		}

		FText BaseAndMarginalCost;
		if (AllSelectedNodesAreSameType)
		{
			int64 TotalSizeMultiplyUsed = 0;
			int64 TotalSizeSingleUse = 0;
			CalculateBaseAndMarginalCostForSelection(SelectedAssetIndices, &TotalSizeMultiplyUsed, &TotalSizeSingleUse);

			BaseAndMarginalCost = FText::Format(LOCTEXT("FooterLeft_BaseAndMarginalCost", " -- Base Cost: {0}  Per Asset Cost: {1}"),
				FText::AsMemory(TotalSizeMultiplyUsed),
				FText::AsMemory((TotalSelfSize + TotalSizeSingleUse) / SelectedAssetIndices.Num()));
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

	bool RegistryHasSeparateCompressedSizes = EditorModule->GetIntegerValueForCustomColumn(AssetData, UE::AssetRegistry::Stage_ChunkInstalledSizeFName, OutRow.StagedCompressedSizeRequiredInstall, &RegistrySource);
	if (!RegistryHasSeparateCompressedSizes)
	{
		// This should only apply to legacy asset registries (before ~6/28/23).
		EditorModule->GetIntegerValueForCustomColumn(AssetData, UE::AssetRegistry::Stage_ChunkCompressedSizeFName, OutRow.StagedCompressedSizeRequiredInstall, &RegistrySource);
	}
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
	OutRow.Color = USlateThemeManager::Get().GetColor((EStyleColor)((uint32)EStyleColor::AccentBlue + AssetTypeHash % ((uint32)EStyleColor::AccentGreen - (uint32)EStyleColor::AccentBlue)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// Refresh list of assets for the tree view
void SAssetTableTreeView::RefreshAssets()
{
	CancelCurrentAsyncOp();
	TSharedPtr<FAssetTable> AssetTable = GetAssetTable();
	if (!AssetTable.IsValid())
	{
		return;
	}

	UE_LOG(LogInsights, Log, TEXT("[AssetTree] Build asset table..."));

	UE::Insights::FStopwatch Stopwatch;
	Stopwatch.Start();

	ClearTableAndTree();

	TMap<FString, int64> PluginToSizeMap;

	TMap<const TCHAR*, DeprecatedTCharSetType, FDefaultSetAllocator, TStringPointerMapKeyFuncs_DEPRECATED<const TCHAR*, DeprecatedTCharSetType>> DiscoveredPluginDependencyEdges;

	TMap<FAssetData, int32> AssetToIndexMap;
	TArray<FAssetData> SourceAssets;

	if (IsRegistrySourceValid())
	{
		RegistrySource.GetOwnedRegistryState()->GetAllAssets(TSet<FName>(), SourceAssets);

		for (int32 SourceAssetIndex = 0; SourceAssetIndex < SourceAssets.Num(); SourceAssetIndex++)
		{
			TArrayView<FAssetData const* const> AssetsInSourcePackage = RegistrySource.GetOwnedRegistryState()->GetAssetsByPackageName(SourceAssets[SourceAssetIndex].PackageName);

			for (FAssetData const* const SourceAsset : AssetsInSourcePackage)
			{
				if (AssetToIndexMap.Find(*SourceAsset) == nullptr)
				{
					FAssetTableRow AssetRow;
					PopulateAssetTableRow(AssetRow, *SourceAsset, *AssetTable);
					AssetTable->AddAsset(AssetRow);
					AssetToIndexMap.Add(*SourceAsset, AssetTable->GetTotalAssetCount() - 1);

					if (int64* PluginSize = PluginToSizeMap.Find(AssetRow.GetPluginName()))
					{
						(*PluginSize) += AssetRow.GetStagedCompressedSizeRequiredInstall();
					}
					else
					{
						PluginToSizeMap.Add(AssetRow.GetPluginName(), AssetRow.GetStagedCompressedSizeRequiredInstall());
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
			TArrayView<FAssetData const* const> AssetsInSourcePackage = RegistrySource.GetOwnedRegistryState()->GetAssetsByPackageName(SourceAssets[SourceAssetIndex].PackageName);

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
							(*PluginSize) += NewRow.GetStagedCompressedSizeRequiredInstall();
						}
						else
						{
							PluginToSizeMap.Add(NewRow.GetPluginName(), NewRow.GetStagedCompressedSizeRequiredInstall());
						}
					}
				}
			}

			if (IndicesOfDependenciesInRowTableToAddToCurrentSourceAssetRow.Num())
			{
				// We found some dependencies. Let's add them.
				for (const FAssetData* const SourceAssetData : AssetsInSourcePackage)
				{
					int32* RowIndex = AssetToIndexMap.Find(*SourceAssetData);
					if (RowIndex == nullptr)
					{
						UE_LOG(LogInsights, Warning, TEXT("Failed to find asset %s in package %s, source asset index %d."), *SourceAssetData->AssetName.ToString(), *SourceAssetData->PackageName.ToString(), SourceAssetIndex);
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
						for (const FAssetData* SourceAssetData : AssetsInSourcePackage)
						{
							int32* RowIndex = AssetToIndexMap.Find(*SourceAssetData);
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

	if (CookMetadata.IsValid())
	{
		// Shader data
		{
			const UE::Cook::FCookMetadataShaderPseudoHierarchy& ShaderHierarchy = CookMetadata.GetShaderPseudoHierarchy();

			const TCHAR* ShaderAssetType = AssetTable->StoreStr(TEXT("Shader Pseudo Asset"));
			const uint32 AssetTypeHash = GetTypeHash(FStringView(ShaderAssetType));
			FLinearColor ShaderColor = USlateThemeManager::Get().GetColor((EStyleColor)((uint32)EStyleColor::AccentBlue + AssetTypeHash % ((uint32)EStyleColor::AccentGreen - (uint32)EStyleColor::AccentBlue)));

			uint64 ShaderStartIndexInAssetTable = AssetTable->GetAssets().Num();

			// Build all the additional asset rows for the ShaderAssets.
			for (const UE::Cook::FCookMetadataShaderPseudoAsset& ShaderAsset : ShaderHierarchy.ShaderAssets)
			{
				FAssetTableRow AssetRow;
				{
					int FirstSlashIndex = INDEX_NONE;
					ShaderAsset.Name.FindChar(TEXT('/'), FirstSlashIndex);
					FString PluginName;
					if (FirstSlashIndex != INDEX_NONE)
					{
						PluginName = ShaderAsset.Name.Left(FirstSlashIndex);
					}
					AssetRow.PluginName = AssetTable->StoreStr(PluginName);

					int LastSlashIndex = INDEX_NONE;
					ShaderAsset.Name.FindLastChar(TEXT('/'), LastSlashIndex);
					FString AssetName;
					if (FirstSlashIndex != INDEX_NONE)
					{
						AssetName = ShaderAsset.Name.Right(ShaderAsset.Name.Len() - LastSlashIndex - 1);
					}
					AssetRow.Name = AssetTable->StoreStr(AssetName);
				}
				AssetRow.Type = ShaderAssetType;
				AssetRow.Color = ShaderColor;
				AssetRow.StagedCompressedSizeRequiredInstall = ShaderAsset.CompressedSize;
				AssetTable->AddAsset(AssetRow);
			}
			
			// This is not efficient as most packages don't have shader dependencies at all
			for (const FAssetData& SourceAsset : SourceAssets)
			{
				if (const TPair<int32, int32>* ShaderDependencyRange = ShaderHierarchy.PackageShaderDependencyMap.Find(SourceAsset.PackageName))
				{
					if (int32* AssetIndex = AssetToIndexMap.Find(SourceAsset))
					{
						FAssetTableRow* OwningAssetRow = AssetTable->GetAsset(*AssetIndex);

						for (int32 ShaderPseudoAssetIndex = ShaderDependencyRange->Key; ShaderPseudoAssetIndex < ShaderDependencyRange->Value; ShaderPseudoAssetIndex++)
						{
							int64 ShaderIndexInAssetTable = ShaderStartIndexInAssetTable + ShaderHierarchy.DependencyList[ShaderPseudoAssetIndex];
							if (ensure(ShaderIndexInAssetTable < AssetTable->GetAssets().Num()))
							{
								OwningAssetRow->Dependencies.AddUnique(ShaderIndexInAssetTable);
								AssetTable->GetAsset(ShaderIndexInAssetTable)->Referencers.AddUnique(*AssetIndex);
							}
						}
					}
				}
			}

			AssetTable->SetVisibleAssetCount(AssetTable->GetAssets().Num());
		}
		
		// Plugin Data
		{
			const UE::Cook::FCookMetadataPluginHierarchy& PluginHierarchy = CookMetadata.GetPluginHierarchy();

			for (int32 CustomColumnIndex = 0; CustomColumnIndex < PluginHierarchy.CustomFieldEntries.Num(); CustomColumnIndex++)
			{
				FAssetTable::FCustomColumnDefinition CustomColumnDefinition;
				CustomColumnDefinition.ColumnId = FName(PluginHierarchy.CustomFieldEntries[CustomColumnIndex].Name);
				UE::Cook::ECookMetadataCustomFieldType FieldType = PluginHierarchy.CustomFieldEntries[CustomColumnIndex].Type;
				if (ensureMsgf(FieldType != UE::Cook::ECookMetadataCustomFieldType::Unknown, TEXT("Unknown field type encountered")))
				{
					if (FieldType == UE::Cook::ECookMetadataCustomFieldType::String)
					{
						CustomColumnDefinition.Type = FAssetTable::ECustomColumnDefinitionType::String;
					}
					else if (FieldType == UE::Cook::ECookMetadataCustomFieldType::Bool)
					{
						CustomColumnDefinition.Type = FAssetTable::ECustomColumnDefinitionType::Boolean;
					}
				}
				CustomColumnDefinition.Key = CustomColumnIndex;
				AssetTable->AddCustomColumn(CustomColumnDefinition);
			}

			int64 TotalMismatch = 0;
			int64 TotalSize = 0;
			for (const UE::Cook::FCookMetadataPluginEntry& PluginEntry : PluginHierarchy.PluginsEnabledAtCook)
			{
				const TCHAR* StoredPluginName = AssetTable->StoreStr(PluginEntry.Name);
				FAssetTablePluginInfo& PluginInfo = AssetTable->GetOrCreatePluginInfo(StoredPluginName);
				if (CookMetadata.GetSizesPresent() != UE::Cook::ECookMetadataSizesPresent::NotPresent)
				{
					PluginInfo.Size = PluginEntry.ExclusiveSizes[UE::Cook::EPluginSizeTypes::Installed];
				}
				else if (const int64* SizePtr = PluginToSizeMap.Find(PluginEntry.Name))
				{
					PluginInfo.Size = *SizePtr;
				}

				PluginInfo.PluginTypeString = AssetTable->StoreStr(*PluginEntry.GetPluginTypeAsText().ToString());

				for (int32 CustomColumnIndex = 0; CustomColumnIndex < PluginHierarchy.CustomFieldEntries.Num(); CustomColumnIndex++)
				{
					if (const UE::Cook::FCookMetadataPluginEntry::CustomFieldVariantType* VariantEntry = PluginEntry.CustomFields.Find(CustomColumnIndex))
					{
						FAssetTablePluginInfo::FCustomColumnData ColumnData;
						ColumnData.Key = CustomColumnIndex;

						if (PluginHierarchy.CustomFieldEntries[CustomColumnIndex].Type == UE::Cook::ECookMetadataCustomFieldType::Bool)
						{
							ColumnData.Value.Set<bool>(VariantEntry->Get<bool>());
						}
						else if (PluginHierarchy.CustomFieldEntries[CustomColumnIndex].Type == UE::Cook::ECookMetadataCustomFieldType::String)
						{
							ColumnData.Value.Set<const TCHAR*>(AssetTable->StoreStr(*VariantEntry->Get<FString>()));
						}

						PluginInfo.CustomColumnData.Add(ColumnData);
					}
				}

				///
				/// validation
				///
				if (const int64* SizePtr = PluginToSizeMap.Find(PluginEntry.Name))
				{
					if (CookMetadata.GetSizesPresent() != UE::Cook::ECookMetadataSizesPresent::NotPresent)
					{
						int64 TotalSizeOfPluginInMetadata = PluginEntry.ExclusiveSizes[UE::Cook::EPluginSizeTypes::Installed];
						if (*SizePtr != TotalSizeOfPluginInMetadata)
						{
							UE_LOG(LogInsights, Warning, TEXT("Plugin %s found with mismatched ucookmetadata and internal asset size calculation. Metadata size: %lld Calculated size: %lld"),
								StoredPluginName, PluginInfo.Size, *SizePtr);
						}
						TotalMismatch += FMath::Abs(*SizePtr - TotalSizeOfPluginInMetadata);
						TotalSize += TotalSizeOfPluginInMetadata;
					}
				}

				const int32 PluginIndex = AssetTable->GetIndexForPlugin(StoredPluginName);
				for (uint32 DependencyIndexInMetadata = PluginEntry.DependencyIndexStart; DependencyIndexInMetadata < PluginEntry.DependencyIndexEnd; DependencyIndexInMetadata++)
				{
					const UE::Cook::FCookMetadataPluginEntry& DependentPluginEntry = PluginHierarchy.PluginsEnabledAtCook[PluginHierarchy.PluginDependencies[DependencyIndexInMetadata]];
					const TCHAR* StoredReferencePluginName = AssetTable->StoreStr(DependentPluginEntry.Name);
					int32 DependencyIndex = AssetTable->GetIndexForPlugin(StoredReferencePluginName);
					if (DependencyIndex == -1)
					{
						DependencyIndex = AssetTable->GetNumPlugins();
						AssetTable->GetOrCreatePluginInfo(StoredReferencePluginName);
					}
					// Note that we can't use PluginInfo directly because the above code could have grown the array 
					// and that would invalidate the reference
					AssetTable->GetOrCreatePluginInfo(StoredPluginName).PluginDependencies.AddUnique(DependencyIndex);
					AssetTable->GetPluginInfoByIndex(DependencyIndex).PluginReferencers.AddUnique(AssetTable->GetIndexForPlugin(StoredPluginName));
				}
			}

			if (TotalMismatch != 0)
			{
				UE_LOG(LogInsights, Warning, TEXT("Total Size of Plugins from Metadata: %lld // Total Discrepancy (vs calculated size from assets): %lld"), TotalSize, TotalMismatch);
			}

			for (uint16 RootPluginIndex : PluginHierarchy.RootPlugins)
			{
				const UE::Cook::FCookMetadataPluginEntry& PluginEntry = PluginHierarchy.PluginsEnabledAtCook[RootPluginIndex];
				const TCHAR* StoredPluginName = AssetTable->StoreStr(PluginEntry.Name);
				FAssetTablePluginInfo& PluginInfo = AssetTable->GetOrCreatePluginInfo(StoredPluginName);
				UE_LOG(LogInsights, Display, TEXT("Found root plugin %s"), StoredPluginName);
				PluginInfo.bIsRootPlugin = true;
			}
		}
	}
	else
	{
		UE_LOG(LogInsights, Warning, TEXT("CookMetadata not available, deriving plugin dependencies and sizes from loaded asset registry instead."));
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
				const int32 PluginIndex = AssetTable->GetIndexForPlugin(StoredPluginName);
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
					AssetTable->GetOrCreatePluginInfo(StoredPluginName).PluginDependencies.AddUnique(DependencyIndex);
					AssetTable->GetPluginInfoByIndex(DependencyIndex).PluginReferencers.AddUnique(AssetTable->GetIndexForPlugin(StoredPluginName));
				}
			}
			else
			{
				UE_LOG(LogInsights, Warning, TEXT("Could not find plugin %s in plugin manager."), PluginInfo.PluginName);
			}
		}
	}

	for (int32 PluginIndex = 0; PluginIndex < AssetTable->GetNumPlugins(); PluginIndex++)
	{
		const FAssetTablePluginInfo& PluginInfo = AssetTable->GetPluginInfoByIndex(PluginIndex);
		if (!PluginInfo.IsRootPlugin() && PluginInfo.GetNumReferencers() == 0 && PluginInfo.GetSize() > 0)
		{
			UE_LOG(LogInsights, Warning, TEXT("Found orphaned plugin %s"), PluginInfo.GetName());
		}
	}

	// Disabling this for now as there are too many differences at the moment. Keeping the code because eventually
	// we probably do want to use it to discover spurious dependencies or dependencies on code+content where content should
	// get separated from code
	// 
	// DumpDifferencesBetweenDiscoveredDataAndLoadedMetadata(DiscoveredPluginDependencyEdges);


	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	UE_LOG(LogInsights, Log, TEXT("[AssetTree] Asset table rebuilt in %.4fs (%d visible + %d hidden = %d assets)"),
		TotalTime, AssetTable->GetVisibleAssetCount(), AssetTable->GetHiddenAssetCount(), AssetTable->GetTotalAssetCount());

	const FAssetTableStringStore& StringStore = AssetTable->GetStringStore();
	UE_LOG(LogInsights, Log, TEXT("[AssetTree] String store: %d strings (%lld bytes) --> %d strings (%lld bytes | %lld bytes) %.0f%%"),
		StringStore.GetNumInputStrings(), StringStore.GetTotalInputStringSize(),
		StringStore.GetNumStrings(), StringStore.GetTotalStringSize(), StringStore.GetAllocatedSize(),
		(double)StringStore.GetAllocatedSize() * 100.0 / (double)StringStore.GetTotalInputStringSize());

	RequestRebuildTree(/*NeedsColumnRebuild =*/true);
}

void SAssetTableTreeView::ClearTableAndTree()
{
	TSharedPtr<FAssetTable> AssetTable = GetAssetTable();

	// Clears all tree nodes (that references the previous assets).
	AssetTable->SetVisibleAssetCount(0);
	RebuildTree(true);

	// Now is safe to clear the previous assets.
	AssetTable->ClearAllData();
}

void SAssetTableTreeView::DumpDifferencesBetweenDiscoveredDataAndLoadedMetadata(TMap<const TCHAR*, DeprecatedTCharSetType, FDefaultSetAllocator, TStringPointerMapKeyFuncs_DEPRECATED<const TCHAR*, DeprecatedTCharSetType>>& DiscoveredPluginDependencyEdges) const
{
	TSharedPtr<FAssetTable> AssetTable = GetAssetTable();

	if (CookMetadata.IsValid())
	{
		// Report discrepancies
		TArray<TPair<FString, FString>> DependenciesInMetadataNotAssets;
		TArray<TPair<FString, FString>> DependenciesInAssetsNotMetadata;

		TMap<FString, int32> PluginNameToIndexInHierarchyMetadata;
		for (int32 IndexInHierarchyMetadata = 0; IndexInHierarchyMetadata < CookMetadata.GetPluginHierarchy().PluginsEnabledAtCook.Num(); IndexInHierarchyMetadata++)
		{
			PluginNameToIndexInHierarchyMetadata.Add(CookMetadata.GetPluginHierarchy().PluginsEnabledAtCook[IndexInHierarchyMetadata].Name, IndexInHierarchyMetadata);
		}

		// Search through our discovered dependencies and ensure that we have a matching dependency in the metadata
		for (const TPair<const TCHAR*, DeprecatedTCharSetType>& DiscoveredDependencies : DiscoveredPluginDependencyEdges)
		{
			int32* IndexInMetadata = PluginNameToIndexInHierarchyMetadata.Find(DiscoveredDependencies.Key);
			if (IndexInMetadata == nullptr)
			{
				UE_LOG(LogInsights, Warning, TEXT("Unable to find plugin %s, known from asset traversal, in cook metadata."), DiscoveredDependencies.Key);
			}
			else
			{
				for (const TCHAR* DiscoveredDependency : DiscoveredDependencies.Value)
				{
					if (int32* IndexOfDependencyInMetadata = PluginNameToIndexInHierarchyMetadata.Find(DiscoveredDependency))
					{
						// Now make sure it's actually in the list of dependencies
						bool FoundDependency = false;
						int32 StartIndex = CookMetadata.GetPluginHierarchy().PluginsEnabledAtCook[*IndexInMetadata].DependencyIndexStart;
						int32 EndIndex = CookMetadata.GetPluginHierarchy().PluginsEnabledAtCook[*IndexInMetadata].DependencyIndexEnd;
						for (int32 MetadataDependencyIndex = StartIndex; MetadataDependencyIndex < EndIndex; MetadataDependencyIndex++)
						{
							if (*IndexOfDependencyInMetadata == MetadataDependencyIndex)
							{
								FoundDependency = true;
								break;
							}
						}

						if (!FoundDependency)
						{
							DependenciesInAssetsNotMetadata.Add(TPair<FString, FString>(DiscoveredDependencies.Key, DiscoveredDependency));
						}
					}
					else
					{
						UE_LOG(LogInsights, Warning, TEXT("Unable to find plugin %s, known from asset traversal, in cook metadata."), DiscoveredDependency);
					}
				}

			}
		}

		// Search through the metadata and ensure that we've found a corresponding asset dependency
		for (int32 PluginIndex = 0; PluginIndex < AssetTable->GetNumPlugins(); PluginIndex++)
		{
			int32* IndexInMetadata = PluginNameToIndexInHierarchyMetadata.Find(AssetTable->GetPluginInfoByIndex(PluginIndex).GetName());
			const FAssetTablePluginInfo& PluginInfoInTable = AssetTable->GetPluginInfoByIndex(PluginIndex);
			if (IndexInMetadata == nullptr)
			{
				UE_LOG(LogInsights, Warning, TEXT("Unable to find plugin %s in metadata."), PluginInfoInTable.GetName());
			}
			else
			{
				const TCHAR* PluginName = AssetTable->GetPluginInfoByIndex(PluginIndex).GetName();
				if (DeprecatedTCharSetType* DependencySet = DiscoveredPluginDependencyEdges.Find(PluginName))
				{
					for (int32 DependencyIndex : PluginInfoInTable.GetDependencies())
					{
						const TCHAR* DependencyName = AssetTable->GetPluginInfoByIndex(DependencyIndex).GetName();
						if (!DependencySet->Contains(DependencyName))
						{
							DependenciesInMetadataNotAssets.Add(TPair<FString, FString>(PluginName, DependencyName));
						}
					}
				}
				else
				{
					UE_LOG(LogInsights, Warning, TEXT("Unable to find plugin %s, known from metadata, in asset traversal data"), PluginInfoInTable.GetName());
				}
			}
		}

		if (DependenciesInAssetsNotMetadata.Num() > 0)
		{
			UE_LOG(LogInsights, Warning, TEXT("Dependencies in assets but not in cook metadata:"));
			for (const TPair<FString, FString>& Edge : DependenciesInAssetsNotMetadata)
			{
				UE_LOG(LogInsights, Warning, TEXT("%s-->%s"), *Edge.Key, *Edge.Value);
			}
		}

		if (DependenciesInMetadataNotAssets.Num() > 0)
		{
			UE_LOG(LogInsights, Warning, TEXT("Dependencies in cook metadata but not in assets:"));
			for (const TPair<FString, FString>& Edge : DependenciesInMetadataNotAssets)
			{
				UE_LOG(LogInsights, Warning, TEXT("%s-->%s"), *Edge.Key, *Edge.Value);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SAssetTableTreeView::RequestRebuildTree(bool NeedsColumnRebuild)
{
	bNeedsToRebuild = true;
	bNeedsToRebuildColumns = NeedsColumnRebuild;
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

	CancelCurrentAsyncOp();

	UE::Insights::FStopwatch Stopwatch;
	Stopwatch.Start();

	UE::Insights::FStopwatch SyncStopwatch;
	SyncStopwatch.Start();

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
