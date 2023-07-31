// Copyright Epic Games, Inc. All Rights Reserved.

#include "K2Node_MakeRequestHeader.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphUtilities.h"
#include "HttpBlueprintFunctionLibrary.h"
#include "HttpBlueprintGraphLog.h"
#include "HttpBlueprintTypes.h"
#include "HttpHeader.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MakeMap.h"
#include "KismetCompiler.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "Framework/Commands/UIAction.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "K2Node_MakeRequestHeader"

namespace UE::HttpBlueprint::Private
{
	namespace MakeRequestHeaderGlobals
	{
		static const FName OutputPinName(TEXT("Header"));
		static const FName PresetPinName(TEXT("Preset"));
		
		static TArray<TPair<FString, FString>> Presets;
	}
	
	// This exists so we can just store an array of the presets instead of a map.
	// This allows us to avoid looping and comparing Enum values.
	// Instead we can just use random access since the index is known
	namespace FPresetPair
	{
		namespace Private
		{
			static FString GetPresetValueFromEnum(const ERequestPresets& InPresetEnum)
			{
				switch (InPresetEnum)
				{
					case ERequestPresets::Json:		return TEXT("application/json");
					case ERequestPresets::Http:		return TEXT("text/html");
					case ERequestPresets::Url:		return TEXT("application/x-www-form-urlencoded");
					case ERequestPresets::Custom:	return TEXT("");
					default:						return TEXT("");
				}
			}
		}

		inline namespace Public
		{
			static FString PresetArrayToString()
			{
				FString OutString = TEXT("Preset String");
				for (int32 Index{0}; Index < MakeRequestHeaderGlobals::Presets.Num(); Index++)
				{
					OutString = FString::Printf(TEXT("%s%s: %s"), LINE_TERMINATOR,
					                            *MakeRequestHeaderGlobals::Presets[Index].Key,
					                            *MakeRequestHeaderGlobals::Presets[Index].Value);
				}

				return OutString;
			}
			
			static FString GetHeaderValueForIndex(const int32& InIndex, const int32& InEnumIndex)
			{
				if (!MakeRequestHeaderGlobals::Presets.IsValidIndex(InIndex))
				{
					FFrame::KismetExecutionMessage(*FString::Printf(
						TEXT("Preset array was not properly initialized. Can not make a request header. Index: %i -- Presets: %s"), InIndex, *PresetArrayToString()),
						ELogVerbosity::Error);
					return {};
				}

				const FString& HeaderValue = MakeRequestHeaderGlobals::Presets[InIndex].Value;
				return HeaderValue.IsEmpty() ? Private::GetPresetValueFromEnum(
						       StaticCast<ERequestPresets>(InEnumIndex)) : HeaderValue;
			}

			static FString GetHeaderKeyForIndex(const int32& InIndex)
			{
				if (!MakeRequestHeaderGlobals::Presets.IsValidIndex(InIndex))
				{
					FFrame::KismetExecutionMessage(*FString::Printf(
					TEXT("Preset array was not properly initialized. Can not make a request header. Index: %i -- Presets: %s"), InIndex, *PresetArrayToString()),
						ELogVerbosity::Error);
					return {};
				}
				
				return MakeRequestHeaderGlobals::Presets[InIndex].Key;
			}

			static int32 NumPresetHeaders()
			{
				return MakeRequestHeaderGlobals::Presets.Num();
			}
		}
	};
}

UK2Node_MakeRequestHeader::UK2Node_MakeRequestHeader(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumInputs = 1;
	PresetEnum = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/HttpBlueprint.ERequestPresets"));

	UE::HttpBlueprint::Private::MakeRequestHeaderGlobals::Presets =
	{
		{ TTuple<FString, FString>("Content-Type", "") },
		{ TTuple<FString, FString>("Accepts", "") },
		{ TTuple<FString, FString>("User-Agent", "X-UnrealEngine-Agent") },
		{ TTuple<FString, FString>("Accept-Encoding", "identity") },
	};
}

void UK2Node_MakeRequestHeader::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();
	
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, FHttpHeader::StaticStruct(), GetOutputPinName());

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* PresetPin = CreatePin(EGPD_Input,
		UEdGraphSchema_K2::PC_Byte,
		PresetEnum.Get(),
		UE::HttpBlueprint::Private::MakeRequestHeaderGlobals::PresetPinName);
	Schema->SetPinDefaultValueAtConstruction(PresetPin, PresetEnum->GetNameStringByIndex(PresetEnumIndex));

	ConstructDefaultPinsForPreset();
}

