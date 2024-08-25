// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMViewBindingListView.h"

#include "Blueprint/WidgetTree.h"
#include "BlueprintEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/TableViewMetadata.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"

#include "Framework/MVVMRowHelper.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMBlueprintViewEvent.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "Types/MVVMBindingEntry.h"

#include "Widgets/BindingEntry/SMVVMBindingRow.h"
#include "Widgets/BindingEntry/SMVVMEventRow.h"
#include "Widgets/BindingEntry/SMVVMFunctionParameterRow.h"
#include "Widgets/BindingEntry/SMVVMGroupRow.h"
#include "Widgets/SMVVMViewBindingPanel.h"
#include "Widgets/SMVVMViewModelPanel.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "BindingListView"

namespace UE::MVVM
{

namespace Private
{
	void ExpandAll(const TSharedPtr<STreeView<TSharedPtr<FBindingEntry>>>& TreeView, const TSharedPtr<FBindingEntry>& Entry)
	{
		TreeView->SetItemExpansion(Entry, true);

		for (const TSharedPtr<FBindingEntry>& Child : Entry->GetFilteredChildren())
		{
			ExpandAll(TreeView, Child);
		}
	}

	TSharedPtr<FBindingEntry> FindBinding(FGuid BindingId, TConstArrayView<TSharedPtr<FBindingEntry>> Entries)
	{
		for (const TSharedPtr<FBindingEntry>& Entry : Entries)
		{
			if (Entry->GetRowType() == FBindingEntry::ERowType::Binding && Entry->GetBindingId() == BindingId)
			{
				return Entry;
			}
			TSharedPtr<FBindingEntry> Result = FindBinding(BindingId, Entry->GetAllChildren());
			if (Result)
			{
				return Result;
			}
		}
		return TSharedPtr<FBindingEntry>();
	}
	
	TSharedPtr<FBindingEntry> FindEvent(UMVVMBlueprintViewEvent* Event, TConstArrayView<TSharedPtr<FBindingEntry>> Entries)
	{
		for (const TSharedPtr<FBindingEntry>& Entry : Entries)
		{
			if (Entry->GetRowType() == FBindingEntry::ERowType::Event && Entry->GetEvent() == Event)
			{
				return Entry;
			}
			TSharedPtr<FBindingEntry> Result = FindEvent(Event, Entry->GetAllChildren());
			if (Result)
			{
				return Result;
			}
		}
		return TSharedPtr<FBindingEntry>();
	}

