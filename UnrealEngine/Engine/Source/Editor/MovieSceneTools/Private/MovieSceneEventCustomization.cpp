// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneEventCustomization.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneEventUtils.h"

#include "UObject/UnrealType.h"
#include "Channels/MovieSceneEvent.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneEventSectionBase.h"
#include "Algo/Find.h"
#include "MovieSceneSequence.h"
#include "ISequencerModule.h"
#include "ScopedTransaction.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Views/SExpanderArrow.h"

#include "PropertyHandle.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"

#include "K2Node_Variable.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "SGraphActionMenu.h"
#include "BlueprintActionMenuUtils.h"
#include "BlueprintActionMenuBuilder.h"
#include "BlueprintActionMenuItem.h"
#include "BlueprintFunctionNodeSpawner.h"

#include "Styling/AppStyle.h"
#include "EditorFontGlyphs.h"

#define LOCTEXT_NAMESPACE "MovieSceneEventCustomization"


namespace UE_MovieSceneEventCustomization
{
	void CollectQuickBindActions(FGraphActionListBuilderBase& OutAllActions, UBlueprint* Blueprint, UClass* BoundObjectPinClass)
	{
		// Build up the context object
		auto RejectAnyNonFunctions = [](const FBlueprintActionFilter& Filter, FBlueprintActionInfo& BlueprintAction)
		{
			const UFunction* Function = BlueprintAction.GetAssociatedFunction();
			return Function == nullptr || Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
		};
		auto RejectAnyUnboundActions = [](const FBlueprintActionFilter& Filter, FBlueprintActionInfo& BlueprintAction)
		{
			return BlueprintAction.GetBindings().Num() <= 0;
		};


		FBlueprintActionMenuBuilder ContextMenuBuilder(nullptr);

		if (BoundObjectPinClass)
		{
			// Add actions that are relevant to the bound object from the pin class
			FBlueprintActionFilter CallOnMemberFilter(FBlueprintActionFilter::BPFILTER_RejectGlobalFields | FBlueprintActionFilter::BPFILTER_RejectPermittedSubClasses);
			CallOnMemberFilter.PermittedNodeTypes.Add(UK2Node_CallFunction::StaticClass());
			CallOnMemberFilter.Context.Blueprints.Add(Blueprint);

			for (FObjectProperty* ObjectProperty : TFieldRange<FObjectProperty>(BoundObjectPinClass))
			{
				if (ObjectProperty->HasAnyPropertyFlags(CPF_BlueprintVisible) && (ObjectProperty->HasMetaData(FBlueprintMetadata::MD_ExposeFunctionCategories) || FBlueprintEditorUtils::IsSCSComponentProperty(ObjectProperty)))
				{
					CallOnMemberFilter.Context.SelectedObjects.Add(ObjectProperty);
					FBlueprintActionFilter::AddUnique(CallOnMemberFilter.TargetClasses, ObjectProperty->PropertyClass);
				}
			}

			FBlueprintActionFilter::AddUnique(CallOnMemberFilter.TargetClasses, BoundObjectPinClass);
	
			// This removes duplicate entries (ie. Set Static Mesh and Set Static Mesh (StaticMeshComponent)), 
			// but also prevents displaying functions on BP components. Comment out for now.
			//CallOnMemberFilter.AddRejectionTest(FBlueprintActionFilter::FRejectionTestDelegate::CreateStatic(RejectAnyUnboundActions));
			
			CallOnMemberFilter.AddRejectionTest(MAKE_ACTION_FILTER_REJECTION_TEST(RejectAnyNonFunctions)->WithFlags(EActionFilterTestFlags::CacheResults));

			ContextMenuBuilder.AddMenuSection(CallOnMemberFilter, FText::FromName(BoundObjectPinClass->GetFName()), 0);
		}

		{
			// Add all actions that are relevant to the sequence director BP itself
			FBlueprintActionFilter MenuFilter(FBlueprintActionFilter::BPFILTER_RejectGlobalFields | FBlueprintActionFilter::BPFILTER_RejectPermittedSubClasses);

			MenuFilter.PermittedNodeTypes.Add(UK2Node_CallFunction::StaticClass());

			MenuFilter.Context.Blueprints.Add(Blueprint);
			MenuFilter.Context.Graphs.Append(Blueprint->UbergraphPages);
			MenuFilter.Context.Graphs.Append(Blueprint->FunctionGraphs);

			if (Blueprint->SkeletonGeneratedClass)
			{
				FBlueprintActionFilter::AddUnique(MenuFilter.TargetClasses, Blueprint->SkeletonGeneratedClass);
			}
			MenuFilter.AddRejectionTest(MAKE_ACTION_FILTER_REJECTION_TEST(RejectAnyNonFunctions)->WithFlags(EActionFilterTestFlags::CacheResults));


			ContextMenuBuilder.AddMenuSection(MenuFilter, LOCTEXT("SequenceDirectorMenu", "This Sequence"), 0);
		}

		ContextMenuBuilder.RebuildActionList();

		OutAllActions.Append(ContextMenuBuilder);
	}

	void CollectAllRebindActions(FGraphActionListBuilderBase& OutAllActions, UBlueprint* Blueprint)
	{
		// Build up the context object
		auto RejectAnyForeignFunctions = [](const FBlueprintActionFilter& Filter, FBlueprintActionInfo& BlueprintAction, UBlueprint* Blueprint)
		{
			const UBlueprintFunctionNodeSpawner* FunctionNodeSpawner = Cast<UBlueprintFunctionNodeSpawner>(BlueprintAction.NodeSpawner);
			const UFunction* FunctionToCall = FunctionNodeSpawner ? FunctionNodeSpawner->GetFunction() : nullptr;

			if (!FunctionToCall || FunctionToCall->HasAnyFunctionFlags(FUNC_BlueprintPure))
			{
				return true;
			}

			UClass* OuterClass = CastChecked<UClass>(FunctionToCall->GetOuter());
			return OuterClass->ClassGeneratedBy != Blueprint;
		};

		FBlueprintActionFilter MenuFilter(FBlueprintActionFilter::BPFILTER_RejectGlobalFields | FBlueprintActionFilter::BPFILTER_RejectPermittedSubClasses);
		MenuFilter.PermittedNodeTypes.Add(UK2Node_CallFunction::StaticClass());

		MenuFilter.Context.Blueprints.Add(Blueprint);

		MenuFilter.AddRejectionTest(MAKE_ACTION_FILTER_REJECTION_TEST(RejectAnyForeignFunctions, Blueprint));

		if (Blueprint->SkeletonGeneratedClass)
		{
			FBlueprintActionFilter::AddUnique(MenuFilter.TargetClasses, Blueprint->SkeletonGeneratedClass);
		}

		FBlueprintActionMenuBuilder ContextMenuBuilder(nullptr);

		ContextMenuBuilder.AddMenuSection(MenuFilter, LOCTEXT("SequenceDirectorMenu", "This Sequence"), 0);
		ContextMenuBuilder.RebuildActionList();
		OutAllActions.Append(ContextMenuBuilder);
	}
}




