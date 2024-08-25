// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/MVVMRowHelper.h"

#include "Blueprint/WidgetTree.h"
#include "BlueprintEditor.h"
#include "Details/WidgetPropertyDragDropOp.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Editor/EditorEngine.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "ScopedTransaction.h"
#include "Widgets/ViewModelFieldDragDropOp.h"

#include "Styling/AppStyle.h"

#include "MVVMEditorSubsystem.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMBlueprintViewEvent.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "Misc/MessageDialog.h"
#include "Types/MVVMBindingEntry.h"
#include "Types/MVVMBindingMode.h"

extern UNREALED_API UEditorEngine* GEditor;

#define LOCTEXT_NAMESPACE "BindingListView_Helper"

namespace UE::MVVM::BindingEntry
{

void FRowHelper::GatherAllChildBindings(UMVVMBlueprintView* BlueprintView, const TConstArrayView<TSharedPtr<FBindingEntry>> Entries, TArray<const FMVVMBlueprintViewBinding*>& OutBindings, TArray<UMVVMBlueprintViewEvent*>& OutEvents)
{
	for (const TSharedPtr<FBindingEntry>& Entry : Entries)
	{
		if (Entry->GetRowType() == FBindingEntry::ERowType::Binding)
		{
			const FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(BlueprintView);
			if (Binding != nullptr)
			{
				OutBindings.AddUnique(Binding);
			}
		}

		if (Entry->GetRowType() == FBindingEntry::ERowType::Event)
		{
			UMVVMBlueprintViewEvent* Event = Entry->GetEvent();
			if (Event)
			{
				OutEvents.AddUnique(Event);
			}
		}
		GatherAllChildBindings(BlueprintView, Entry->GetAllChildren(), OutBindings, OutEvents);
	}
}

void FRowHelper::DeleteEntries(const UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintView* BlueprintView, TArrayView<const TSharedPtr<FBindingEntry>> Selection)
{
	if (WidgetBlueprint && BlueprintView)
	{
		TArray<const FMVVMBlueprintViewBinding*> BindingsToRemove;
		TArray<UMVVMBlueprintViewEvent*> EventsToRemove;
		GatherAllChildBindings(BlueprintView, Selection, BindingsToRemove, EventsToRemove);

		if (BindingsToRemove.Num() == 0 && EventsToRemove.Num() == 0)
		{
			return;
		}

		TArray<FText> BindingDisplayNames;
		for (const FMVVMBlueprintViewBinding* Binding : BindingsToRemove)
		{
			BindingDisplayNames.Add(FText::FromString(Binding->GetDisplayNameString(WidgetBlueprint)));
		}
		for (const UMVVMBlueprintViewEvent* Event : EventsToRemove)
		{
			BindingDisplayNames.Add(Event->GetDisplayName(true));
		}

		const FText Message = FText::Format(BindingDisplayNames.Num() == 1 ?
			LOCTEXT("ConfirmDeleteSingle", "Are you sure that you want to delete this binding/event?\n\n{1}") :
			LOCTEXT("ConfirmDeleteMultiple", "Are you sure that you want to delete these {0} bindings/events?\n\n{1}"),
			BindingDisplayNames.Num(),
			FText::Join(FText::FromString("\n"), BindingDisplayNames));

		const FText Title = LOCTEXT("DeleteBindings", "Delete Bindings?");
		if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::Yes, Message, Title) == EAppReturnType::Yes)
		{
			FScopedTransaction Transaction(LOCTEXT("DeleteBindingsTransaction", "Delete Bindings"));
			BlueprintView->Modify();

			for (int32 BindingIndex = BindingsToRemove.Num() - 1; BindingIndex > -1; BindingIndex--)
			{
				const FMVVMBlueprintViewBinding* Binding = BindingsToRemove[BindingIndex];
				BlueprintView->RemoveBinding(Binding);
			}
			for (UMVVMBlueprintViewEvent* Event : EventsToRemove)
			{
				BlueprintView->RemoveEvent(Event);
			}
		}
	}
}