	void FilterEntryList(FString FilterString, const TArray<TSharedPtr<FBindingEntry>>& RootGroups, TArray<TSharedPtr<FBindingEntry>>& FilteredRootGroups, UMVVMBlueprintView* BlueprintView, UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr)
	{
		if (!FilterString.TrimStartAndEnd().IsEmpty())
		{
			TArray<FString> SearchKeywords;
			FilterString.ParseIntoArray(SearchKeywords, TEXT(" "));

			struct FIsAllKeywordsInString
			{
				bool operator()(FString EntryString, TArray<FString>& SearchKeywords)
				{
					for (const FString& Keyword : SearchKeywords)
					{
						if (!EntryString.Contains(Keyword))
						{
							return false;
						}
					}
					return true;
				}
			} IsAllKeywordsInString;

			struct FAddFilteredEntry
			{
				FAddFilteredEntry(FIsAllKeywordsInString& InIsAllKeywordsInString, TArray<FString>& InSearchKeywords, UMVVMBlueprintView* InBlueprintView, UWidgetBlueprint* InWidgetBlueprint)
					: IsAllKeywordsInString(InIsAllKeywordsInString)
					, SearchKeywords(InSearchKeywords)
					, WidgetBlueprint(InWidgetBlueprint)
					, BlueprintView(InBlueprintView)
				{}
				void operator()(TSharedPtr<FBindingEntry> ParentEntry)
				{
					for (TSharedPtr<FBindingEntry> Entry : ParentEntry->GetAllChildren())
					{
						FString EntryString = Entry->GetSearchNameString(BlueprintView, WidgetBlueprint);
						if (IsAllKeywordsInString(EntryString, SearchKeywords))
						{
							// If the filter text is found in the group name, we keep the entire group.
							ParentEntry->AddFilteredChild(Entry);
						}
						else
						{
							(*this)(Entry);
						}
					}
					ParentEntry->SetUseFilteredChildList();
				}
				FIsAllKeywordsInString& IsAllKeywordsInString;
				TArray<FString>& SearchKeywords;
				UWidgetBlueprint* WidgetBlueprint = nullptr;
				UMVVMBlueprintView* BlueprintView = nullptr;
			};
			FAddFilteredEntry AddFilteredEntry = FAddFilteredEntry(IsAllKeywordsInString, SearchKeywords, BlueprintView, MVVMExtensionPtr->GetWidgetBlueprint());

			for (const TSharedPtr<FBindingEntry>& GroupEntry : RootGroups)
			{
				FString EntryString = GroupEntry->GetSearchNameString(BlueprintView, MVVMExtensionPtr->GetWidgetBlueprint());

				// If the filter text is found in the group name, we keep the entire group.
				if (IsAllKeywordsInString(EntryString, SearchKeywords))
				{
					FilteredRootGroups.Add(GroupEntry);
				}
				else
				{
					AddFilteredEntry(GroupEntry);
					if (GroupEntry->GetFilteredChildren().Num() > 0)
					{
						FilteredRootGroups.Add(GroupEntry);
					}
				}
			}
		}
		else
		{
			FilteredRootGroups = RootGroups;
		}
	}
} // namespace


void SBindingsList::Construct(const FArguments& InArgs, TSharedPtr<SBindingsPanel> Owner, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor, UMVVMWidgetBlueprintExtension_View* InMVVMExtension)
{
	BindingPanel = Owner;
	MVVMExtension = InMVVMExtension;
	WeakBlueprintEditor = InBlueprintEditor;
	check(InMVVMExtension);
	check(InMVVMExtension->GetBlueprintView());

	MVVMExtension->OnBlueprintViewChangedDelegate().AddSP(this, &SBindingsList::Refresh);
	MVVMExtension->GetBlueprintView()->OnBindingsUpdated.AddSP(this, &SBindingsList::Refresh);
	MVVMExtension->GetBlueprintView()->OnEventsUpdated.AddSP(this, &SBindingsList::Refresh);
	MVVMExtension->GetBlueprintView()->OnBindingsAdded.AddSP(this, &SBindingsList::ClearFilterText);
	MVVMExtension->GetBlueprintView()->OnViewModelsUpdated.AddSP(this, &SBindingsList::ForceRefresh);

	ChildSlot
	[
		SAssignNew(TreeView, STreeView<TSharedPtr<FBindingEntry>>)
		.TreeItemsSource(&FilteredRootGroups)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SBindingsList::GenerateEntryRow)
		.OnGetChildren(this, &SBindingsList::GetChildrenOfEntry)
		.OnContextMenuOpening(this, &SBindingsList::OnSourceConstructContextMenu)
		.OnSelectionChanged(this, &SBindingsList::OnSourceListSelectionChanged)
		.ItemHeight(32)
	];

	Refresh();
}

SBindingsList::~SBindingsList()
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		MVVMExtensionPtr->OnBlueprintViewChangedDelegate().RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnBindingsUpdated.RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnEventsUpdated.RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnBindingsAdded.RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnViewModelsUpdated.RemoveAll(this);
	}
}

void SBindingsList::GetChildrenOfEntry(TSharedPtr<FBindingEntry> Entry, TArray<TSharedPtr<FBindingEntry>>& OutChildren) const
{
	OutChildren.Append(Entry->GetFilteredChildren());
}