TSharedRef<IPropertyTypeCustomization> FMovieSceneEventCustomization::MakeInstance()
{
	return MakeShared<FMovieSceneEventCustomization>();
}

TSharedRef<IPropertyTypeCustomization> FMovieSceneEventCustomization::MakeInstance(UMovieSceneSection* InSection)
{
	TSharedRef<FMovieSceneEventCustomization> Custo = MakeShared<FMovieSceneEventCustomization>();
	Custo->WeakExternalSection = InSection;
	return Custo;
}

void FMovieSceneEventCustomization::GetEditObjects(TArray<UObject*>& OutObjects) const
{
	UMovieSceneEventSectionBase* ExternalSection = Cast<UMovieSceneEventSectionBase>(WeakExternalSection.Get());
	if (ExternalSection)
	{
		OutObjects.Add(ExternalSection);
	}
	else
	{
		PropertyHandle->GetOuterObjects(OutObjects);
	}
}

void FMovieSceneEventCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{}

void FMovieSceneEventCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	ChildBuilder.AddCustomRow(FText())
	.NameContent()
	[
		SNew(STextBlock)
		.Font(CustomizationUtils.GetRegularFont())
		.Text(LOCTEXT("EventValueText", "Event"))
	]
	.ValueContent()
	.MinDesiredWidth(200.f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		[
			SNew(SComboButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.OnGetMenuContent(this, &FMovieSceneEventCustomization::GetMenuContent)
			.CollapseMenuOnParentFocus(true)
			.ContentPadding(FMargin(4.f, 0.f))
			.ButtonContent()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &FMovieSceneEventCustomization::GetEventIcon)
				]

				+ SHorizontalBox::Slot()
				.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(CustomizationUtils.GetRegularFont())
					.Text(this, &FMovieSceneEventCustomization::GetEventName)
				]
			]
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FMovieSceneEventCustomization::NavigateToDefinition), LOCTEXT("NavigateToDefinition_Tip", "Navigate to this event's definition"))
		]
	];

	const bool bAnyBoundEvents = GetAllValidEndpoints().Num() != 0;
	if (bAnyBoundEvents)
	{
		FText CallInEditorText    = LOCTEXT("CallInEditor_Label",   "Call In Editor");
		FText CallInEditorTooltip = LOCTEXT("CallInEditor_Tooltip", "When checked, this event will be triggered in the Editor outside of PIE.\n\nBEWARE: ANY CHANGES MADE AS A RESULT OF THIS EVENT BEING CALLED MAY END UP BEING SAVED IN THE CURRENT LEVEL OR ASSET.");

		ChildBuilder.AddCustomRow(CallInEditorText)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(CallInEditorText)
			.ToolTipText(CallInEditorTooltip)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.ToolTipText(CallInEditorTooltip)
			.IsChecked(this, &FMovieSceneEventCustomization::GetCallInEditorCheckState)
			.OnCheckStateChanged(this, &FMovieSceneEventCustomization::OnSetCallInEditorCheckState)
		];
	}


	UK2Node* CommonEndpoint = GetCommonEndpoint();
	if (!CommonEndpoint)
	{
		return;
	}

	TArray<UEdGraphPin*> ValidBoundObjectPins;
	for (UEdGraphPin* Pin : CommonEndpoint->Pins)
	{
		if (Pin->Direction == EGPD_Output && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface))
		{
			ValidBoundObjectPins.Add(Pin);
		}
	}


	if (ValidBoundObjectPins.Num() > 0)
	{
		FText BoundObjectPinText    = LOCTEXT("BoundObjectPin_Label", "Pass Bound Object To");
		FText BoundObjectPinTooltip = LOCTEXT("BoundObjectPin_Tooltip", "Specifies a pin to pass the bound object(s) through when the event is triggered. Interface and object pins are both supported.");

		ChildBuilder.AddCustomRow(BoundObjectPinText)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(BoundObjectPinText)
			.ToolTipText(BoundObjectPinTooltip)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SComboButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.ToolTipText(BoundObjectPinTooltip)
			.OnGetMenuContent(this, &FMovieSceneEventCustomization::GetBoundObjectPinMenuContent)
			.CollapseMenuOnParentFocus(true)
			.ContentPadding(FMargin(4.f, 0.f))
			.ButtonContent()
			[
				SNew(SBox)
				.HeightOverride(21.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SImage)
						.Image(this, &FMovieSceneEventCustomization::GetBoundObjectPinIcon)
					]

					+ SHorizontalBox::Slot()
					.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(CustomizationUtils.GetRegularFont())
						.Text(this, &FMovieSceneEventCustomization::GetBoundObjectPinText)
					]
				]
			]
		];
	}


	UFunction* CommonFunction = nullptr;
	UBlueprint* Blueprint = CommonEndpoint->GetBlueprint();
	if (Blueprint)
	{
		if (UK2Node_Event* Event = Cast<UK2Node_Event>(CommonEndpoint))
		{
			CommonFunction = Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass->FindFunctionByName(Event->GetFunctionName()) : nullptr;
		}
		else if (UK2Node_FunctionEntry* EndpointEntry = Cast<UK2Node_FunctionEntry>(CommonEndpoint))
		{
			CommonFunction = EndpointEntry->FindSignatureFunction();
		}
		else
		{
			// @todo: Error not supported
		}

		Blueprint->OnCompiled().AddSP(this, &FMovieSceneEventCustomization::OnBlueprintCompiled);
	}

	if (CommonFunction)
	{
		IDetailCategoryBuilder& DetailCategoryBuilder = ChildBuilder.GetParentCategory().GetParentLayout().EditCategory("Payload", LOCTEXT("PayloadLabel", "Payload"), ECategoryPriority::Uncommon);

		bool bPayloadUpToDate = true;

		for (int32 Index = 0; Index < RawData.Num(); ++Index)
		{
			TSharedPtr<FStructOnScope> StructData = MakeShared<FStructOnScope>(CommonFunction);

			const FMovieSceneEvent* EntryPoint = static_cast<FMovieSceneEvent*>(RawData[Index]);

			TArray<FName, TInlineAllocator<8>> AllValidNames;

			for (FProperty* Field : TFieldRange<FProperty>(CommonFunction))
			{
				if (Field->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm) || Field->GetFName() == EntryPoint->BoundObjectPinName)
				{
					continue;
				}

				const FMovieSceneEventPayloadVariable* PayloadVariable = EntryPoint->PayloadVariables.Find(Field->GetFName());
				if (PayloadVariable)
				{
					AllValidNames.Add(Field->GetFName());

					// We have an override for this variable
					const bool bImportSuccess = FBlueprintEditorUtils::PropertyValueFromString(Field, PayloadVariable->Value, StructData->GetStructMemory());
					if (!bImportSuccess)
					{
						// @todo: error
					}
				}
				else if (UEdGraphPin* Pin = CommonEndpoint->FindPin(Field->GetFName(), EGPD_Output))
				{
					if (!Pin->DefaultValue.IsEmpty())
					{
						const bool bImportSuccess = FBlueprintEditorUtils::PropertyValueFromString(Field, Pin->DefaultValue, StructData->GetStructMemory());
						if (!bImportSuccess)
						{
							// @todo: error
						}
					}
				}

				IDetailPropertyRow* ExternalRow = DetailCategoryBuilder.AddExternalStructureProperty(StructData.ToSharedRef(), Field->GetFName(), EPropertyLocation::Default, FAddPropertyParams().ForceShowProperty());

				TSharedPtr<IPropertyHandle> LocalVariableProperty = ExternalRow->GetPropertyHandle();
				FSimpleDelegate Delegate = FSimpleDelegate::CreateSP(this, &FMovieSceneEventCustomization::OnPayloadVariableChanged, StructData.ToSharedRef(), LocalVariableProperty);

				LocalVariableProperty->SetOnPropertyValueChanged(Delegate);
				LocalVariableProperty->SetOnChildPropertyValueChanged(Delegate);
			}

			bPayloadUpToDate &= (AllValidNames.Num() == EntryPoint->PayloadVariables.Num());
		}

		if (!bPayloadUpToDate)
		{
			DetailCategoryBuilder.AddCustomRow(FText())
			.WholeRowContent()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.Padding(FMargin(5.f, 0.f, 5.f, 0.f))
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "Log.Warning")
					.Font(FAppStyle::GetFontStyle("FontAwesome.10"))
					.Text(FEditorFontGlyphs::Exclamation_Triangle)
				]

				+ SHorizontalBox::Slot()
				.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "Log.Warning")
					.Text(LOCTEXT("PayloadOutOfDateError", "Payload variables may be out-of-date. Please compile the blueprint."))
				]
			];
		}
	}
}