void FRowHelper::ShowBlueprintGraph(FBlueprintEditor* BlueprintEditor, UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintView* BlueprintView, TArrayView<const TSharedPtr<FBindingEntry>> Entries)
{
	auto ShowGraph = [WidgetBlueprint, BlueprintEditor](UEdGraph* Graph)
		{
			if (Graph && BlueprintEditor)
			{
				if (Graph->HasAnyFlags(RF_Transient))
				{
					BlueprintEditor->OpenDocument(Graph, FDocumentTracker::OpenNewDocument);
				}
				else
				{
					FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Graph);
				}
			}
		};

	const UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	for (const TSharedPtr<FBindingEntry>& Entry : Entries)
	{
		if (Entry->GetRowType() == FBindingEntry::ERowType::Binding)
		{
			if (FMVVMBlueprintViewBinding* ViewBinding = Entry->GetBinding(BlueprintView))
			{
				UMVVMBlueprintViewConversionFunction* ConversionFunctionA = ViewBinding->Conversion.GetConversionFunction(true);
				UMVVMBlueprintViewConversionFunction* ConversionFunctionB = ViewBinding->Conversion.GetConversionFunction(false);
				if (ConversionFunctionA)
				{
					ShowGraph(ConversionFunctionA->GetWrapperGraph());
				}
				if (ConversionFunctionB)
				{
					ShowGraph(ConversionFunctionB->GetWrapperGraph());
				}
			}
		}
		else if (Entry->GetRowType() == FBindingEntry::ERowType::Event)
		{
			if (UMVVMBlueprintViewEvent* Event = Entry->GetEvent())
			{
				ShowGraph(Event->GetWrapperGraph());
			}
		}
	}
}

TOptional<FMVVMBlueprintPropertyPath> FRowHelper::DropFieldSelector(const UWidgetBlueprint* WidgetBlueprint, const FDragDropEvent& DragDropEvent)
{
	if (WidgetBlueprint == nullptr)
	{
		return TOptional<FMVVMBlueprintPropertyPath>();
	}

	TSharedPtr<FDecoratedDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
	if (!DragDropOp.IsValid())
	{
		return TOptional<FMVVMBlueprintPropertyPath>();
	}

	// Accept all drag-drop operations that are widget properties, but only accept view model fields when we are dropping into the Source box.
	if (!DragDropOp->IsOfType<FWidgetPropertyDragDropOp>() && (!DragDropOp->IsOfType<FViewModelFieldDragDropOp>()))
	{
		return TOptional<FMVVMBlueprintPropertyPath>();
	}

	TSharedPtr<FViewModelFieldDragDropOp> ViewModelFieldDragDropOp = DragDropEvent.GetOperationAs<FViewModelFieldDragDropOp>();
	TSharedPtr<FWidgetPropertyDragDropOp> WidgetPropertyDragDropOp = DragDropEvent.GetOperationAs<FWidgetPropertyDragDropOp>();
	bool bIsViewModelProperty = ViewModelFieldDragDropOp != nullptr;

	UWidgetBlueprint* DragDropWidgetBP = bIsViewModelProperty ? ViewModelFieldDragDropOp->WidgetBP.Get() : WidgetPropertyDragDropOp->WidgetBP.Get();
	if (WidgetBlueprint != DragDropWidgetBP)
	{
		return TOptional<FMVVMBlueprintPropertyPath>();
	}

	TArray<FFieldVariant> FieldPath = bIsViewModelProperty ? ViewModelFieldDragDropOp->DraggedField : WidgetPropertyDragDropOp->DraggedPropertyPath;
	FMVVMBlueprintPropertyPath PropertyPath;
	for (const FFieldVariant& Field : FieldPath)
	{
		PropertyPath.AppendPropertyPath(WidgetBlueprint, FMVVMConstFieldVariant(Field));
	}

	UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	if (bIsViewModelProperty)
	{
		if (ViewModelFieldDragDropOp->ViewModelId.IsValid())
		{
			PropertyPath.SetViewModelId(ViewModelFieldDragDropOp->ViewModelId);
		}
		else
		{
			return TOptional<FMVVMBlueprintPropertyPath>();
		}
	}
	else
	{
		if (UWidget* OwnerWidgetPtr = WidgetPropertyDragDropOp->OwnerWidget.Get())
		{
			// Search for the widget by its name in the widget tree
			// If the widget is not found, we know it is the root preview widget so we use the blueprint name.
			if (WidgetBlueprint->WidgetTree->FindWidget(OwnerWidgetPtr->GetFName()) && WidgetBlueprint->GetFName() != OwnerWidgetPtr->GetFName())
			{
				PropertyPath.SetWidgetName(OwnerWidgetPtr->GetFName());
			}
			else
			{
				PropertyPath.SetSelfContext();
			}
		}
	}

	return PropertyPath;
}