template<typename TEntryValueType>
void SBindingsList::RegisterWrapperGraphModified(TEntryValueType* EntryValue, TSharedPtr<FBindingEntry> BindingEntry)
{
	{
		const FObjectKey ObjectKey = FObjectKey(EntryValue);
		TPair<TWeakPtr<FBindingEntry>, FDelegateHandle>* FoundWrapperGraphModifiedPtr = WrapperGraphModifiedDelegates.Find(ObjectKey);
		if (FoundWrapperGraphModifiedPtr)
		{
			TSharedPtr<FBindingEntry> FoundWrapperGraphModified = FoundWrapperGraphModifiedPtr->Get<0>().Pin();
			if (FoundWrapperGraphModified != BindingEntry)
			{
				EntryValue->OnWrapperGraphModified.Remove(FoundWrapperGraphModifiedPtr->Get<1>());
				WrapperGraphModifiedDelegates.Remove(ObjectKey);
				FoundWrapperGraphModifiedPtr = nullptr;
			}
		}
		if (FoundWrapperGraphModifiedPtr == nullptr)
		{
			FDelegateHandle DelegateHandle = EntryValue->OnWrapperGraphModified.AddSP(this, &SBindingsList::HandleRefreshChildren, ObjectKey);
			WrapperGraphModifiedDelegates.Add(ObjectKey, { TWeakPtr<FBindingEntry>(BindingEntry), DelegateHandle });
		}
	}
}