void FMovieSceneEventCustomization::OnPayloadVariableChanged(TSharedRef<FStructOnScope> InStructData, TSharedPtr<IPropertyHandle> LocalVariableProperty)
{
	// This function should only ever be bound if all the entry points call the same function
	FProperty* Property = LocalVariableProperty->GetProperty();
	if (!Property)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SetPayloadValue", "Set payload value"));

	bool bChangedAnything = false;

	UBlueprint* Blueprint = nullptr;

	FString NewValueString;

	const bool bSuccessfulTextExport = FBlueprintEditorUtils::PropertyValueToString(Property, InStructData->GetStructMemory(), NewValueString, nullptr);
	if (!bSuccessfulTextExport)
	{
		// @todo: error
		return;
	}


	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	if (!ensure(RawData.Num() == EditObjects.Num()))
	{
		return;
	}

	for (int32 Index = 0; Index < RawData.Num(); ++Index)
	{
		UMovieSceneEventSectionBase* EventSection = Cast<UMovieSceneEventSectionBase>(EditObjects[Index]);
		FMovieSceneEvent*  EntryPoint   = static_cast<FMovieSceneEvent*>(RawData[Index]);
		if (!EventSection || !EntryPoint)
		{
			continue;
		}

		EventSection->Modify();

		FMovieSceneEventPayloadVariable* PayloadVariable = EntryPoint->PayloadVariables.Find(Property->GetFName());
		if (!PayloadVariable)
		{
			PayloadVariable = &EntryPoint->PayloadVariables.Add(Property->GetFName());
		}

		PayloadVariable->Value = NewValueString;
		bChangedAnything = true;
	}

	if (bChangedAnything)
	{
		UBlueprint* BP = GetCommonEndpoint()->GetBlueprint();
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	else
	{
		Transaction.Cancel();
	}
}

ECheckBoxState FMovieSceneEventCustomization::GetCallInEditorCheckState() const
{
	ECheckBoxState CheckState = ECheckBoxState::Undetermined;
	for (UK2Node* Endpoint : GetAllValidEndpoints())
	{
		bool bCallInEditor = false;
		if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Endpoint))
		{
			bCallInEditor = CustomEvent->bCallInEditor;
		}
		else if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Endpoint))
		{
			bCallInEditor = FunctionEntry->MetaData.bCallInEditor;
		}

		if (CheckState == ECheckBoxState::Undetermined)
		{
			CheckState = bCallInEditor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		else if (bCallInEditor != (CheckState == ECheckBoxState::Checked) )
		{
			return ECheckBoxState::Undetermined;
		}
	}
	return CheckState;
}

