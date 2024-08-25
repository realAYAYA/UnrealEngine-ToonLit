// Copyright Epic Games, Inc. All Rights Reserved.

#include "SParameterPicker.h"

#include "Param/AnimNextParameterBlock.h"
#include "UncookedOnlyUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Param/ParamType.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "DetailLayoutBuilder.h"
#include "EditorUtils.h"
#include "SAddParametersDialog.h"
#include "Param/ParameterPickerArgs.h"
#include "Widgets/Input/SSearchBox.h"
#include "String/ParseTokens.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"
#include "SSimpleButton.h"
#include "Param/ExternalParameterRegistry.h"

#define LOCTEXT_NAMESPACE "SParameterPicker"

namespace UE::AnimNext::Editor
{

namespace ParameterPicker
{
static FName Column_Parameter(TEXT("Parameter"));
static FName Column_Block(TEXT("Block"));
static FName Column_Type(TEXT("Type"));
static FName Column_New(TEXT("New"));
}

struct FParameterPickerEntry
{
	enum class EFilterResult : uint8
	{
		DoesNotPassFilter	= 0x00,
		PassesFilter		= 0x01,
		ChildPassesFilter	= 0x02,
	};

	FRIEND_ENUM_CLASS_FLAGS(EFilterResult);

	FParameterPickerEntry() = default;

	FParameterPickerEntry(const FParameterBindingReference& InBinding)
		: Binding(InBinding)
	{
		PinType = UE::AnimNext::UncookedOnly::FUtils::GetPinTypeFromParamType(InBinding.Type);
		PinIcon = FBlueprintEditorUtils::GetIconFromPin(PinType, /* bIsLarge = */true);
		PinColor = GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);
		FString ParameterString = Binding.Parameter.ToString();
		int32 LastUnderscoreLoc = INDEX_NONE;
		if (ParameterString.FindLastChar(TEXT('_'), LastUnderscoreLoc))
		{
			ParameterString.RightChopInline(LastUnderscoreLoc + 1);
		}
		DisplayString = MoveTemp(ParameterString);
	}

	FParameterPickerEntry(FName InName)
	{
		Binding.Parameter = InName;
		FString ParameterString = Binding.Parameter.ToString();
		int32 LastUnderscoreLoc = INDEX_NONE;
		if (ParameterString.FindLastChar(TEXT('_'), LastUnderscoreLoc))
		{
			ParameterString.RightChopInline(LastUnderscoreLoc + 1);
		}
		DisplayString = MoveTemp(ParameterString);
	}

	bool PassesFilter(const FString& InFilterText) const
	{
		return Binding.Parameter.ToString().Contains(InFilterText);
	}

	FString DisplayString;

	FParameterBindingReference Binding;

	FEdGraphPinType PinType;

	const FSlateBrush* PinIcon = nullptr;

	FLinearColor PinColor = FLinearColor::White;

	TSharedPtr<FParameterPickerEntry> Parent;

	TArray<TSharedRef<FParameterPickerEntry>> Children;

	TArray<TSharedRef<FParameterPickerEntry>> FilteredChildren;

	EFilterResult FilterResult = EFilterResult::PassesFilter;
};

ENUM_CLASS_FLAGS(FParameterPickerEntry::EFilterResult);