FText UK2Node_MakeRequestHeader::GetTooltipText() const
{
	return LOCTEXT("MakeRequestHeader_Tooltip", "Creates a Header object that can be used to send Http Requests");
}

FText UK2Node_MakeRequestHeader::GetNodeTitle(ENodeTitleType::Type Title) const
{
	return LOCTEXT("MakeRequestHeader_Title", "Make Request Header");
}

void UK2Node_MakeRequestHeader::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);
}

void UK2Node_MakeRequestHeader::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin->Direction == EGPD_Input
		&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		ConstructDefaultPinsForPreset();
	}
}

void UK2Node_MakeRequestHeader::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);

	if (!Context->bIsDebugging)
	{
		FToolMenuSection& Section = Menu->AddSection("K2Node_MakeRequestHeader", LOCTEXT("MakeRequestHeader_ContextMenu", "Make Request Header"));
		
		if (Context->Pin)
		{
			if (Context->Pin->Direction == EGPD_Input && !Context->Pin->ParentPin)
			{
				Section.AddMenuEntry("RemovePin", LOCTEXT("MakeRequestHeader_RemovePin", "Remove header pin"),
				                     LOCTEXT("MakeRequestHeader_RemovePin_Tooltip", "Remove this header element pin"),
				                     FSlateIcon(),
				                     FUIAction(FExecuteAction::CreateUObject(
					                     const_cast<UK2Node_MakeRequestHeader*>(this),
					                     &UK2Node_MakeRequestHeader::RemoveInputPin,
					                     const_cast<UEdGraphPin*>(Context->Pin))));
			}
		}
		else
		{
			Section.AddMenuEntry("AddPin", LOCTEXT("MakeRequestHeader_AddPin", "Add header pin"),
			                     LOCTEXT("MakeRequestHeader_AddPin_Tooltip", "Add another header pin"),
			                     FSlateIcon(),
			                     FUIAction(FExecuteAction::CreateUObject(
				                     const_cast<UK2Node_MakeRequestHeader*>(this),
				                     &UK2Node_MakeRequestHeader::AddInputPin)));
		}
	}
}

void UK2Node_MakeRequestHeader::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	const FName& FunctionName = GET_FUNCTION_NAME_CHECKED(UHttpBlueprintFunctionLibrary, MakeRequestHeader);
	UClass* FunctionClass = UHttpBlueprintFunctionLibrary::StaticClass();

	UK2Node_CallFunction* CallFunctionNode = SourceGraph->CreateIntermediateNode<UK2Node_CallFunction>();
	CallFunctionNode->FunctionReference.SetExternalMember(FunctionName, FunctionClass);
	CallFunctionNode->AllocateDefaultPins();
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(CallFunctionNode, this);

	UK2Node_MakeMap* MakeMapNode = SourceGraph->CreateIntermediateNode<UK2Node_MakeMap>();
	MakeMapNode->AllocateDefaultPins();
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(MakeMapNode, this);

	UEdGraphPin* MapOutPin = MakeMapNode->GetOutputPin();

	UEdGraphPin* CallFunctionInputPin = CallFunctionNode->FindPinChecked(TEXT("Headers"));
	MapOutPin->MakeLinkTo(CallFunctionInputPin);
	MakeMapNode->PinConnectionListChanged(MapOutPin);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (PresetEnumIndex == StaticCast<int32>(ERequestPresets::Custom))
	{
		// Add input pins to the MakeMap node
		// UK2Node_MakeMap::AddInputPin adds two pins per call which is why we only need to call it for Pins.Num / 2
		for (int32 Index{0}; Index < (Pins.Num() / 2); Index++)
		{
			if (IsValidInputPin(Pins[Index]))
			{
				MakeMapNode->AddInputPin();
			}
		}
	
		// Skip the first index because it's the output pin
		for (int32 Index{1}; Index < Pins.Num(); ++Index)
		{
			if (IsValidInputPin(Pins[Index]))
			{
				UEdGraphPin* MapPin = MakeMapNode->FindPinChecked(MakeMapNode->GetPinName(Index - 2));
				MapPin->PinType = Pins[Index]->PinType;
				CompilerContext.MovePinLinksToIntermediate(*Pins[Index], *MapPin);
			}
		}
	}
	else
	{
		if (PresetEnumIndex != StaticCast<int32>(ERequestPresets::Custom))
		{
			// MakeMapNode spawns one set of pins by default. So we only need to Spawn NumPins - 1
			for (int32 Index{0}; Index < UE::HttpBlueprint::Private::FPresetPair::NumPresetHeaders() - 1; Index++)
			{
				MakeMapNode->AddInputPin();
			}
			
			TArray<UEdGraphPin*> KeyPins, ValuePins;
			MakeMapNode->GetKeyAndValuePins(KeyPins, ValuePins);
			for (int32 Index{0}; Index < KeyPins.Num(); Index++)
			{
				UEdGraphPin* KeyPin = KeyPins[Index];
				UEdGraphPin* ValuePin = ValuePins[Index];

				Schema->TrySetDefaultValue(*KeyPin, UE::HttpBlueprint::Private::FPresetPair::GetHeaderKeyForIndex(Index));
				Schema->TrySetDefaultValue(*ValuePin, UE::HttpBlueprint::Private::FPresetPair::GetHeaderValueForIndex(Index, PresetEnumIndex));
			}
		}
	}

	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(GetOutputPinName()),
	                                           *CallFunctionNode->FindPinChecked(TEXT("OutHeader")));

	BreakAllNodeLinks();
}