void FMovieSceneEventCustomization::OnSetCallInEditorCheckState(ECheckBoxState NewCheckedState)
{
	FScopedTransaction Transaction(LOCTEXT("SetCallInEditor", "Set Call in Editor"));

	const bool bCallInEditor = (NewCheckedState == ECheckBoxState::Checked);

	TSet<UBlueprint*> Blueprints;

	for (UK2Node* Endpoint : GetAllValidEndpoints())
	{
		if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Endpoint))
		{
			CustomEvent->Modify();
			CustomEvent->bCallInEditor = bCallInEditor;
			Blueprints.Add(CustomEvent->GetBlueprint());
		}
		else if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Endpoint))
		{
			FunctionEntry->Modify();
			FunctionEntry->MetaData.bCallInEditor = bCallInEditor;
			Blueprints.Add(FunctionEntry->GetBlueprint());
		}
	}

	for (UBlueprint* Blueprint : Blueprints)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void FMovieSceneEventCustomization::OnBlueprintCompiled(UBlueprint*)
{
	PropertyUtilities->ForceRefresh();
}

UMovieSceneSequence* FMovieSceneEventCustomization::GetCommonSequence() const
{
	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	UMovieSceneSequence* CommonEventSequence = nullptr;

	for (UObject* Obj : EditObjects)
	{
		UMovieSceneSequence* ThisSequence = Obj ? Obj->GetTypedOuter<UMovieSceneSequence>() : nullptr;
		if (CommonEventSequence && CommonEventSequence != ThisSequence)
		{
			return nullptr;
		}

		CommonEventSequence = ThisSequence;
	}
	return CommonEventSequence;
}

UMovieSceneEventTrack* FMovieSceneEventCustomization::GetCommonTrack() const
{
	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	UMovieSceneEventTrack* CommonEventTrack = nullptr;

	for (UObject* Obj : EditObjects)
	{
		UMovieSceneEventTrack* ThisTrack = Obj ? Obj->GetTypedOuter<UMovieSceneEventTrack>() : nullptr;
		if (CommonEventTrack && CommonEventTrack != ThisTrack)
		{
			return nullptr;
		}

		CommonEventTrack = ThisTrack;
	}
	return CommonEventTrack;
}

void FMovieSceneEventCustomization::IterateEndpoints(TFunctionRef<bool(UK2Node*)> Callback) const
{
	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	if (!ensure(RawData.Num() == EditObjects.Num()))
	{
		return;
	}

	for (int32 Index = 0; Index < RawData.Num(); ++Index)
	{
		UMovieSceneEventSectionBase* EventSection = Cast<UMovieSceneEventSectionBase>(EditObjects[Index]);
		FMovieSceneEvent*  EntryPoint   = static_cast<FMovieSceneEvent*>(RawData[Index]);
		if (!EventSection || !EntryPoint)
		{
			continue;
		}

		UMovieSceneSequence*       Sequence       = EventSection->GetTypedOuter<UMovieSceneSequence>();
		FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);
		if (!SequenceEditor)
		{
			continue;
		}

		UBlueprint* SequenceDirectorBP = SequenceEditor->FindDirectorBlueprint(Sequence);
		if (SequenceDirectorBP)
		{
			UK2Node* Endpoint = FMovieSceneEventUtils::FindEndpoint(EntryPoint, EventSection, SequenceDirectorBP);
			if (!Callback(Endpoint))
			{
				return;
			}
		}
	}
}

TArray<UK2Node*> FMovieSceneEventCustomization::GetAllValidEndpoints() const
{
	TArray<UK2Node*> Endpoints;

	IterateEndpoints(
		[&Endpoints](UK2Node* Endpoint)
		{
			if (Endpoint)
			{
				Endpoints.Add(Endpoint);
			}
			return true;
		}
	);

	return Endpoints;
}

UK2Node* FMovieSceneEventCustomization::GetCommonEndpoint() const
{
	UK2Node* CommonEndpoint = nullptr;

	IterateEndpoints(
		[&CommonEndpoint](UK2Node* Endpoint)
		{
			if (CommonEndpoint && Endpoint != CommonEndpoint)
			{
				CommonEndpoint = nullptr;
				return false;
			}

			CommonEndpoint = Endpoint;
			return true;
		}
	);

	return CommonEndpoint;
}

TSharedRef<SWidget> FMovieSceneEventCustomization::GetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr, nullptr, true);

	UMovieSceneSequence*       Sequence       = GetCommonSequence();
	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);

	UBlueprint* DirectorBP = SequenceEditor ? SequenceEditor->FindDirectorBlueprint(Sequence) : nullptr;

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CreateEventEndpoint_Text",    "Create New Endpoint"),
		LOCTEXT("CreateEventEndpoint_Tooltip", "Creates a new event endpoint in this sequence's blueprint."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.CreateEventBinding"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMovieSceneEventCustomization::CreateEventEndpoint)
		)
	);

	const bool bAnyBoundEvents = GetAllValidEndpoints().Num() != 0;
	if (!bAnyBoundEvents && Sequence)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("CreateQuickBinding_Text",    "Quick Bind"),
			LOCTEXT("CreateQuickBinding_Tooltip", "Shows a list of functions on this object binding that can be bound directly to this event."),
			FNewMenuDelegate::CreateSP(this, &FMovieSceneEventCustomization::PopulateQuickBindSubMenu, Sequence),
			false /* bInOpenSubMenuOnClick */,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.CreateQuickBinding"),
			false /* bInShouldWindowAfterMenuSelection */
		);
	}
	else
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("Rebind_Text",         "Rebind To"),
			LOCTEXT("Rebind_Text_Tooltip", "Rebinds this event to a different function call or event node."),
			FNewMenuDelegate::CreateSP(this, &FMovieSceneEventCustomization::PopulateRebindSubMenu, Sequence),
			false /* bInOpenSubMenuOnClick */,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.CreateQuickBinding"),
			false /* bInShouldCloseWindowAfterMenuSelection */
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearEventEndpoint_Text",    "Clear"),
			LOCTEXT("ClearEventEndpoint_Tooltip", "Unbinds this event from its current binding."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.ClearEventBinding"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FMovieSceneEventCustomization::ClearEventEndpoint)
			)
		);
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FMovieSceneEventCustomization::GetBoundObjectPinMenuContent()
{
	UK2Node* CommonEndpoint = GetCommonEndpoint();
	if (CommonEndpoint)
	{
		bool bAnyValidPins        = false;
		bool bBoundObjectPinIsSet = false;

		FMenuBuilder MenuBuilder(true, nullptr, nullptr, true);
		for (UEdGraphPin* Pin : CommonEndpoint->Pins)
		{
			if (Pin->Direction == EGPD_Output && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface))
			{
				bAnyValidPins = true;

				FText PinText = FText::FromName(Pin->PinName);
				MenuBuilder.AddMenuEntry(
					PinText,
					FText::Format(LOCTEXT("SetBoundObjectPin_Tooltip", "When calling this event with a bound object, pass the object through pin {0}."), PinText),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FMovieSceneEventCustomization::SetBoundObjectPinName, Pin->PinName),
						FCanExecuteAction(),
						FIsActionChecked::CreateSP(this, &FMovieSceneEventCustomization::CompareBoundObjectPinName, Pin->PinName)
					),
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);
			}
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearBoundObjectPin_Label", "Clear"),
			LOCTEXT("ClearBoundObjectPin_Tooltip", "Clears the reference to the bound object pin, meaning any bound objects will not be passed to the event"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FMovieSceneEventCustomization::SetBoundObjectPinName, FName())
			)
		);

		if (bAnyValidPins || bBoundObjectPinIsSet)
		{
			return MenuBuilder.MakeWidget();
		}
		else
		{
			return SNew(STextBlock).Text(LOCTEXT("BoundObjectPinError_NoPins", "No compatible pins were found. Only Object and Interface pins can be bound to objects."));
		}
	}

	return SNew(STextBlock).Text(LOCTEXT("BoundObjectPinError_MultipleEvents", "Cannot choose a bound object pin with multiple events selected"));
}