void SParameterPicker::Construct(const FArguments& InArgs)
{
	using namespace ParameterPicker;

	Args = InArgs._Args;

	if(Args.OnGetParameterBindings != nullptr)
	{
		Args.OnGetParameterBindings->BindSP(this, &SParameterPicker::HandleGetParameterBindings);
	}

	if(Args.bFocusSearchWidget)
	{
		RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime)
		{
			if (SearchBox.IsValid())
			{
				FWidgetPath WidgetToFocusPath;
				FSlateApplication::Get().GeneratePathToWidgetUnchecked(SearchBox.ToSharedRef(), WidgetToFocusPath);
				FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPath, EFocusCause::SetDirectly);
				WidgetToFocusPath.GetWindow()->SetWidgetToFocusOnActivate(SearchBox);
				return EActiveTimerReturnType::Stop;
			}

			return EActiveTimerReturnType::Continue;
		}));
	}

	TSharedPtr<SHeaderRow> HeaderRow;
	TSharedRef<SWidget> AddNewParameterWidget = SNullWidget::NullWidget;

	if(Args.bAllowNew)
	{
		SAssignNew(AddNewParameterWidget, SSimpleButton)
		.Text(LOCTEXT("AddNewParameterButton", "New Parameter"))
		.ToolTipText(LOCTEXT("AddColumnHeaderTooltip", "Add a new parameter at global scope"))
		.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
		.OnClicked_Lambda([this]()
		{
			FSlateApplication::Get().DismissAllMenus();
			TSharedRef<SAddParametersDialog> AddParametersDialog =
				SNew(SAddParametersDialog)
				.AllowMultiple(false)
				.OnFilterParameterType(Args.OnFilterParameterType)
				.InitialParamType(Args.NewParameterType);
			TArray<FParameterToAdd> ParametersToAdd;
			if(AddParametersDialog->ShowModal(ParametersToAdd))
			{
				if(ParametersToAdd.Num() > 0)
				{
					FScopedTransaction Transaction(LOCTEXT("AddParameter", "Add parameter"));
					for (const FParameterToAdd& ParameterToAdd : ParametersToAdd)
					{
						FParameterBindingReference Reference;
						Reference.Parameter = ParametersToAdd[0].Name;
						Args.OnAddParameter.ExecuteIfBound(ParametersToAdd[0]);
						Args.OnParameterPicked.ExecuteIfBound(Reference);
					}
				}
			}
			return FReply::Handled();
		});
	}


	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.MaxWidth(250.0f)
			[
				SAssignNew(SearchBox, SSearchBox)
				.OnTextChanged_Lambda([this](FText InText)
				{
					FilterText = InText;
					RefreshFilter();
				})
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoWidth()
			[
				AddNewParameterWidget
			]
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2.0f)
		[
			SAssignNew(EntriesList, STreeView<TSharedRef<FParameterPickerEntry>>)
			.TreeItemsSource(&FilteredHierarchy)
			.SelectionMode(Args.bMultiSelect ? ESelectionMode::Multi : ESelectionMode::Single)
			.OnGenerateRow(this, &SParameterPicker::HandleGenerateRow)
			.OnSelectionChanged(this, &SParameterPicker::HandleSelectionChanged)
			.OnGetChildren(this, &SParameterPicker::HandleGetChildren)
			.OnIsSelectableOrNavigable(this, &SParameterPicker::HandleIsSelectableOrNavigable)
			.ItemHeight(20.0f)
			.HeaderRow(
				SAssignNew(HeaderRow, SHeaderRow)
				+SHeaderRow::Column(Column_Type)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(24.0f)
				.HeaderContent()
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::GetBrush("Kismet.VariableList.TypeIcon"))
						.ToolTipText(LOCTEXT("TypeColumnHeaderTooltip", "The parameter's type"))
					]
				]

				+SHeaderRow::Column(Column_Parameter)
				.DefaultLabel(LOCTEXT("ParameterColumnHeader", "Parameter"))
				.ToolTipText(LOCTEXT("ParameterColumnHeaderTooltip", "The parameter's name"))
				.FillWidth(0.33f)
			)
		]
	];

	
	if (Args.bShowBlocks)
	{
		HeaderRow->AddColumn(
			SHeaderRow::Column(Column_Block)
			.DefaultLabel(LOCTEXT("BlockColumnHeader", "Block"))
			.ToolTipText(LOCTEXT("BlockColumnHeaderTooltip", "The parameter block that has a binding to the parameter"))
			.FillWidth(0.33f));
	}

	if(Args.bAllowNew)
	{
		HeaderRow->AddColumn(
			SHeaderRow::Column(Column_New)
			.DefaultLabel(FText::GetEmpty())
			.HeaderContentPadding(FMargin(0.0f))
			.FixedWidth(24.0f)
			.HeaderContent()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("AddColumnHeaderTooltip", "Add a new parameter at global scope"))
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked_Lambda([this]()
				{
					FSlateApplication::Get().DismissAllMenus();
					TSharedRef<SAddParametersDialog> AddParametersDialog =
						SNew(SAddParametersDialog)
						.AllowMultiple(false);
					TArray<FParameterToAdd> ParametersToAdd;
					if(AddParametersDialog->ShowModal(ParametersToAdd))
					{
						if(ParametersToAdd.Num() > 0)
						{
							FScopedTransaction Transaction(LOCTEXT("AddParameter", "Add parameter"));

							FParameterBindingReference Reference(ParametersToAdd[0].Name, ParametersToAdd[0].Type);
							Args.OnParameterPicked.ExecuteIfBound(Reference);
						}
					}
					return FReply::Handled();
				})
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::GetBrush("Icons.Plus"))
					]
				]
			]);
	}
	
	RefreshEntries();
}

