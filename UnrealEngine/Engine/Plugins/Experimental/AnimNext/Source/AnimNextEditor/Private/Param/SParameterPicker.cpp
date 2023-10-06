// Copyright Epic Games, Inc. All Rights Reserved.

#include "SParameterPicker.h"

#include "Param/AnimNextParameterBlock.h"
#include "UncookedOnlyUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Param/ParamType.h"
#include "Param/AnimNextParameter.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Param/AnimNextParameterLibrary.h"
#include "DetailLayoutBuilder.h"
#include "EditorUtils.h"
#include "Param/ParameterPickerArgs.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "SParameterPicker"

namespace UE::AnimNext::Editor
{

namespace ParameterPicker
{
static FName Column_Parameter(TEXT("Parameter"));
static FName Column_Library(TEXT("Library"));
static FName Column_Block(TEXT("Block"));
}

struct FParameterPickerEntry
{
	FParameterPickerEntry() = default;

	FParameterPickerEntry(const FParameterBindingReference& InBinding, const FAnimNextParamType& InParamType)
		: Binding(InBinding)
		, ParamType(InParamType)
	{
		PinType = UE::AnimNext::UncookedOnly::FUtils::GetPinTypeFromParamType(ParamType);
		PinIcon = FBlueprintEditorUtils::GetIconFromPin(PinType, /* bIsLarge = */true);
		PinColor = GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);
	}

	bool PassesFilter(const FString& InFilterText) const
	{
		return Binding.Parameter.ToString().Contains(InFilterText);
	}

	FParameterBindingReference Binding;

	FAnimNextParamType ParamType;

	FEdGraphPinType PinType;

	const FSlateBrush* PinIcon;

	FLinearColor PinColor;
};

void SParameterPicker::Construct(const FArguments& InArgs)
{
	using namespace ParameterPicker;

	Args = InArgs._Args;

	if(Args.OnGetParameterBindings != nullptr)
	{
		Args.OnGetParameterBindings->BindSP(this, &SParameterPicker::HandleGetParameterBindings);
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SSearchBox)
			.OnTextChanged_Lambda([this](FText InText)
			{
				FilterText = InText;
				RefreshFilter();
			})
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2.0f)
		[
			SAssignNew(EntriesList, SListView<TSharedRef<FParameterPickerEntry>>)
			.ListItemsSource(&FilteredEntries)
			.OnGenerateRow(this, &SParameterPicker::HandleGenerateRow)
			.OnSelectionChanged(this, &SParameterPicker::HandleSelectionChanged)
			.ItemHeight(20.0f)
			.HeaderRow(
				SNew(SHeaderRow)
				+SHeaderRow::Column(Column_Parameter)
				.DefaultLabel(LOCTEXT("ParameterColumnHeader", "Parameter"))
				.ToolTipText(LOCTEXT("ParameterColumnHeaderTooltip", "The parameter"))
				.FillWidth(0.33f)
				
				+SHeaderRow::Column(Column_Library)
				.DefaultLabel(LOCTEXT("LibraryColumnHeader", "Library"))
				.ToolTipText(LOCTEXT("LibraryColumnHeaderTooltip", "The library that the parameter is declared in"))
				.FillWidth(0.33f)
				
				+SHeaderRow::Column(Column_Block)
				.DefaultLabel(LOCTEXT("BlockColumnHeader", "Block"))
				.ToolTipText(LOCTEXT("BlockColumnHeaderTooltip", "The parameter block that has a binding to the parameter"))
				.FillWidth(0.33f)
			)
		]
	];

	RefreshEntries();
}