bool FMovieSceneEventCustomization::CompareBoundObjectPinName(FName InPinName) const
{
	bool bMatch = false;

	auto Enumerator = [&bMatch, InPinName](void* Ptr, const int32, const int32)
	{
		bMatch = (static_cast<FMovieSceneEvent*>(Ptr)->BoundObjectPinName == InPinName);
		return bMatch;
	};

	PropertyHandle->EnumerateRawData(Enumerator);

	return bMatch;
}

void FMovieSceneEventCustomization::SetBoundObjectPinName(FName InNewBoundObjectPinName)
{
	FScopedTransaction Transaction(LOCTEXT("SetBoundObjectPinTransaction", "Set Bound Object Pin"));

	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	for (UObject* Object : EditObjects)
	{
		Object->Modify();
	}

	auto Enumerator = [InNewBoundObjectPinName](void* Ptr, const int32, const int32)
	{
		static_cast<FMovieSceneEvent*>(Ptr)->BoundObjectPinName = InNewBoundObjectPinName;
		return true;
	};
	PropertyHandle->EnumerateRawData(Enumerator);

	// Ensure that anything listening for property changed notifications are notified of the new binding
	PropertyHandle->NotifyFinishedChangingProperties();
	PropertyUtilities->ForceRefresh();
}

const FSlateBrush* FMovieSceneEventCustomization::GetBoundObjectPinIcon() const
{
	if (CompareBoundObjectPinName(NAME_None))
	{
		return nullptr;
	}

	return FAppStyle::GetBrush("Graph.Pin.Disconnected_VarA");
}

FText FMovieSceneEventCustomization::GetBoundObjectPinText() const
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	FName CommonPinName;

	for (void* Ptr : RawData)
	{
		FName PinName = static_cast<FMovieSceneEvent*>(Ptr)->BoundObjectPinName;
		if (CommonPinName == NAME_None)
		{
			CommonPinName = PinName;
		}
		else if (CommonPinName != PinName)
		{
			return LOCTEXT("MultiplePinValues", "Multiple Values");
		}
	}

	return FText::FromName(CommonPinName);
}

void FMovieSceneEventCustomization::PopulateQuickBindSubMenu(FMenuBuilder& MenuBuilder, UMovieSceneSequence* Sequence)
{
	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);
	if (!SequenceEditor)
	{
		return;
	}

	UBlueprint* Blueprint = SequenceEditor->GetOrCreateDirectorBlueprint(Sequence);
	if (!Blueprint)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();

	TOptional<FGuid> CommonObjectBindingID;
	{
		TArray<UObject*> EditObjects;
		GetEditObjects(EditObjects);

		for (UObject* Outer : EditObjects)
		{
			FGuid ThisBindingID;
			MovieScene->FindTrackBinding(*Outer->GetTypedOuter<UMovieSceneTrack>(), ThisBindingID);

			if (CommonObjectBindingID.IsSet() && CommonObjectBindingID != ThisBindingID)
			{
				CommonObjectBindingID.Reset();
				break;
			}

			CommonObjectBindingID = ThisBindingID;
		}
	}


	FMovieSceneEventEndpointParameters EventParams = FMovieSceneEventEndpointParameters::Generate(MovieScene, CommonObjectBindingID.Get(FGuid()));

	TSharedRef<SGraphActionMenu> ActionMenu = SNew(SGraphActionMenu)
		.OnCreateCustomRowExpander_Static([](const FCustomExpanderData& Data) -> TSharedRef<SExpanderArrow> { return SNew(SExpanderArrow, Data.TableRow); })
		.OnCollectAllActions_Static(UE_MovieSceneEventCustomization::CollectQuickBindActions, Blueprint, EventParams.BoundObjectPinClass)
		.OnActionSelected(this, &FMovieSceneEventCustomization::HandleQuickBindActionSelected, Blueprint, EventParams);

	ActionMenu->RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda(
		[FilterTextBox = ActionMenu->GetFilterTextBox()](double, float)
		{
			FSlateApplication::Get().SetKeyboardFocus(FilterTextBox);
			return EActiveTimerReturnType::Stop;
		}
	));

	MenuBuilder.AddWidget(
		SNew(SBox)
		.WidthOverride(300.f)
		.MaxDesiredHeight(500.f)
		[
			ActionMenu
		],
		FText()
	);
}