void FRowHelper::DragEnterFieldSelector(const UWidgetBlueprint* WidgetBlueprint, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDecoratedDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
	if (DragDropOp.IsValid())
	{
		// Accept all drag-drop operations that are widget properties, but only accept view model fields when we are dropping into the Source box.
		if (DragDropOp->IsOfType<FWidgetPropertyDragDropOp>() || DragDropOp->IsOfType<FViewModelFieldDragDropOp>())
		{
			TSharedPtr<FViewModelFieldDragDropOp> ViewModelFieldDragDropOp = DragDropEvent.GetOperationAs<FViewModelFieldDragDropOp>();
			TSharedPtr<FWidgetPropertyDragDropOp> WidgetPropertyDragDropOp = DragDropEvent.GetOperationAs<FWidgetPropertyDragDropOp>();
			bool IsViewModelProperty = false;

			if (ViewModelFieldDragDropOp)
			{
				IsViewModelProperty = true;
			}

			UWidgetBlueprint* DragDropWidgetBP = IsViewModelProperty ? ViewModelFieldDragDropOp->WidgetBP.Get() : WidgetPropertyDragDropOp->WidgetBP.Get();
			if (DragDropWidgetBP && DragDropWidgetBP == WidgetBlueprint)
			{
				DragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
			}
		}
		else
		{
			DragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
		}
	}
}

namespace Private
{

TArray<TSharedPtr<FBindingEntry>> WeakToSharedPtr(const TSharedPtr<TArray<TWeakPtr<FBindingEntry>>>& CopiedEntries)
{
	TArray<TSharedPtr<FBindingEntry>> Result;
	Result.Reserve(CopiedEntries->Num());
	for (TWeakPtr<FBindingEntry> Entry : *CopiedEntries)
	{
		if (TSharedPtr<FBindingEntry> Pin = Entry.Pin())
		{
			Result.Add(Pin);
		}
	}
	return Result;
}

void HandleDeleteEntries(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintView* View, TSharedPtr<TArray<TWeakPtr<FBindingEntry>>> Entries)
{
	FRowHelper::DeleteEntries(WidgetBlueprint, View, WeakToSharedPtr(Entries));
}

void HandleResetSelectedPin(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintView* View, TSharedPtr<TArray<TWeakPtr<FBindingEntry>>> Entries)
{
	const UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	for (const TSharedPtr<FBindingEntry>& Entry : WeakToSharedPtr(Entries))
	{
		if (Entry->GetRowType() == FBindingEntry::ERowType::EventParameter)
		{
			EditorSubsystem->ResetPinToDefaultValue(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterId());
		}
		else if (Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter)
		{
			if (FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(View))
			{
				const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);
				EditorSubsystem->ResetPinToDefaultValue(WidgetBlueprint, *Binding, Entry->GetBindingParameterId(), bSourceToDestination);
			}
		}
	}
}

void HandleBreakSelectedPin(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintView* View, TSharedPtr<TArray<TWeakPtr<FBindingEntry>>> Entries)
{
	const UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	for (const TSharedPtr<FBindingEntry>& Entry : WeakToSharedPtr(Entries))
	{
		if (Entry->GetRowType() == FBindingEntry::ERowType::EventParameter)
		{
			EditorSubsystem->SplitPin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterId());
		}
		else if (Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter)
		{
			if (FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(View))
			{
				const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);
				EditorSubsystem->SplitPin(WidgetBlueprint, *Binding, Entry->GetBindingParameterId(), bSourceToDestination);
			}
		}
	}
}

void HandleRecombineSelectedPin(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintView* View, TSharedPtr<TArray<TWeakPtr<FBindingEntry>>> Entries)
{
	const UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	for (const TSharedPtr<FBindingEntry>& Entry : WeakToSharedPtr(Entries))
	{
		if (Entry->GetRowType() == FBindingEntry::ERowType::EventParameter)
		{
			EditorSubsystem->RecombinePin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterId());
		}
		else if (Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter)
		{
			if (FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(View))
			{
				const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);
				EditorSubsystem->RecombinePin(WidgetBlueprint, *Binding, Entry->GetBindingParameterId(), bSourceToDestination);
			}
		}
	}
}