void SBindingsList::Refresh()
{
	struct FPreviousGroup
	{
		TSharedPtr<FBindingEntry> Group;
		TArray<TSharedPtr<FBindingEntry>> Children;
	};

	TArray<FPreviousGroup> PreviousRootGroups;
	for (const TSharedPtr<FBindingEntry>& PreviousEntry : AllRootGroups)
	{
		ensure(PreviousEntry->GetRowType() == FBindingEntry::ERowType::Group);
		FPreviousGroup& NewItem = PreviousRootGroups.AddDefaulted_GetRef();
		NewItem.Group = PreviousEntry;

		struct FRecursiveAdd
		{
			void operator()(FPreviousGroup& NewItem, const TSharedPtr<FBindingEntry>& Entry)
			{
				for (TSharedPtr<FBindingEntry> PreviousChildEntry : Entry->GetAllChildren())
				{
					NewItem.Children.Add(PreviousChildEntry);
					(*this)(NewItem, PreviousChildEntry);
				}
				Entry->ResetChildren();
			}
		};
		FRecursiveAdd{}(NewItem, PreviousEntry);
	}

	AllRootGroups.Reset();
	FilteredRootGroups.Reset();

	TArray<TSharedPtr<FBindingEntry>> NewEntries;

	UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get();
	UMVVMBlueprintView* BlueprintView = MVVMExtensionPtr ? MVVMExtensionPtr->GetBlueprintView() : nullptr;
	UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr ? MVVMExtensionPtr->GetWidgetBlueprint() : nullptr;

	// generate our entries
	// for each widget with bindings, create an entry at the root level
	// then add all bindings that reference that widget as its children
	if (BlueprintView)
	{
		auto FindPreviousGroupEntry = [&PreviousRootGroups, Self = this](FName GroupName)
		{
			return PreviousRootGroups.FindByPredicate([GroupName](const FPreviousGroup& Other) { return Other.Group->GetGroupName() == GroupName; });
		};
		auto FindGroupEntry = [&NewEntries, Self = this](FPreviousGroup* PreviousGroupEntry, FName GroupName, FGuid ViewModelId)
		{
			TSharedPtr<FBindingEntry> GroupEntry;
			if (PreviousGroupEntry)
			{
				GroupEntry = PreviousGroupEntry->Group;
			}
			else if (TSharedPtr<FBindingEntry>* FoundGroup = NewEntries.FindByPredicate([GroupName](const TSharedPtr<FBindingEntry>& Other)
				{ return Other->GetGroupName() == GroupName && Other->GetRowType() == FBindingEntry::ERowType::Group; }))
			{
				GroupEntry = *FoundGroup;
			}

			if (!GroupEntry.IsValid())
			{
				GroupEntry = MakeShared<FBindingEntry>();
				GroupEntry->SetGroup(GroupName, ViewModelId);

				NewEntries.Add(GroupEntry);
			}
			Self->AllRootGroups.AddUnique(GroupEntry);
			return GroupEntry;
		};

		for (const FMVVMBlueprintViewBinding& Binding : BlueprintView->GetBindings())
		{
			// Make sure the graph for the bindings is generated
			if (Binding.Conversion.SourceToDestinationConversion)
			{
				Binding.Conversion.SourceToDestinationConversion->GetOrCreateWrapperGraph(WidgetBlueprint);
			}
			if (Binding.Conversion.DestinationToSourceConversion)
			{
				Binding.Conversion.DestinationToSourceConversion->GetOrCreateWrapperGraph(WidgetBlueprint);
			}
			
			FName GroupName;
			FGuid GroupViewModelId;
			switch (Binding.DestinationPath.GetSource(WidgetBlueprint))
			{
			case EMVVMBlueprintFieldPathSource::SelfContext:
				GroupName = WidgetBlueprint->GetFName();
				break;
			case EMVVMBlueprintFieldPathSource::Widget:
				GroupName = Binding.DestinationPath.GetWidgetName();
				break;
			case EMVVMBlueprintFieldPathSource::ViewModel:
				if (const FMVVMBlueprintViewModelContext* ViewModelContext = BlueprintView->FindViewModel(Binding.DestinationPath.GetViewModelId()))
				{
					GroupName = ViewModelContext->GetViewModelName();
					GroupViewModelId = ViewModelContext->GetViewModelId();
				}
				break;
			}

			// Find the group entry
			FPreviousGroup* PreviousGroupEntry = FindPreviousGroupEntry(GroupName);
			TSharedPtr<FBindingEntry> GroupEntry = FindGroupEntry(PreviousGroupEntry, GroupName, GroupViewModelId);

			// Create/Find the child entry
			TSharedPtr<FBindingEntry> BindingEntry;
			FGuid BindingId = Binding.BindingId;
			{
				if (PreviousGroupEntry)
				{
					if (TSharedPtr<FBindingEntry>* FoundBinding = PreviousGroupEntry->Children.FindByPredicate([BindingId](const TSharedPtr<FBindingEntry>& Other)
						{ return Other->GetBindingId() == BindingId && Other->GetRowType() == FBindingEntry::ERowType::Binding; }))
					{
						BindingEntry = *FoundBinding;
					}
				}

				if (!BindingEntry.IsValid())
				{
					BindingEntry = MakeShared<FBindingEntry>();
					BindingEntry->SetBindingId(BindingId);

					NewEntries.Add(BindingEntry);
				}
				GroupEntry->AddChild(BindingEntry);
			}

			// Create/Find entries for conversion function parameters
			if (UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(UE::MVVM::IsForwardBinding(Binding.BindingType)))
			{
				// Register to any modifications made in the graph
				RegisterWrapperGraphModified(ConversionFunction, BindingEntry);

				// Make sure the graph is up to date
				ConversionFunction->GetOrCreateWrapperGraph(MVVMExtensionPtr->GetWidgetBlueprint());

				for (const FMVVMBlueprintPin& Pin : ConversionFunction->GetPins())
				{
					UEdGraphPin* GraphPin = ConversionFunction->GetOrCreateGraphPin(MVVMExtensionPtr->GetWidgetBlueprint(), Pin.GetId());
					if (GraphPin && GraphPin->bHidden)
					{
						continue;
					}

					TSharedPtr<FBindingEntry> ArgumentEntry;
					if (PreviousGroupEntry)
					{
						TSharedPtr<FBindingEntry>* FoundParameter = PreviousGroupEntry->Children.FindByPredicate(
							[BindingId, ArgumentId = Pin.GetId()](const TSharedPtr<FBindingEntry>& Other)
							{
								return Other->GetBindingId() == BindingId && Other->GetRowType() == FBindingEntry::ERowType::BindingParameter && Other->GetBindingParameterId() == ArgumentId;
							});
						if (FoundParameter)
						{
							ArgumentEntry = *FoundParameter;
						}
					}

					if (!ArgumentEntry.IsValid())
					{
						ArgumentEntry = MakeShared<FBindingEntry>();
						ArgumentEntry->SetBindingParameter(Binding.BindingId, Pin.GetId());

						NewEntries.Add(ArgumentEntry);
					}
					BindingEntry->AddChild(ArgumentEntry);
				}
			}
		}

		for (UMVVMBlueprintViewEvent* Event : BlueprintView->GetEvents())
		{
			// Make sure the graph is up to date
			Event->GetOrCreateWrapperGraph();

			FName GroupName;
			FGuid GroupViewModelId;
			switch (Event->GetEventPath().GetSource(WidgetBlueprint))
			{
			case EMVVMBlueprintFieldPathSource::SelfContext:
				GroupName = WidgetBlueprint->GetFName();
				break;
			case EMVVMBlueprintFieldPathSource::Widget:
				GroupName = Event->GetEventPath().GetWidgetName();
				break;
			case EMVVMBlueprintFieldPathSource::ViewModel:
				if (const FMVVMBlueprintViewModelContext* ViewModelContext = BlueprintView->FindViewModel(Event->GetEventPath().GetViewModelId()))
				{
					GroupName = ViewModelContext->GetViewModelName();
					GroupViewModelId = ViewModelContext->GetViewModelId();
				}
				break;
			}

			// Find the group entry
			FPreviousGroup* PreviousGroupEntry = FindPreviousGroupEntry(GroupName);
			TSharedPtr<FBindingEntry> GroupEntry = FindGroupEntry(PreviousGroupEntry, GroupName, GroupViewModelId);

			// Create/Find the child entry
			TSharedPtr<FBindingEntry> EventEntry;
			{
				if (PreviousGroupEntry)
				{
					if (TSharedPtr<FBindingEntry>* FoundBinding = PreviousGroupEntry->Children.FindByPredicate([Event](const TSharedPtr<FBindingEntry>& Other)
						{
							return Other->GetRowType() == FBindingEntry::ERowType::Event && Other->GetEvent() == Event;
						}))
					{
						EventEntry = *FoundBinding;
					}
				}

				if (!EventEntry.IsValid())
				{
					EventEntry = MakeShared<FBindingEntry>();
					EventEntry->SetEvent(Event);

					NewEntries.Add(EventEntry);
				}
				GroupEntry->AddChild(EventEntry);
			}

			// Register to any modifications made by the graph
			RegisterWrapperGraphModified(Event, EventEntry);

			// Create/Find entries for function parameters
			for (const FMVVMBlueprintPin& Pin : Event->GetPins())
			{
				UEdGraphPin* GraphPin = Event->GetOrCreateGraphPin(Pin.GetId());
				if (GraphPin && GraphPin->bHidden)
				{
					continue;
				}

				TSharedPtr<FBindingEntry> ArgumentEntry;
				if (PreviousGroupEntry)
				{
					TSharedPtr<FBindingEntry>* FoundParameter = PreviousGroupEntry->Children.FindByPredicate(
						[Event, ArgumentId = Pin.GetId()](const TSharedPtr<FBindingEntry>& Other)
						{
							return Other->GetRowType() == FBindingEntry::ERowType::EventParameter && Other->GetEvent() == Event && Other->GetEventParameterId() == ArgumentId;
						});
					if (FoundParameter)
					{
						ArgumentEntry = *FoundParameter;
					}
				}

				if (!ArgumentEntry.IsValid())
				{
					ArgumentEntry = MakeShared<FBindingEntry>();
					ArgumentEntry->SetEventParameter(Event, Pin.GetId());

					NewEntries.Add(ArgumentEntry);
				}
				EventEntry->AddChild(ArgumentEntry);
			}
		}

		Private::FilterEntryList(FilterText.ToString(), AllRootGroups, FilteredRootGroups, BlueprintView, MVVMExtensionPtr);
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
		for (const TSharedPtr<FBindingEntry>& Entry : NewEntries)
		{
			Private::ExpandAll(TreeView, Entry);
		}
	}
}