void SParameterPicker::RefreshEntries()
{
	Entries.Empty();

	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter ARFilter;

	TSet<TTuple<FName, FAssetData>> BoundParameters;

	TMap<FAssetData, FAnimNextParameterLibraryAssetRegistryExports> LibraryExportMap;

	auto GetExportsForLibrary = [&LibraryExportMap](const FAssetData& InLibrary) -> const FAnimNextParameterLibraryAssetRegistryExports&
	{
		if(FAnimNextParameterLibraryAssetRegistryExports* ExistingExports = LibraryExportMap.Find(InLibrary))
		{
			return *ExistingExports;
		}

		FAnimNextParameterLibraryAssetRegistryExports& NewExports = LibraryExportMap.Add(InLibrary);
		FUtils::GetExportedParametersForLibrary(InLibrary, NewExports);
		return NewExports;
	};
	
	// Find all blocks and their bound parameters
	if(Args.bShowBoundParameters)
	{
		ARFilter.ClassPaths = { UAnimNextParameterBlock::StaticClass()->GetClassPathName() };
		
		TArray<FAssetData> BlockAssets;
		AssetRegistry.GetAssets(ARFilter, BlockAssets);

		for(const FAssetData& BlockAsset : BlockAssets)
		{
			FAnimNextParameterBlockAssetRegistryExports Exports;
			if(FUtils::GetExportedBindingsForBlock(BlockAsset, Exports))
			{
				for(const FAnimNextParameterBlockAssetRegistryExportEntry& Export : Exports.Bindings)
				{
					const FSoftObjectPath LibraryAssetPath(Export.Library);
					FAssetData LibraryAsset = AssetRegistry.GetAssetByObjectPath(LibraryAssetPath);
					if(LibraryAsset.IsValid())
					{
						BoundParameters.Add({ Export.Name, LibraryAsset });

						FParameterBindingReference NewReference(Export.Name, LibraryAsset, BlockAsset);
						if(!Args.OnFilterParameter.IsBound() || Args.OnFilterParameter.Execute(NewReference) == EFilterParameterResult::Include)
						{
							const FAnimNextParameterLibraryAssetRegistryExports& LibraryExports = GetExportsForLibrary(LibraryAsset);
							FAnimNextParamType ParamType = FUtils::GetParameterTypeFromLibraryExports(Export.Name, LibraryExports);

							TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(NewReference, ParamType);
							Entries.Add(NewEntry);
						}
					}
				}
			};
		}
	}

	// Find all library parameters (that have not already been added as bound above)
	if(Args.bShowUnboundParameters)
	{
		ARFilter.ClassPaths = { UAnimNextParameterLibrary::StaticClass()->GetClassPathName() };

		TArray<FAssetData> LibraryAssets;
		AssetRegistry.GetAssets(ARFilter, LibraryAssets);

		for(const FAssetData& LibraryAsset : LibraryAssets)
		{
			const FAnimNextParameterLibraryAssetRegistryExports& Exports = GetExportsForLibrary(LibraryAsset);
			for(const FAnimNextParameterLibraryAssetRegistryExportEntry& Export : Exports.Parameters)
			{
				if(!BoundParameters.Contains( { Export.Name, LibraryAsset } ))
				{
					FParameterBindingReference NewReference(Export.Name, LibraryAsset);
					if(!Args.OnFilterParameter.IsBound() || Args.OnFilterParameter.Execute(NewReference) == EFilterParameterResult::Include)
					{
						TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(NewReference, Export.Type);
						Entries.Add(NewEntry);
					}
				}
			}
		}
	}

	RefreshFilter();
}

void SParameterPicker::RefreshFilter()
{
	FilteredEntries.Empty();

	const FString FilterTextAsString = FilterText.ToString();
	for(const TSharedRef<FParameterPickerEntry>& Entry : Entries)
	{
		if(Entry->PassesFilter(FilterTextAsString))
		{
			FilteredEntries.Add(Entry);
		}
	}

	EntriesList->RequestListRefresh();
}

class SParameterPickerRow : public SMultiColumnTableRow<TSharedRef<FParameterPickerEntry>>
{
	SLATE_BEGIN_ARGS(SParameterPickerRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<FParameterPickerEntry> InEntry)
	{
		Entry = InEntry;

		SMultiColumnTableRow<TSharedRef<FParameterPickerEntry>>::Construct( SMultiColumnTableRow<TSharedRef<FParameterPickerEntry>>::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		using namespace ParameterPicker;

		if(InColumnName == Column_Parameter)
		{
			return
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(Entry->PinIcon)
					.ColorAndOpacity(Entry->PinColor)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(FText::FromName(Entry->Binding.Parameter))
					.ToolTipText(FText::FromName(Entry->Binding.Parameter))
				];
		}
		else if(InColumnName == Column_Library)
		{
			if(Entry->Binding.Library.IsValid())
			{
				
				return
					SNew(SBox)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(FText::FromName(Entry->Binding.Library.AssetName))
						.ToolTipText(FText::FromName(Entry->Binding.Library.PackageName))
					];
			}
		}
		else if(InColumnName == Column_Block)
		{
			if(Entry->Binding.Block.IsValid())
			{
				return
					SNew(SBox)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(FText::FromName(Entry->Binding.Block.AssetName))
						.ToolTipText(FText::FromName(Entry->Binding.Block.PackageName))
					];
			}
		}

		return SNullWidget::NullWidget;
	}
	
	TSharedPtr<FParameterPickerEntry> Entry;
};

TSharedRef<ITableRow> SParameterPicker::HandleGenerateRow(TSharedRef<FParameterPickerEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable) const
{
	return SNew(SParameterPickerRow, InOwnerTable, InEntry);
}

void SParameterPicker::HandleSelectionChanged(TSharedPtr<FParameterPickerEntry> InEntry, ESelectInfo::Type InSelectInfo)
{
	Args.OnSelectionChanged.ExecuteIfBound();
}

void SParameterPicker::HandleGetParameterBindings(TArray<FParameterBindingReference>& OutParameterBindings) const
{
	TArray<TSharedRef<FParameterPickerEntry>> SelectedEntries;
	EntriesList->GetSelectedItems(SelectedEntries);

	for(TSharedRef<FParameterPickerEntry>& Entry : SelectedEntries)
	{
		OutParameterBindings.Emplace(Entry->Binding);
	}
}

}

#undef LOCTEXT_NAMESPACE