void HandleResetOrphanedSelectedPin(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintView* View, TSharedPtr<TArray<TWeakPtr<FBindingEntry>>> Entries)
{
	const UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	for (const TSharedPtr<FBindingEntry>& Entry : WeakToSharedPtr(Entries))
	{
		if (Entry->GetRowType() == FBindingEntry::ERowType::EventParameter)
		{
			EditorSubsystem->ResetOrphanedPin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterId());
		}
		else if (Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter)
		{
			if (FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(View))
			{
				const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);
				EditorSubsystem->ResetOrphanedPin(WidgetBlueprint, *Binding, Entry->GetBindingParameterId(), bSourceToDestination);
			}
		}
	}
}

} //namespace

FMenuBuilder FRowHelper::CreateContextMenu(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintView* View, TConstArrayView<TSharedPtr<FBindingEntry>> Entries)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	if (WidgetBlueprint && View && Entries.Num() > 0)
	{
		TSharedPtr<TArray<TWeakPtr<FBindingEntry>>> CopiedEntries;
		CopiedEntries = MakeShared<TArray<TWeakPtr<FBindingEntry>>>();
		CopiedEntries->Reserve(Entries.Num());
		for (const TSharedPtr<FBindingEntry>& Entry : Entries)
		{
			CopiedEntries->Add(Entry);
		}

		const UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		{
			bool bCanRemoveEntry = true;
			for (const TSharedPtr<FBindingEntry>& Entry : Entries)
			{
				switch (Entry->GetRowType())
				{
				case FBindingEntry::ERowType::Group:
				case FBindingEntry::ERowType::Binding:
				case FBindingEntry::ERowType::Event:
					break;
				default:
					bCanRemoveEntry = false;
					break;
				}
			}

			FUIAction RemoveAction;
			RemoveAction.ExecuteAction = FExecuteAction::CreateStatic(&Private::HandleDeleteEntries, WidgetBlueprint, View, CopiedEntries);
			RemoveAction.CanExecuteAction = FCanExecuteAction::CreateLambda([bCanRemoveEntry]() { return bCanRemoveEntry; });
			MenuBuilder.AddMenuEntry(LOCTEXT("RemoveBinding", "Remove"),
				LOCTEXT("RemoveBindingTooltip", "Remove bindings or events."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
				RemoveAction);
		}

		{
			bool bCanSplitPin = true;
			bool bCanRecombinePin = true;
			bool bCanRecombinePinVisible = true;
			bool bCanResetPin = true;
			bool bCanResetPinVisible = true;
			bool bCanResetOrphanedPin = true;
			auto SetAll = [&](bool Value)
				{
					bCanSplitPin = Value;
					bCanRecombinePin = Value;
					bCanRecombinePinVisible = Value;
					bCanResetPin = Value;
					bCanResetPinVisible = Value;
					bCanResetOrphanedPin = Value;
				};
			auto AllFalse = [&]()
				{
					return !bCanRecombinePin && !bCanSplitPin && !bCanRecombinePinVisible && bCanResetPin && !bCanResetPinVisible && !bCanResetOrphanedPin;
				};

			for (const TSharedPtr<FBindingEntry>& Entry : Entries)
			{
				if (Entry->GetRowType() == FBindingEntry::ERowType::EventParameter)
				{
					if (!EditorSubsystem->CanSplitPin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterId()))
					{
						bCanSplitPin = false;
					}
					if (!EditorSubsystem->CanRecombinePin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterId()))
					{
						bCanRecombinePin = false;
					}
					if (!EditorSubsystem->CanResetPinToDefaultValue(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterId()))
					{
						bCanResetPin = false;
					}
					if (!EditorSubsystem->CanResetOrphanedPin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterId()))
					{
						bCanResetOrphanedPin = false;
					}

					UMVVMBlueprintViewEvent* ViewEvent = Entry->GetEvent();
					UEdGraphPin* GraphPin = ViewEvent ? ViewEvent->GetOrCreateGraphPin(Entry->GetEventParameterId()) : nullptr;
					if (GraphPin == nullptr)
					{
						bCanRecombinePinVisible = false;
						bCanResetPinVisible = false;
					}
					else
					{
						if (GraphPin->ParentPin == nullptr)
						{
							bCanRecombinePinVisible = false;
						}
						if (!FMVVMBlueprintPin::IsInputPin(GraphPin) || GetDefault<UEdGraphSchema_K2>()->ShouldHidePinDefaultValue(GraphPin))
						{
							bCanResetPinVisible = false;
						}
					}
				}
				else if (Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter)
				{
					const FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(View);
					if (Binding)
					{
						const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);
						if (!EditorSubsystem->CanSplitPin(WidgetBlueprint, *Binding, Entry->GetBindingParameterId(), bSourceToDestination))
						{
							bCanSplitPin = false;
						}
						if (!EditorSubsystem->CanRecombinePin(WidgetBlueprint, *Binding, Entry->GetBindingParameterId(), bSourceToDestination))
						{
							bCanRecombinePin = false;
						}
						if (!EditorSubsystem->CanResetPinToDefaultValue(WidgetBlueprint, *Binding, Entry->GetBindingParameterId(), bSourceToDestination))
						{
							bCanResetPin = false;
						}
						if (!EditorSubsystem->CanResetOrphanedPin(WidgetBlueprint, *Binding, Entry->GetBindingParameterId(), bSourceToDestination))
						{
							bCanResetOrphanedPin = false;
						}

						UEdGraphPin* GraphPin = EditorSubsystem->GetConversionFunctionArgumentPin(WidgetBlueprint, *Binding, Entry->GetBindingParameterId(), bSourceToDestination);
						if (GraphPin == nullptr)
						{
							bCanRecombinePinVisible = false;
							bCanResetPinVisible = false;
						}
						else
						{
							if (GraphPin->ParentPin == nullptr)
							{
								bCanRecombinePinVisible = false;
							}
							if (!FMVVMBlueprintPin::IsInputPin(GraphPin) || GetDefault<UEdGraphSchema_K2>()->ShouldHidePinDefaultValue(GraphPin))
							{
								bCanResetPinVisible = false;
							}
						}
					}
					else
					{
						SetAll(false);
					}
				}
				else
				{
					SetAll(false);
				}

				if (AllFalse())
				{
					break;
				}
			}

			if (bCanResetPinVisible)
			{
				FUIAction ResetPinAction;
				ResetPinAction.ExecuteAction = FExecuteAction::CreateStatic(&Private::HandleResetSelectedPin, WidgetBlueprint, View, CopiedEntries);
				ResetPinAction.CanExecuteAction = FCanExecuteAction::CreateLambda([bCanResetPin]() { return bCanResetPin; });
				MenuBuilder.AddMenuEntry(LOCTEXT("ResetPin", "Reset to Default Value"),
					LOCTEXT("ResetPinTooltip", "Reset value of this pin to the default"),
					FSlateIcon(),
					ResetPinAction);
			}
			if (bCanSplitPin)
			{
				FUIAction SplitPinAction;
				SplitPinAction.ExecuteAction = FExecuteAction::CreateStatic(&Private::HandleBreakSelectedPin, WidgetBlueprint, View, CopiedEntries);
				MenuBuilder.AddMenuEntry(LOCTEXT("BreakPin", "Split Struct Pin"),
					LOCTEXT("BreakPinTooltip", "Breaks a struct pin in to a separate pin per element."),
					FSlateIcon(),
					SplitPinAction);
			}
			if (bCanRecombinePinVisible)
			{
				FUIAction RecombinePinAction;
				RecombinePinAction.ExecuteAction = FExecuteAction::CreateStatic(&Private::HandleRecombineSelectedPin, WidgetBlueprint, View, CopiedEntries);
				RecombinePinAction.CanExecuteAction = FCanExecuteAction::CreateLambda([bCanRecombinePin]() { return bCanRecombinePin; });
				MenuBuilder.AddMenuEntry(LOCTEXT("RecombinePin", "Recombine Struct Pin"),
					LOCTEXT("RecombinePinTooltip", "Takes struct pins that have been broken in to composite elements and combines them back to a single struct pin."),
					FSlateIcon(),
					RecombinePinAction);
			}
			if (bCanResetOrphanedPin)
			{
				FUIAction ResetOrphanedPinAction;
				ResetOrphanedPinAction.ExecuteAction = FExecuteAction::CreateStatic(&Private::HandleResetOrphanedSelectedPin, WidgetBlueprint, View, CopiedEntries);
				MenuBuilder.AddMenuEntry(LOCTEXT("ResetOrphanedPin", "Remove the Orphaned Struct Pin"),
					LOCTEXT("ResetOrphanedPinTooltip", "Removes pins that used to exist but do not exist anymore."),
					FSlateIcon(),
					ResetOrphanedPinAction);
			}
		}
	}

	return MenuBuilder;
}

} // namespace UE::MVVM::BindingEntry

#undef LOCTEXT_NAMESPACE