void SParameterPicker::RefreshEntries()
{
	Entries.Empty();

	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter ARFilter;

	TSet<TTuple<FName, FAssetData>> BoundParameters;

	if(Args.bAllowNone)
	{
		Entries.Add(MakeShared<FParameterPickerEntry>(FParameterBindingReference(NAME_None, FAnimNextParamType())));
	}
	
	// Find all blocks and their bound parameters
	if(Args.bShowBoundParameters)
	{
		ARFilter.ClassPaths = { UAnimNextParameterBlock::StaticClass()->GetClassPathName() };
		
		TArray<FAssetData> BlockAssets;
		AssetRegistry.GetAssets(ARFilter, BlockAssets);

		for(const FAssetData& BlockAsset : BlockAssets)
		{
			FAnimNextParameterProviderAssetRegistryExports Exports;
			if(UncookedOnly::FUtils::GetExportedParametersForAsset(BlockAsset, Exports))
			{
				for(const FAnimNextParameterAssetRegistryExportEntry& Export : Exports.Parameters)
				{
					BoundParameters.Add({ Export.Name, BlockAsset });
					FParameterBindingReference NewReference(Export.Name, Export.Type, BlockAsset);
					if(!Args.OnFilterParameter.IsBound() || Args.OnFilterParameter.Execute(NewReference) == EFilterParameterResult::Include)
					{
						FAnimNextParamType ParamType = UE::AnimNext::UncookedOnly::FUtils::GetParameterTypeFromName(Export.Name);
						if(!Args.OnFilterParameterType.IsBound() || Args.OnFilterParameterType.Execute(ParamType) == EFilterParameterResult::Include)
						{
							if (Args.bShowBoundParameters && EnumHasAnyFlags(Export.Flags, EAnimNextParameterFlags::Bound))
							{
								TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(NewReference);
								Entries.Add(NewEntry);
							}
						}
					}
				}
			};
		}
	}

	// Find all parameters (that have not already been added as bound above)
	if(Args.bShowUnboundParameters)
	{
		FAnimNextParameterProviderAssetRegistryExports AllExports;
		if(UE::AnimNext::UncookedOnly::FUtils::GetExportedParametersFromAssetRegistry(AllExports))
		{
			for(const FAnimNextParameterAssetRegistryExportEntry& ExportEntry : AllExports.Parameters)
			{
				if(!BoundParameters.Contains( { ExportEntry.Name, ExportEntry.ReferencingAsset } ))
				{
					FParameterBindingReference NewReference(ExportEntry.Name, ExportEntry.Type, ExportEntry.ReferencingAsset);

					if(!Args.OnFilterParameter.IsBound() || Args.OnFilterParameter.Execute(NewReference) == EFilterParameterResult::Include)
					{
						if (!Args.OnFilterParameterType.IsBound() || Args.OnFilterParameterType.Execute(ExportEntry.Type) == EFilterParameterResult::Include)
						{
							TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(NewReference);
							Entries.Add(NewEntry);
						}
					}
				}
			}
		}
	}

	if (Args.bShowBuiltInParameters)
	{
		FExternalParameterRegistry::ForEachParameter([this](FName InParameterName, const IParameterSourceFactory::FParameterInfo& InInfo)
		{
			FParameterBindingReference NewReference(InParameterName, InInfo.Type);
			if (!Args.OnFilterParameter.IsBound() || Args.OnFilterParameter.Execute(NewReference) == EFilterParameterResult::Include)
			{
				if (!Args.OnFilterParameterType.IsBound() || Args.OnFilterParameterType.Execute(InInfo.Type) == EFilterParameterResult::Include)
				{
					Entries.Add(MakeShared<FParameterPickerEntry>(NewReference));
				}
			}
		});
	}

	BuildHierarchy();

	RefreshFilter();
}