void FMovieSceneEventCustomization::HandleQuickBindActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedAction, ESelectInfo::Type InSelectionType, UBlueprint* Blueprint, FMovieSceneEventEndpointParameters Params)
{
	if (InSelectionType != ESelectInfo::OnMouseClick && InSelectionType != ESelectInfo::OnKeyPress)
	{
		return;
	}

	for (TSharedPtr<FEdGraphSchemaAction> Action : SelectedAction)
	{
		if (Action->GetTypeId() == FBlueprintActionMenuItem::StaticGetTypeId())
		{
			const UBlueprintFunctionNodeSpawner* FunctionNodeSpawner = Cast<UBlueprintFunctionNodeSpawner>(static_cast<FBlueprintActionMenuItem*>(Action.Get())->GetRawAction());
			const UFunction* FunctionToCall = FunctionNodeSpawner ? FunctionNodeSpawner->GetFunction() : nullptr;

			if (FunctionToCall)
			{
				UClass* OuterClass = CastChecked<UClass>(FunctionToCall->GetOuter());
				if (OuterClass->ClassGeneratedBy == Blueprint && Blueprint->SkeletonGeneratedClass)
				{
					// Attempt to locate a custom event or a function graph of this name on the blueprint
					for (UEdGraph* Graph : Blueprint->UbergraphPages)
					{
						for (UEdGraphNode* Node : Graph->Nodes)
						{
							UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
							if (CustomEvent && Blueprint->SkeletonGeneratedClass->FindFunctionByName(CustomEvent->GetFunctionName()) == FunctionToCall)
							{
								// Use this custom event
								UEdGraphPin* BoundObjectPin = FMovieSceneEventUtils::FindBoundObjectPin(CustomEvent, Params.BoundObjectPinClass);
								this->SetEventEndpoint(CustomEvent, BoundObjectPin, CustomEvent, EAutoCreatePayload::Variables);
								return;
							}
						}
					}

					for (UEdGraph* Graph : Blueprint->FunctionGraphs)
					{
						if (Blueprint->SkeletonGeneratedClass->FindFunctionByName(Graph->GetFName()) != FunctionToCall)
						{
							continue;
						}

						// Use this function graph for the event endpoint
						for (UEdGraphNode* Node : Graph->Nodes)
						{
							UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node);
							if (FunctionEntry)
							{
								UEdGraphPin* BoundObjectPin = FMovieSceneEventUtils::FindBoundObjectPin(FunctionEntry, Params.BoundObjectPinClass);
								this->SetEventEndpoint(FunctionEntry, BoundObjectPin, FunctionEntry, EAutoCreatePayload::Variables);
								return;
							}
						}
					}
				}
			}
		}

		if (Action)
		{
			UK2Node_CustomEvent* NewEventEndpoint = FMovieSceneEventUtils::CreateUserFacingEvent(Blueprint, Params);

			UEdGraphPin* ThenPin = NewEventEndpoint->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
			UEdGraphPin* BoundObjectPin = FMovieSceneEventUtils::FindBoundObjectPin(NewEventEndpoint, Params.BoundObjectPinClass);

			FVector2D NodePosition(NewEventEndpoint->NodePosX + 400.f, NewEventEndpoint->NodePosY);
			UEdGraphNode* NewNode = Action->PerformAction(NewEventEndpoint->GetGraph(), BoundObjectPin ? BoundObjectPin : ThenPin, NodePosition);

			if (NewNode)
			{
				UEdGraphPin* ExecPin = NewNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
				if (ensure(ThenPin && ExecPin))
				{
					ThenPin->MakeLinkTo(ExecPin);
				}
			}

			this->SetEventEndpoint(NewEventEndpoint, BoundObjectPin, Cast<UK2Node>(NewNode), EAutoCreatePayload::Pins | EAutoCreatePayload::Variables);
		}
	}
}

void FMovieSceneEventCustomization::PopulateRebindSubMenu(FMenuBuilder& MenuBuilder, UMovieSceneSequence* Sequence)
{
	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);
	if (!SequenceEditor)
	{
		return;
	}

	UBlueprint* Blueprint = SequenceEditor->GetOrCreateDirectorBlueprint(Sequence);
	if (!Blueprint)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();

	TOptional<FGuid> CommonObjectBindingID;
	{
		TArray<UObject*> EditObjects;
		GetEditObjects(EditObjects);

		for (UObject* Outer : EditObjects)
		{
			FGuid ThisBindingID;
			MovieScene->FindTrackBinding(*Outer->GetTypedOuter<UMovieSceneTrack>(), ThisBindingID);

			if (CommonObjectBindingID.IsSet() && CommonObjectBindingID != ThisBindingID)
			{
				CommonObjectBindingID.Reset();
				break;
			}

			CommonObjectBindingID = ThisBindingID;
		}
	}

	FMovieSceneEventEndpointParameters EventParams = FMovieSceneEventEndpointParameters::Generate(MovieScene, CommonObjectBindingID.Get(FGuid()));

	TSharedRef<SGraphActionMenu> ActionMenu = SNew(SGraphActionMenu)
		.OnCreateCustomRowExpander_Static([](const FCustomExpanderData& Data) -> TSharedRef<SExpanderArrow> { return SNew(SExpanderArrow, Data.TableRow); })
		.OnCollectAllActions_Static(UE_MovieSceneEventCustomization::CollectAllRebindActions, Blueprint)
		.OnActionSelected(this, &FMovieSceneEventCustomization::HandleRebindActionSelected, Blueprint, EventParams.BoundObjectPinClass);

	ActionMenu->RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda(
		[FilterTextBox = ActionMenu->GetFilterTextBox()](double, float)
		{
			FSlateApplication::Get().SetKeyboardFocus(FilterTextBox);
			return EActiveTimerReturnType::Stop;
		}
	));

	MenuBuilder.AddWidget(
		SNew(SBox)
		.WidthOverride(300.f)
		.MaxDesiredHeight(500.f)
		[
			ActionMenu
		],
		FText()
	);
}