void UK2Node_MakeRequestHeader::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	if (const UClass* ActionKey = GetClass();
		ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		checkf(NodeSpawner != nullptr, TEXT("Node spawner failed to create a valid Node"));

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_MakeRequestHeader::GetMenuCategory() const
{
	return LOCTEXT("MakeRequestHeader_Category", "Http");
}

bool UK2Node_MakeRequestHeader::NodeCausesStructuralBlueprintChange() const
{
	return true;
}

bool UK2Node_MakeRequestHeader::IsNodePure() const
{
	return true;
}

FName UK2Node_MakeRequestHeader::GetOutputPinName()
{
	return UE::HttpBlueprint::Private::MakeRequestHeaderGlobals::OutputPinName;
}

UEdGraphPin* UK2Node_MakeRequestHeader::GetOutputPin() const
{
	return FindPinChecked(GetOutputPinName());
}

bool UK2Node_MakeRequestHeader::IsValidInputPin(const UEdGraphPin* InputPin) const
{
	return InputPin->Direction == EGPD_Input
		&& InputPin->PinName != UE::HttpBlueprint::Private::MakeRequestHeaderGlobals::PresetPinName
		&& InputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_String;
}

void UK2Node_MakeRequestHeader::ConstructDefaultPinsForPreset()
{
	const UEdGraphPin* PresetPin = FindPinChecked(UE::HttpBlueprint::Private::MakeRequestHeaderGlobals::PresetPinName);
	if (const int32 EnumIndex = PresetEnum->GetIndexByName(*PresetPin->DefaultValue);
		EnumIndex != INDEX_NONE)
	{
		PresetEnumIndex = EnumIndex;
		if (PresetEnumIndex == StaticCast<int32>(ERequestPresets::Custom))
		{
			if (OptionalPins.Num() > 0)
			{
				const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
				for (const FOptionalPin& OptionalPin : OptionalPins)
				{
					UEdGraphPin* NewPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, OptionalPin.PinName);
					Schema->SetPinDefaultValueAtConstruction(NewPin, OptionalPin.PinDefaultValue);
				}
				NumInputs = Pins.Num() - 2;
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
			}
		}
		else
		{
			// Remove all pins except for the preset and output pin
			// Store all custom pins for reconstruction later
			OptionalPins.Empty(Pins.Num() - 2);
			for (int32 Index{2}; Index < Pins.Num(); ++Index)
			{
				UEdGraphPin* PinToRemove = Pins.IsValidIndex(Index) ? Pins[Index] : nullptr;
				if (PinToRemove == nullptr)
				{
					UE_LOG(LogHttpBlueprintEditor, Warning, TEXT("Pin was invalid"));
					break;
				}
				
				FScopedTransaction Transaction(LOCTEXT("RemovePinTx", "RemovePin"));
				Modify();

				FOptionalPin NewOptionalPin;
				NewOptionalPin.PinName = PinToRemove->PinName;
				NewOptionalPin.PinDefaultValue = PinToRemove->DefaultValue;
				OptionalPins.Add(NewOptionalPin);
				
				TFunction<void(UEdGraphPin*)> RemovePinLambda = [this, &RemovePinLambda](UEdGraphPin* PinToRemove)->void
				{
					check(PinToRemove);
					for (int32 SubPinIndex = PinToRemove->SubPins.Num() - 1; SubPinIndex >= 0; --SubPinIndex)
					{
						RemovePinLambda(PinToRemove->SubPins[SubPinIndex]);
					}

					int32 PinRemovalIndex = INDEX_NONE;
					if (Pins.Find(PinToRemove, PinRemovalIndex))
					{
						Pins.RemoveAt(PinRemovalIndex);
						PinToRemove->MarkAsGarbage();
					}
				};
				
				RemovePinLambda(PinToRemove);
				PinConnectionListChanged(PinToRemove);
				--Index;
			}

			NumInputs = 0;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
		}
	}
}