void SParameterPicker::BuildHierarchy()
{
	Hierarchy.Reset();

	TMap<FName, TSharedRef<FParameterPickerEntry>> NameMap;
	NameMap.Reserve(Entries.Num());

	for (const TSharedRef<FParameterPickerEntry>& Entry : Entries)
	{
		NameMap.Add(Entry->Binding.Parameter, Entry);
	}

	TArray< TSharedRef<FParameterPickerEntry>> NewEntries;
	for (const TSharedRef<FParameterPickerEntry>& Entry : Entries)
	{
		// Parse name into separators
		TStringBuilder<256> WholeParameterString;
		Entry->Binding.Parameter.ToString(WholeParameterString);

		TStringBuilder<256> PartialParameterString;

		// We use '_' as a separator here as:
		// - Each param is a UObject in editor and uses its object name, so we cannot use '.'
		// - This maps nicely to Verse tags that use '_' to hierarchically define their relationship
		TSharedPtr<FParameterPickerEntry> Parent = nullptr;
		UE::String::ParseTokens(WholeParameterString, TEXT('_'), [this, &PartialParameterString, &NameMap, &Parent, &NewEntries](FStringView InStringView)
		{
			if(Parent.IsValid())
			{
				// Have a parent, so add child if it doesnt exist already
				PartialParameterString += TEXT('_');
				PartialParameterString += InStringView;

				const FName ParameterName(InStringView);
				const FName PartialParameterName(PartialParameterString);
				if (TSharedRef<FParameterPickerEntry>* ExistingEntry = NameMap.Find(PartialParameterName))
				{
					(*ExistingEntry)->Parent = Parent;
					if (!Parent->Children.ContainsByPredicate([&ExistingEntry](const TSharedRef<FParameterPickerEntry>& InEntry) { return (*ExistingEntry)->Binding.Parameter == InEntry->Binding.Parameter; }))
					{
						Parent->Children.Add(*ExistingEntry);
					}
					Parent = *ExistingEntry;
				}
				else
				{
					TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(PartialParameterName);
					NewEntry->Parent = Parent;
					if (!Parent->Children.ContainsByPredicate([&NewEntry](const TSharedRef<FParameterPickerEntry>& InEntry) { return NewEntry->Binding.Parameter == InEntry->Binding.Parameter; }))
					{
						Parent->Children.Add(NewEntry);
					}
					Parent = NameMap.Add(PartialParameterName, NewEntry);
					NewEntries.Add(NewEntry);
				}
			}
			else
			{
				PartialParameterString += InStringView;

				// Add root item if it doesnt exist already
				const FName ParameterName(InStringView);
				if (TSharedRef<FParameterPickerEntry>* ExistingParent = NameMap.Find(ParameterName))
				{
					Parent = *ExistingParent;
				}
				else
				{
					TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(ParameterName);
					Parent = NameMap.Add(ParameterName, NewEntry);
					NewEntries.Add(NewEntry);
				}

				if (!Hierarchy.ContainsByPredicate([&Parent](const TSharedRef<FParameterPickerEntry>& InEntry){ return Parent->Binding.Parameter == InEntry->Binding.Parameter; }))
				{
					Hierarchy.Add(Parent.ToSharedRef());
				}
			}
		}, UE::String::EParseTokensOptions::SkipEmpty);
	}

	Entries.Append(NewEntries);
}

void SParameterPicker::RefreshFilter()
{
	FilteredEntries.Reset();
	FilteredHierarchy.Reset();
	const FString FilterTextAsString = FilterText.ToString();

	for (const TSharedRef<FParameterPickerEntry>& Entry : Entries)
	{
		Entry->FilterResult = FParameterPickerEntry::EFilterResult::DoesNotPassFilter;
	}

	for(const TSharedRef<FParameterPickerEntry>& Entry : Entries)
	{
		Entry->FilteredChildren.Reset();
		if(Entry->PassesFilter(FilterTextAsString))
		{
			Entry->FilterResult |= FParameterPickerEntry::EFilterResult::PassesFilter;
			FilteredEntries.Add(Entry);

			if(FilterTextAsString.Len() > 0)
			{
				TSharedPtr< FParameterPickerEntry> ParentEntry = Entry->Parent;
				while (ParentEntry.IsValid())
				{
					ParentEntry->FilterResult |= FParameterPickerEntry::EFilterResult::ChildPassesFilter;
					ParentEntry = ParentEntry->Parent;
				}
			}
		}
	}

	TArray<TSharedRef<FParameterPickerEntry>> Stack;
	Stack.Reserve(16);
	for (const TSharedRef<FParameterPickerEntry>& Entry : Hierarchy)
	{
		Stack.Add(Entry);
	}

	while (Stack.Num() > 0)
	{
		TSharedRef<FParameterPickerEntry> Top = Stack.Top();
		Stack.Pop(EAllowShrinking::No);

		if (Top->FilterResult != FParameterPickerEntry::EFilterResult::DoesNotPassFilter)
		{
			if (!Top->Parent.IsValid())
			{
				FilteredHierarchy.Add(Top);
			}
			else
			{
				Top->Parent->FilteredChildren.Add(Top);
			}

			for (const TSharedRef<FParameterPickerEntry>& ChildEntry : Top->Children)
			{
				Stack.Add(ChildEntry);
			}

			if (EnumHasAnyFlags(Top->FilterResult, FParameterPickerEntry::EFilterResult::ChildPassesFilter))
			{
				EntriesList->SetItemExpansion(Top, true);
			}
		}
	}

	EntriesList->RequestTreeRefresh();
}