void FMovieSceneEventCustomization::HandleRebindActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedAction, ESelectInfo::Type InSelectionType, UBlueprint* Blueprint, UClass* BoundObjectPinClass)
{
	if (InSelectionType != ESelectInfo::OnMouseClick && InSelectionType != ESelectInfo::OnKeyPress)
	{
		return;
	}

	if (!Blueprint->SkeletonGeneratedClass)
	{
		return;
	}

	for (TSharedPtr<FEdGraphSchemaAction> Action : SelectedAction)
	{
		if (Action->GetTypeId() != FBlueprintActionMenuItem::StaticGetTypeId())
		{
			continue;
		}

		const UBlueprintFunctionNodeSpawner* FunctionNodeSpawner = Cast<UBlueprintFunctionNodeSpawner>(static_cast<FBlueprintActionMenuItem*>(Action.Get())->GetRawAction());
		const UFunction* FunctionToCall = FunctionNodeSpawner ? FunctionNodeSpawner->GetFunction() : nullptr;
		if (!FunctionToCall)
		{
			continue;
		}

		UClass* OuterClass = CastChecked<UClass>(FunctionToCall->GetOuter());
		if (OuterClass->ClassGeneratedBy != Blueprint)
		{
			continue;
		}

		// Attempt to locate a custom event or a function graph of this name on the blueprint
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
				if (CustomEvent && Blueprint->SkeletonGeneratedClass->FindFunctionByName(CustomEvent->GetFunctionName()) == FunctionToCall)
				{
					// Use this custom event
					UEdGraphPin* BoundObjectPin = FMovieSceneEventUtils::FindBoundObjectPin(CustomEvent, BoundObjectPinClass);
					this->SetEventEndpoint(CustomEvent, BoundObjectPin, CustomEvent, EAutoCreatePayload::Variables);
					return;
				}
			}
		}

		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Blueprint->SkeletonGeneratedClass->FindFunctionByName(Graph->GetFName()) != FunctionToCall)
			{
				continue;
			}

			// Use this function graph for the event endpoint
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node);
				if (FunctionEntry)
				{
					UEdGraphPin* BoundObjectPin = FMovieSceneEventUtils::FindBoundObjectPin(FunctionEntry, BoundObjectPinClass);
					this->SetEventEndpoint(FunctionEntry, BoundObjectPin, FunctionEntry, EAutoCreatePayload::Variables);
					return;
				}
			}
		}
	}

	ensureMsgf(false, TEXT("Unknown blueprint action type encountered for rebinding"));
}

const FSlateBrush* FMovieSceneEventCustomization::GetEventIcon() const
{
	UK2Node* CommonEndpoint = GetCommonEndpoint();

	if (CommonEndpoint)
	{
		FLinearColor Color;
		FSlateIcon EndpointIcon = CommonEndpoint->GetIconAndTint(Color);
		return EndpointIcon.GetIcon();
	}
	else
	{
		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);
		if (RawData.Num() > 1)
		{
			return FAppStyle::GetBrush("Sequencer.MultipleEvents");
		}
	}

	return FAppStyle::GetBrush("Sequencer.UnboundEvent");
}

FText FMovieSceneEventCustomization::GetEventName() const
{
	UEdGraphNode* CommonEndpoint = GetCommonEndpoint();

	if (CommonEndpoint)
	{
		return CommonEndpoint->GetNodeTitle(ENodeTitleType::MenuTitle);
	}
	else
	{
		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);
		if (RawData.Num() != 1)
		{
			return LOCTEXT("MultipleValuesText", "Multiple Values");
		}
	}

	return LOCTEXT("UnboundText", "Unbound");
}

void FMovieSceneEventCustomization::SetEventEndpoint(UK2Node* NewEndpoint, UEdGraphPin* BoundObjectPin, UK2Node* PayloadTemplate, EAutoCreatePayload AutoCreatePayload)
{
	FScopedTransaction Transaction(LOCTEXT("SetEventEndpoint", "Set Event Endpoint"));

	// Modify and assign the blueprint for outer sections
	TArray<UObject*> EditObjects;
	GetEditObjects(EditObjects);

	TArray<UBlueprint*> AllBlueprints;

	// If we're assigning a new valid endpoint, it must reside within the same blueprint as everything we're assigning it to.
	// Anything else must be implemented as a call function node connected to a custom event node
	UBlueprint* Blueprint = NewEndpoint ? NewEndpoint->GetBlueprint() : nullptr;
	if (Blueprint)
	{
		for (UObject* Outer : EditObjects)
		{
			UMovieSceneSequence*       Sequence           = Outer->GetTypedOuter<UMovieSceneSequence>();
			FMovieSceneSequenceEditor* SequenceEditor     = FMovieSceneSequenceEditor::Find(Sequence);
			UBlueprint*                SequenceDirectorBP = SequenceEditor ? SequenceEditor->GetOrCreateDirectorBlueprint(Sequence) : nullptr;

			if (!ensureAlwaysMsgf(SequenceDirectorBP == Blueprint, TEXT("Attempting to assign an event endpoint to an event with a different sequence director Blueprint.")))
			{
				Transaction.Cancel();
				return;
			}
		}

		Blueprint->Modify();
	}

	for (UObject* Outer : EditObjects)
	{
		UMovieSceneEventSectionBase* BaseEventSection = Cast<UMovieSceneEventSectionBase>(Outer);
		if (BaseEventSection)
		{
			BaseEventSection->Modify();
			if (Blueprint)
			{
				FMovieSceneEventUtils::BindEventSectionToBlueprint(BaseEventSection, Blueprint);
			}
		}
	}

	TArray<FName> PayloadNames;
	if (PayloadTemplate && EnumHasAnyFlags(AutoCreatePayload, EAutoCreatePayload::Variables | EAutoCreatePayload::Pins))
	{
		UK2Node_EditablePinBase* EditableNode = Cast<UK2Node_EditablePinBase>(NewEndpoint);

		for (UEdGraphPin* PayloadPin : PayloadTemplate->Pins)
		{
			if (PayloadPin != BoundObjectPin && PayloadPin->Direction == EGPD_Input && PayloadPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && PayloadPin->LinkedTo.Num() == 0 && PayloadPin->PinName != UEdGraphSchema_K2::PN_Self)
			{
				// Make a payload variable for this pin
				if (EnumHasAnyFlags(AutoCreatePayload, EAutoCreatePayload::Variables))
				{
					PayloadNames.Add(PayloadPin->PinName);
				}

				if (EditableNode && EnumHasAnyFlags(AutoCreatePayload, EAutoCreatePayload::Pins))
				{
					UEdGraphPin* NewPin = EditableNode->CreateUserDefinedPin(PayloadPin->PinName, PayloadPin->PinType, EGPD_Output);
					if (PayloadTemplate != NewEndpoint && NewPin)
					{
						NewPin->MakeLinkTo(PayloadPin);
					}
				}
			}
		}
	}

	// Assign the endpoints to all events
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);

	check(EditObjects.Num() == RawData.Num());

	for (int32 Index = 0; Index < RawData.Num(); ++Index)
	{
		UMovieSceneEventSectionBase* EventSection = Cast<UMovieSceneEventSectionBase>(EditObjects[Index]);
		FMovieSceneEvent*  EntryPoint   = static_cast<FMovieSceneEvent*>(RawData[Index]);

		if (EntryPoint)
		{
			FMovieSceneEventUtils::SetEndpoint(EntryPoint, EventSection, NewEndpoint, BoundObjectPin);

			for (FName PayloadVar : PayloadNames)
			{
				if (!EntryPoint->PayloadVariables.Contains(PayloadVar))
				{
					EntryPoint->PayloadVariables.Add(PayloadVar);
				}
			}
		}
	}

	// Ensure that anything listening for property changed notifications are notified of the new binding
	PropertyHandle->NotifyFinishedChangingProperties();

	// Compile the blueprint now that clients have had a chance to update underlying data (we do this after to ensure we are compiling the correct data
	if (Blueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}

	// Forcibly update the panel now that our endpoint has changed
	PropertyUtilities->ForceRefresh();

	if (NewEndpoint)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NewEndpoint, false);
	}
}