void SBindingsList::ForceRefresh()
{
	AllRootGroups.Reset();
	FilteredRootGroups.Reset();
	Refresh();
}

void SBindingsList::HandleRefreshChildren(FObjectKey ObjectHolder)
{
	TPair<TWeakPtr<FBindingEntry>, FDelegateHandle>* Found = WrapperGraphModifiedDelegates.Find(ObjectHolder);
	if (Found)
	{
		TSharedPtr<FBindingEntry> FoundWrapperGraphModified = Found->Get<0>().Pin();
		if (FoundWrapperGraphModified)
		{
			FoundWrapperGraphModified->ResetChildren();
			Refresh();
		}
	}
}

TSharedRef<ITableRow> SBindingsList::GenerateEntryRow(TSharedPtr<FBindingEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedPtr<ITableRow> Row;

	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		switch (Entry->GetRowType())
		{
			case FBindingEntry::ERowType::Group:
			{
				Row = SNew(UE::MVVM::BindingEntry::SGroupRow, OwnerTable, WeakBlueprintEditor.Pin(), MVVMExtensionPtr->GetWidgetBlueprint(), Entry);
				break;
			}
			case FBindingEntry::ERowType::Binding:
			{
				Row = SNew(UE::MVVM::BindingEntry::SBindingRow, OwnerTable, WeakBlueprintEditor.Pin(), MVVMExtensionPtr->GetWidgetBlueprint(), Entry);
				break;
			}
			case FBindingEntry::ERowType::BindingParameter:
			case FBindingEntry::ERowType::EventParameter:
			{
				Row = SNew(UE::MVVM::BindingEntry::SFunctionParameterRow, OwnerTable, WeakBlueprintEditor.Pin(), MVVMExtensionPtr->GetWidgetBlueprint(), Entry);
				break;
			}
			case FBindingEntry::ERowType::Event:
			{
				Row = SNew(UE::MVVM::BindingEntry::SEventRow, OwnerTable, WeakBlueprintEditor.Pin(), MVVMExtensionPtr->GetWidgetBlueprint(), Entry);
				break;
			}
		}

		return Row.ToSharedRef();
	}

	ensureMsgf(false, TEXT("Failed to create binding or widget row."));
	return SNew(STableRow<TSharedPtr<FBindingEntry>>, OwnerTable);
}