bool UK2Node_MakeRequestHeader::CanAddPin() const
{
	return StaticCast<ERequestPresets>(PresetEnumIndex) == ERequestPresets::Custom;
}

bool UK2Node_MakeRequestHeader::CanRemovePin(const UEdGraphPin* Pin) const
{
	return StaticCast<ERequestPresets>(PresetEnumIndex) == ERequestPresets::Custom && Pin->Direction == EGPD_Input &&
		Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String;
}

void UK2Node_MakeRequestHeader::AddInputPin()
{
	FScopedTransaction Transaction(LOCTEXT("AddPinTx", "Add Pin"));
	Modify();

	++NumInputs;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String,
	          *FString::Printf(TEXT("%s"), *GetPinName((NumInputs - 1) * 2).ToString()));

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String,
	          *FString::Printf(TEXT("%s"), *GetPinName((NumInputs - 1) * 2 + 1).ToString()));
	
	if (!GetBlueprint()->bBeingCompiled)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
		GetGraph()->NotifyGraphChanged();
	}
}

void UK2Node_MakeRequestHeader::RemoveInputPin(UEdGraphPin* Pin)
{
	check(Pin);
	check(Pin->Direction == EGPD_Input);
	check(Pin->ParentPin == nullptr);
	check(Pins.Contains(Pin));

	FScopedTransaction Transaction(LOCTEXT("RemovePinTx", "RemovePin"));
	Modify();

	TFunction<void(UEdGraphPin*)> RemovePinLambda = [this, &RemovePinLambda](UEdGraphPin* PinToRemove)->void
	{
		check(PinToRemove);
		for (int32 SubPinIndex = PinToRemove->SubPins.Num() - 1; SubPinIndex >= 0; --SubPinIndex)
		{
			RemovePinLambda(PinToRemove->SubPins[SubPinIndex]);
		}

		int32 PinRemovalIndex = INDEX_NONE;
		if (Pins.Find(PinToRemove, PinRemovalIndex))
		{
			Pins.RemoveAt(PinRemovalIndex);
			PinToRemove->MarkAsGarbage();
		}
	};

	RemovePinLambda(Pin);
	PinConnectionListChanged(Pin);

	--NumInputs;
	SyncPinNames();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

void UK2Node_MakeRequestHeader::SyncPinNames()
{
	int32 CurrentNumParentPins = 0;
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{		
		UEdGraphPin*& CurrentPin = Pins[PinIndex];
		if (CurrentPin->Direction == EGPD_Input &&
			CurrentPin->ParentPin == nullptr	&&
			IsValidInputPin(Pins[PinIndex]))
		{
			const FName OldName = CurrentPin->PinName;
			const FName ElementName = GetPinName(CurrentNumParentPins++);

			CurrentPin->Modify();
			CurrentPin->PinName = ElementName;

			if (CurrentPin->SubPins.Num() > 0)
			{
				const FString OldNameStr = OldName.ToString();
				const FString ElementNameStr = ElementName.ToString();
				FString OldFriendlyName = OldNameStr;
				FString ElementFriendlyName = ElementNameStr;

				OldFriendlyName.InsertAt(1, " ");
				ElementFriendlyName.InsertAt(1, " ");

				for (UEdGraphPin* SubPin : CurrentPin->SubPins)
				{
					FString SubPinFriendlyName = SubPin->PinFriendlyName.ToString();
					SubPinFriendlyName.ReplaceInline(*OldFriendlyName, *ElementFriendlyName);

					SubPin->Modify();
					SubPin->PinName = *SubPin->PinName.ToString().Replace(*OldNameStr, *ElementNameStr);
					SubPin->PinFriendlyName = FText::FromString(SubPinFriendlyName);
				}
			}
		}
	}
}

FName UK2Node_MakeRequestHeader::GetPinName(const int32& PinIndex)
{
	const int32 PairIndex = PinIndex / 2;
	if (PinIndex % 2 == 0)
	{
		return *FString::Printf(TEXT("Header %d"), PairIndex);
	}

	return *FString::Printf(TEXT("Value %d"), PairIndex);
}

#undef LOCTEXT_NAMESPACE