void FMovieSceneEventCustomization::CreateEventEndpoint()
{
	struct FSectionData
	{
		TArray<FMovieSceneEvent*, TInlineAllocator<1>>  EntryPoints;
	};
	struct FTrackData
	{
		TSortedMap<UMovieSceneEventSectionBase*, FSectionData>  Sections;
	};
	struct FSequenceData
	{
		TSortedMap<UMovieSceneEventTrack*, FTrackData> Tracks;
	};

	TSortedMap<UMovieSceneSequence*, FSequenceData> PerSequenceData;

	// Populate all the sequences represented by this customization
	{
		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);

		TArray<UObject*> EditObjects;
		GetEditObjects(EditObjects);

		check(RawData.Num() == EditObjects.Num());

		for (int32 Index = 0; Index < RawData.Num(); ++Index)
		{
			UMovieSceneEventSectionBase* EventSection = Cast<UMovieSceneEventSectionBase>(EditObjects[Index]);
			FMovieSceneEvent*  EntryPoint   = static_cast<FMovieSceneEvent*>(RawData[Index]);

			if (EventSection)
			{
				FSequenceData& SequenceData = PerSequenceData.FindOrAdd(EventSection->GetTypedOuter<UMovieSceneSequence>());
				FTrackData&    TrackData    = SequenceData.Tracks.FindOrAdd(EventSection->GetTypedOuter<UMovieSceneEventTrack>());
				FSectionData&  SectionData  = TrackData.Sections.FindOrAdd(EventSection);

				SectionData.EntryPoints.Add(EntryPoint);
			}
		}
	}

	UK2Node_CustomEvent* NewEventEndpoint = nullptr;

	FScopedTransaction Transaction(LOCTEXT("CreateEventEndpoint", "Create Event Endpoint"));

	for (const TPair<UMovieSceneSequence*, FSequenceData>& SequencePair : PerSequenceData)
	{
		FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(SequencePair.Key);
		if (!SequenceEditor)
		{
			continue;
		}

		UBlueprint* SequenceDirectorBP = SequenceEditor->GetOrCreateDirectorBlueprint(SequencePair.Key);
		if (!SequenceDirectorBP)
		{
			continue;
		}

		UMovieScene* MovieScene = SequencePair.Key->GetMovieScene();

		TOptional<FGuid> CommonObjectBindingID;
		for (const TPair<UMovieSceneEventTrack*, FTrackData>& TrackPair : SequencePair.Value.Tracks)
		{
			FGuid ThisBindingID;
			MovieScene->FindTrackBinding(*TrackPair.Key, ThisBindingID);

			if (CommonObjectBindingID.IsSet() && CommonObjectBindingID != ThisBindingID)
			{
				CommonObjectBindingID.Reset();
				break;
			}

			CommonObjectBindingID = ThisBindingID;
		}

		FMovieSceneEventEndpointParameters Parameters = FMovieSceneEventEndpointParameters::Generate(MovieScene, CommonObjectBindingID.Get(FGuid()));

		SequenceDirectorBP->Modify();

		NewEventEndpoint = FMovieSceneEventUtils::CreateUserFacingEvent(SequenceDirectorBP, Parameters);
		if (!NewEventEndpoint)
		{
			continue;
		}

		UEdGraphPin* BoundObjectPin = FMovieSceneEventUtils::FindBoundObjectPin(NewEventEndpoint, Parameters.BoundObjectPinClass);
		for (const TPair<UMovieSceneEventTrack*, FTrackData>& TrackPair : SequencePair.Value.Tracks)
		{
			for (const TPair<UMovieSceneEventSectionBase*, FSectionData>& SectionPair : TrackPair.Value.Sections)
			{
				SectionPair.Key->Modify();
				FMovieSceneEventUtils::BindEventSectionToBlueprint(SectionPair.Key, SequenceDirectorBP);

				for (FMovieSceneEvent* EntryPoint : SectionPair.Value.EntryPoints)
				{
					FMovieSceneEventUtils::SetEndpoint(EntryPoint, SectionPair.Key, NewEventEndpoint, BoundObjectPin);
				}
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsModified(SequenceDirectorBP);
	}

	// Ensure that anything listening for property changed notifications are notified of the new binding
	PropertyHandle->NotifyFinishedChangingProperties();
	PropertyUtilities->ForceRefresh();

	if (NewEventEndpoint)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NewEventEndpoint, false);
	}
}

void FMovieSceneEventCustomization::ClearEventEndpoint()
{
	SetEventEndpoint(nullptr, nullptr, nullptr, EAutoCreatePayload::None);
}

void FMovieSceneEventCustomization::NavigateToDefinition()
{
	UEdGraphNode* CommonEndpoint = GetCommonEndpoint();
	if (CommonEndpoint)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(CommonEndpoint, false);
	}
}

#undef LOCTEXT_NAMESPACE