void SBindingsList::OnFilterTextChanged(const FText& InFilterText)
{
	FilterText = InFilterText;
	Refresh();
}

void SBindingsList::ClearFilterText()
{
	FilterText = FText::GetEmpty();
}

TSharedPtr<SWidget> SBindingsList::OnSourceConstructContextMenu()
{
	UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get();
	UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr ? MVVMExtensionPtr->GetWidgetBlueprint() : nullptr;
	UMVVMBlueprintView* View = MVVMExtensionPtr ? MVVMExtensionPtr->GetBlueprintView() : nullptr;

	return BindingEntry::FRowHelper::CreateContextMenu(WidgetBlueprint, View, TreeView->GetSelectedItems()).MakeWidget();
}

void SBindingsList::RequestNavigateToBinding(FGuid BindingId)
{
	TSharedPtr<FBindingEntry> Entry = Private::FindBinding(BindingId, FilteredRootGroups);
	if (Entry && TreeView)
	{
		TreeView->RequestNavigateToItem(Entry);
	}
}

void SBindingsList::RequestNavigateToEvent(UMVVMBlueprintViewEvent* Event)
{
	TSharedPtr<FBindingEntry> Entry = Private::FindEvent(Event, FilteredRootGroups);
	if (Entry && TreeView)
	{
		TreeView->RequestNavigateToItem(Entry);
	}
}

FReply SBindingsList::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
		{
			UMVVMBlueprintView* BlueprintView = MVVMExtensionPtr->GetBlueprintView();
			const UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr->GetWidgetBlueprint();
			TArray<TSharedPtr<FBindingEntry>> Selection = TreeView->GetSelectedItems();

			BindingEntry::FRowHelper::DeleteEntries(WidgetBlueprint, BlueprintView, Selection);
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SBindingsList::OnSourceListSelectionChanged(TSharedPtr<FBindingEntry> Entry, ESelectInfo::Type SelectionType) const
{
	if (bSelectionChangedGuard)
	{
		return;
	}
	TGuardValue<bool> ReentrantGuard(bSelectionChangedGuard, true);

	if (TSharedPtr<SBindingsPanel> BindingPanelPtr = BindingPanel.Pin())
	{
		if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
		{
			if (UMVVMBlueprintView* View = MVVMExtensionPtr->GetBlueprintView())
			{
				TArray<TSharedPtr<FBindingEntry>> SelectedEntries = TreeView->GetSelectedItems();
				TArray<FMVVMBlueprintViewBinding*> SelectedBindings;

				for (const TSharedPtr<FBindingEntry>& SelectedEntry : SelectedEntries)
				{
					if (FMVVMBlueprintViewBinding* SelectedBinding = Entry->GetBinding(View))
					{
						SelectedBindings.Add(SelectedBinding);
					}
				}

				BindingPanelPtr->OnBindingListSelectionChanged(SelectedBindings);
			}
		}
	}
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