class SParameterPickerRow : public SMultiColumnTableRow<TSharedRef<FParameterPickerEntry>>
{
	SLATE_BEGIN_ARGS(SParameterPickerRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<FParameterPickerEntry> InEntry, TSharedRef<const SParameterPicker> InParameterPicker)
	{
		Entry = InEntry;
		ParameterPicker = InParameterPicker;
		SMultiColumnTableRow<TSharedRef<FParameterPickerEntry>>::Construct( SMultiColumnTableRow<TSharedRef<FParameterPickerEntry>>::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		using namespace ParameterPicker;

		if (InColumnName == Column_Type)
		{
			return
				SNew(SHorizontalBox)
				.Visibility(Entry->Binding.Type.IsValid() ? EVisibility::Visible : EVisibility::Hidden)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(Entry->PinIcon)
					.ColorAndOpacity(Entry->PinColor)
					.ToolTipText_Lambda([this]() -> FText
					{
						return FText::FromString(Entry->Binding.Type.ToString());
					})
				];
		}
		else if(InColumnName == Column_Parameter)
		{
			return
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this))
					.IndentAmount(8)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(FText::FromString(Entry->DisplayString))
					.ToolTipText(FText::Format(LOCTEXT("ParameterPathTooltip", "{1}::{0}"), UncookedOnly::FUtils::GetParameterDisplayNameText(Entry->Binding.Parameter), FText::FromName(Entry->Binding.Asset.AssetName)))
					.HighlightText_Lambda([this]()
					{
						return ParameterPicker.Pin()->FilterText;
					})
				];
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
	
	TWeakPtr<const SParameterPicker> ParameterPicker;
	TSharedPtr<FParameterPickerEntry> Entry;
};

TSharedRef<ITableRow> SParameterPicker::HandleGenerateRow(TSharedRef<FParameterPickerEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable) const
{
	return SNew(SParameterPickerRow, InOwnerTable, InEntry, SharedThis(this));
}

void SParameterPicker::HandleGetChildren(TSharedRef<FParameterPickerEntry> InEntry, TArray<TSharedRef<FParameterPickerEntry>>& OutChildren) const
{
	OutChildren = InEntry->FilteredChildren;
}

void SParameterPicker::HandleSelectionChanged(TSharedPtr<FParameterPickerEntry> InEntry, ESelectInfo::Type InSelectInfo)
{
	Args.OnSelectionChanged.ExecuteIfBound();

	if(!Args.bMultiSelect && EntriesList->GetNumItemsSelected() == 1 && Args.OnParameterPicked.IsBound())
	{
		TArray<TSharedRef<FParameterPickerEntry>> SelectedEntries;
		EntriesList->GetSelectedItems(SelectedEntries);

		if(SelectedEntries[0]->Binding.Type.IsValid() || Args.bAllowNone)
		{
			Args.OnParameterPicked.ExecuteIfBound(SelectedEntries[0]->Binding);
		}
	}
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

bool SParameterPicker::HandleIsSelectableOrNavigable(TSharedRef<FParameterPickerEntry> InEntry) const
{
	// Only allow selecting items that have valid parameters
	return InEntry->Binding.Type.IsValid();
}

}

#undef LOCTEXT_NAMESPACE