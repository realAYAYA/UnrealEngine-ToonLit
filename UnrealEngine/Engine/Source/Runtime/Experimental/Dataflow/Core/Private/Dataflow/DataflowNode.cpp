// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNode.h"

#include "ChaosLog.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowArchive.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowNode)

const FName FDataflowNode::DataflowInput = TEXT("DataflowInput");
const FName FDataflowNode::DataflowOutput = TEXT("DataflowOutput");
const FName FDataflowNode::DataflowPassthrough = TEXT("DataflowPassthrough");
const FName FDataflowNode::DataflowIntrinsic = TEXT("DataflowIntrinsic");

const FLinearColor FDataflowNode::DefaultNodeTitleColor = FLinearColor(1.f, 1.f, 0.8f);
const FLinearColor FDataflowNode::DefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

//
// Inputs
//

void FDataflowNode::AddInput(FDataflowInput* InPtr)
{
	if (InPtr)
	{
		for (TPair<uint32, FDataflowInput*> Elem : Inputs)
		{
			FDataflowInput* In = Elem.Value;
			ensureMsgf(!In->GetName().IsEqual(InPtr->GetName()), TEXT("Add Input Failed: Existing Node input already defined with name (%s)"), *InPtr->GetName().ToString());
		}

		const uint32 PropertyOffset = InPtr->GetOffset();
		if (!Inputs.Contains(PropertyOffset))
		{
			Inputs.Add(PropertyOffset, InPtr);
		}
		else
		{
			Inputs[PropertyOffset] = InPtr;
		}
	}
}

FDataflowInput* FDataflowNode::FindInput(FName InName)
{
	for (TPair<uint32, FDataflowInput*> Elem : Inputs)
	{
		FDataflowInput* Con = Elem.Value;
		if (Con->GetName().IsEqual(InName))
		{
			return Con;
		}
	}
	return nullptr;
}


const FDataflowInput* FDataflowNode::FindInput(const void* Reference) const
{
	for (TPair<uint32, FDataflowInput*> Elem : Inputs)
	{
		FDataflowInput* Con = Elem.Value;
		if (Con->RealAddress() == Reference)
		{
			return Con;
		}
	}
	return nullptr;
}

FDataflowInput* FDataflowNode::FindInput(void* Reference)
{
	for (TPair<uint32, FDataflowInput*> Elem : Inputs)
	{
		FDataflowInput* Con = Elem.Value;
		if (Con->RealAddress() == Reference)
		{
			return Con;
		}
	}
	return nullptr;
}

const FDataflowInput* FDataflowNode::FindInput(const FGuid& InGuid) const
{
	for (TPair<uint32, FDataflowInput*> Elem : Inputs)
	{
		FDataflowInput* Con = Elem.Value;
		if (Con->GetGuid() == InGuid)
		{
			return Con;
		}
	}
	return nullptr;
}

TArray< FDataflowInput* > FDataflowNode::GetInputs() const
{
	TArray< FDataflowInput* > Result;
	Inputs.GenerateValueArray(Result);
	return Result;
}

void FDataflowNode::ClearInputs()
{
	for (TPair<uint32, FDataflowInput*> Elem : Inputs)
	{
		FDataflowInput* Con = Elem.Value;
		delete Con;
	}
	Inputs.Reset();
}


//
// Outputs
//


void FDataflowNode::AddOutput(FDataflowOutput* InPtr)
{
	if (InPtr)
	{
		for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
		{
			FDataflowOutput* Out = Elem.Value;
			ensureMsgf(!Out->GetName().IsEqual(InPtr->GetName()), TEXT("Add Output Failed: Existing Node output already defined with name (%s)"), *InPtr->GetName().ToString());
		}

		const uint32 PropertyOffset = InPtr->GetOffset();
		if (!Outputs.Contains(PropertyOffset))
		{
			Outputs.Add(PropertyOffset, InPtr);
		}
		else
		{
			Outputs[PropertyOffset] = InPtr;
		}
	}
}

FDataflowOutput* FDataflowNode::FindOutput(FName InName)
{
	for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
	{
		FDataflowOutput* Con = Elem.Value;
		if (Con->GetName().IsEqual(InName))
		{
			return (FDataflowOutput*)Con;
		}
	}
	return nullptr;
}

const FDataflowOutput* FDataflowNode::FindOutput(FName InName) const
{
	for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
	{
		FDataflowOutput* Con = Elem.Value;
		if (Con->GetName().IsEqual(InName))
		{
			return (FDataflowOutput*)Con;
		}
	}
	return nullptr;
}

const FDataflowOutput* FDataflowNode::FindOutput(const void* Reference) const
{
	for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
	{
		FDataflowOutput* Con = Elem.Value;
		if (Con->RealAddress() == Reference)
		{
			return (FDataflowOutput*)Con;
		}
	}
	return nullptr;
}

FDataflowOutput* FDataflowNode::FindOutput(void* Reference)
{
	for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
	{
		FDataflowOutput* Con = Elem.Value;
		if (Con->RealAddress() == Reference)
		{
			return (FDataflowOutput*)Con;
		}
	}
	return nullptr;
}

const FDataflowOutput* FDataflowNode::FindOutput(const FGuid& InGuid) const
{
	for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
	{
		FDataflowOutput* Con = Elem.Value;
		if (Con->GetGuid() == InGuid)
		{
			return Con;
		}
	}
	return nullptr;
}

int32 FDataflowNode::NumOutputs() const
{
	return Outputs.Num();
}


TArray< FDataflowOutput* > FDataflowNode::GetOutputs() const
{
	TArray< FDataflowOutput* > Result;
	Outputs.GenerateValueArray(Result);
	return Result;
}


void FDataflowNode::ClearOutputs()
{
	for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
	{
		FDataflowOutput* Con = Elem.Value;
		delete Con;
	}
	Outputs.Reset();
}


TArray<Dataflow::FPin> FDataflowNode::GetPins() const
{
	TArray<Dataflow::FPin> RetVal;
	for (TPair<uint32, FDataflowInput*> Elem : Inputs)
	{
		FDataflowInput* Con = Elem.Value;
		RetVal.Add({ Dataflow::FPin::EDirection::INPUT,Con->GetType(), Con->GetName() });
	}
	for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
	{
		FDataflowOutput* Con = Elem.Value;
		RetVal.Add({ Dataflow::FPin::EDirection::OUTPUT,Con->GetType(), Con->GetName() });
	}
	return RetVal;
}

void FDataflowNode::UnregisterPinConnection(const Dataflow::FPin& Pin)
{
	if (Pin.Direction == Dataflow::FPin::EDirection::INPUT)
	{
		for (TMap< int32, FDataflowInput*>::TIterator Iter = Inputs.CreateIterator(); Iter; ++Iter)
		{
			FDataflowInput* Con = Iter.Value();
			if (Con->GetName().IsEqual(Pin.Name) && Con->GetType().IsEqual(Pin.Type))
			{
				Iter.RemoveCurrent();
				delete Con;

				// Invalidate graph as this input might have had connections
				Invalidate();
				break;
			}
		}
	}
	else if (Pin.Direction == Dataflow::FPin::EDirection::OUTPUT)
	{
		for (TMap<int32, FDataflowOutput*>::TIterator Iter = Outputs.CreateIterator(); Iter; ++Iter)
		{
			FDataflowOutput* Con = Iter.Value();
			if (Con->GetName().IsEqual(Pin.Name) && Con->GetType().IsEqual(Pin.Type))
			{
				Iter.RemoveCurrent();
				delete Con;

				// Invalidate graph as this input might have had connections
				Invalidate();
				break;
			}
		}
	}
}

void FDataflowNode::Invalidate(const Dataflow::FTimestamp& InModifiedTimestamp)
{
	if (LastModifiedTimestamp < InModifiedTimestamp)
	{
		LastModifiedTimestamp = InModifiedTimestamp;

		for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
		{
			FDataflowOutput* Con = Elem.Value;
			Con->Invalidate(InModifiedTimestamp);
		}

		OnInvalidate();

		OnNodeInvalidatedDelegate.Broadcast(this);
	}
}

const FProperty* FDataflowNode::FindProperty(const UStruct* Struct, const void* InProperty, const FName& PropertyName, TArray<const FProperty*>* OutPropertyChain) const
{
	const FProperty* Property = nullptr;
	for (FPropertyValueIterator PropertyIt(FProperty::StaticClass(), Struct, this); PropertyIt; ++PropertyIt)
	{
		if (InProperty == PropertyIt.Value() && (PropertyName == NAME_None || PropertyName == PropertyIt.Key()->GetName()))
		{
			Property = PropertyIt.Key();
			if (OutPropertyChain)
			{
				PropertyIt.GetPropertyChain(*OutPropertyChain);
			}
			break;
		}
	}
	return Property;
}

const FProperty* FDataflowNode::FindProperty(const UStruct* Struct, const FName& PropertyFullName, TArray<const FProperty*>* OutPropertyChain) const
{
	const FProperty* Property = nullptr;
	for (FPropertyValueIterator PropertyIt(FProperty::StaticClass(), Struct, this); PropertyIt; ++PropertyIt)
	{
		TArray<const FProperty*> PropertyChain;
		PropertyIt.GetPropertyChain(PropertyChain);
		if (GetPropertyFullName(PropertyChain) == PropertyFullName)
		{
			Property = PropertyIt.Key();
			if (OutPropertyChain)
			{
				*OutPropertyChain = MoveTemp(PropertyChain);
			}
			break;
		}
	}
	return Property;
}

uint32 FDataflowNode::GetPropertyOffset(const TArray<const FProperty*>& PropertyChain)
{
	uint32 Offset = 0;
	for (const FProperty* const Property : PropertyChain)
	{
		Offset += (uint32)Property->GetOffset_ForInternal();
	}
	return Offset;
}

uint32 FDataflowNode::GetPropertyOffset(const FName& PropertyFullName) const
{
	uint32 Offset = 0;
	if (const TUniquePtr<const FStructOnScope> ScriptOnStruct =
		TUniquePtr<FStructOnScope>(const_cast<FDataflowNode*>(this)->NewStructOnScope()))  // The mutable Struct Memory is not accessed here, allowing for the const_cast and keeping this method const
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			TArray<const FProperty*> PropertyChain;
			FindProperty(Struct, PropertyFullName, &PropertyChain);
			Offset = GetPropertyOffset(PropertyChain);
		}
	}
	return Offset;
}

FString FDataflowNode::GetPropertyFullNameString(const TConstArrayView<const FProperty*>& PropertyChain)
{
	FString PropertyFullName;
	for (const FProperty* const Property : PropertyChain)
	{
		const FString PropertyName = Property->GetName();
		PropertyFullName = PropertyFullName.IsEmpty() ?
			PropertyName :
			FString::Format(TEXT("{0}.{1}"), { PropertyName, PropertyFullName });
	}
	return PropertyFullName;
}

FName FDataflowNode::GetPropertyFullName(const TArray<const FProperty*>& PropertyChain)
{
	const FString PropertyFullName = GetPropertyFullNameString(TConstArrayView<const FProperty*>(PropertyChain));
	return FName(*PropertyFullName);
}

FText FDataflowNode::GetPropertyDisplayNameText(const TArray<const FProperty*>& PropertyChain)
{
#if WITH_EDITORONLY_DATA  // GetDisplayNameText() is only available if WITH_EDITORONLY_DATA
	FText PropertyText;
	for (const FProperty* const Property : PropertyChain)
	{
		static const FTextFormat TextFormat(NSLOCTEXT("DataflowNode", "PropertyDisplayNameTextConcatenator", "{0}.{1}"));
		PropertyText = PropertyText.IsEmpty() ?
			Property->GetDisplayNameText() :
			FText::Format(TextFormat, Property->GetDisplayNameText(), PropertyText);
	}
	return PropertyText;
#else
	return FText::FromName(GetPropertyFullName(PropertyChain));
#endif
}

void FDataflowNode::RegisterInputConnection(const void* InProperty, const FName& PropertyName)
{
	if (TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* Struct = ScriptOnStruct->GetStruct())
		{
			TArray<const FProperty*> PropertyChain;
			const FProperty* const Property =
				FindProperty(Struct, InProperty, PropertyName, &PropertyChain);
			if (ensure(Property && PropertyChain.Num()))
			{
				const FName PropName(GetPropertyFullName(PropertyChain));
				const FName PropType(Property->GetCPPType());
				AddInput(new FDataflowInput({ PropType, PropName, this, Property }));
			}
		}
	}
}

void FDataflowNode::UnregisterInputConnection(const void* InProperty, const FName& PropertyName)
{
	if (TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			TArray<const FProperty*> PropertyChain;
			const FProperty* const Property =
				FindProperty(Struct, InProperty, PropertyName, &PropertyChain);
			if (ensure(Property && PropertyChain.Num()))
			{
				const uint32 Offset = GetPropertyOffset(PropertyChain);
				if (FDataflowInput* const* const Input = Inputs.Find(Offset))
				{
					Inputs.Remove(Offset);
					delete *Input;

					// Invalidate graph as this input might have had connections
					Invalidate();
				}
			}
		}
	}
}

void FDataflowNode::RegisterOutputConnection(const void* InProperty, const void* Passthrough, const FName& PropertyName, const FName& PassthroughName)
{
	if (TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* Struct = ScriptOnStruct->GetStruct())
		{
			FDataflowOutput* OutputConnection = nullptr;
			TArray<const FProperty*> PropertyChain;
			const FProperty* const Property =
				FindProperty(Struct, InProperty, PropertyName, &PropertyChain);
			if (ensure(Property && PropertyChain.Num()))
			{
				const FName PropName(GetPropertyFullName(PropertyChain));
				const FName PropType(Property->GetCPPType());
				OutputConnection = new FDataflowOutput({ PropType, PropName, this, Property });

				TArray<const FProperty*> PassthroughPropertyChain;
				if (FindProperty(Struct, Passthrough, PassthroughName, &PassthroughPropertyChain))
				{
					const uint32 PassthroughOffset = GetPropertyOffset(PassthroughPropertyChain);
					OutputConnection->SetPassthroughOffset(PassthroughOffset);
				}
				AddOutput(OutputConnection);
			}
		}
	}
}

bool FDataflowNode::ValidateConnections()
{
	bHasValidConnections = true;
#if WITH_EDITORONLY_DATA
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			for (FPropertyValueIterator PropertyIt(FProperty::StaticClass(), Struct, ScriptOnStruct->GetStructMemory()); PropertyIt; ++PropertyIt)
			{
				const FProperty* const Property = PropertyIt.Key();
				check(Property);

				if (Property->HasMetaData(FDataflowNode::DataflowInput))
				{
					TArray<const FProperty*> PropertyChain;
					PropertyIt.GetPropertyChain(PropertyChain);
					const FName PropName(GetPropertyFullName(PropertyChain));

					if (!FindInput(PropName))
					{
						UE_LOG(LogChaos, Warning, TEXT("Missing dataflow RegisterInputConnection in constructor for (%s:%s)"), *GetName().ToString(), *PropName.ToString())
							bHasValidConnections = false;
					}
				}
				if (Property->HasMetaData(FDataflowNode::DataflowOutput))
				{
					TArray<const FProperty*> PropertyChain;
					PropertyIt.GetPropertyChain(PropertyChain);
					const FName PropName(GetPropertyFullName(PropertyChain));

					const FDataflowOutput* const OutputConnection = FindOutput(PropName);
					if(!OutputConnection)
					{
						UE_LOG(LogChaos, Warning, TEXT("Missing dataflow RegisterOutputConnection in constructor for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
						bHasValidConnections = false;
					}
					// If OutputConnection is valid, validate passthrough connections if they exist
					else if (const FString* PassthroughName = Property->FindMetaData(FDataflowNode::DataflowPassthrough))
					{
						void* PassthroughConnectionAddress = OutputConnection->GetPassthroughRealAddress();
						if (PassthroughConnectionAddress == nullptr)
						{
							UE_LOG(LogChaos, Warning, TEXT("Missing DataflowPassthrough registration for (%s:%s)"), *GetName().ToString(), *PropName.ToString());
							bHasValidConnections = false;
						}

						// Assume passthrough name is relative to current property name.
						FString FullPassthroughName;
						if (PropertyChain.Num() <= 1)
						{
							FullPassthroughName = *PassthroughName;
						}
						else
						{
							FullPassthroughName = FString::Format(TEXT("{0}.{1}"), { GetPropertyFullNameString(TConstArrayView<const FProperty*>(&PropertyChain[1], PropertyChain.Num() - 1)), *PassthroughName});
						}

						const FDataflowInput* PassthroughConnectionInput = FindInput(FName(FullPassthroughName));
						const FDataflowInput* PassthroughConnectionInputFromArg = FindInput(PassthroughConnectionAddress);

						if(PassthroughConnectionInputFromArg != PassthroughConnectionInput)
						{
							UE_LOG(LogChaos, Warning, TEXT("Mismatch in declared and registered DataflowPassthrough connection; (%s:%s vs %s)"), *GetName().ToString(), *PropName.ToString(), *PassthroughConnectionInputFromArg->GetName().ToString());
							bHasValidConnections = false;
						}

						if(!PassthroughConnectionInput)
						{
							UE_LOG(LogChaos, Warning, TEXT("Incorrect DataflowPassthrough Connection set for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
							bHasValidConnections = false;
						}

						else if(OutputConnection->GetType() != PassthroughConnectionInput->GetType())
						{
							UE_LOG(LogChaos, Warning, TEXT("DataflowPassthrough connection types mismatch for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
							bHasValidConnections = false;
						}
					}
					else if(OutputConnection->GetPassthroughRealAddress()) 
					{
						UE_LOG(LogChaos, Warning, TEXT("Missing DataflowPassthrough declaration for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
						bHasValidConnections = false;
					}
				}
			}
		}
	}
#endif
	return bHasValidConnections;
}

FString FDataflowNode::GetToolTip()
{
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
#if WITH_EDITORONLY_DATA
		if (const UStruct* Struct = ScriptOnStruct->GetStruct())
		{
			FString OutStr, InputsStr, OutputsStr;

			FText StructText = Struct->GetToolTipText();

			OutStr.Appendf(TEXT("%s\n\n%s\n"), *GetDisplayName().ToString(), *StructText.ToString());

			for (FPropertyValueIterator PropertyIt(FProperty::StaticClass(), Struct, ScriptOnStruct->GetStructMemory()); PropertyIt; ++PropertyIt)
			{
				const FProperty* const Property = PropertyIt.Key();
				check(Property);

				if (Property->HasMetaData(TEXT("Tooltip")))
				{
					FString ToolTipStr = Property->GetToolTipText(true).ToString();
					if (ToolTipStr.Len() > 0)
					{
						TArray<FString> OutArr;
						ToolTipStr.ParseIntoArray(OutArr, TEXT(":\r\n"));
						
						if (OutArr.Num() == 0)
						{
							break;
						}

						TArray<const FProperty*> PropertyChain;
						PropertyIt.GetPropertyChain(PropertyChain);

						const FName PropName(GetPropertyFullName(PropertyChain));

						const FString& MainTooltipText = (OutArr.Num() > 1) ? OutArr[1] : OutArr[0];

						if (Property->HasMetaData(FDataflowNode::DataflowInput) &&
							Property->HasMetaData(FDataflowNode::DataflowOutput) &&
							Property->HasMetaData(FDataflowNode::DataflowPassthrough))
						{
							if (Property->HasMetaData(FDataflowNode::DataflowIntrinsic))
							{
								InputsStr.Appendf(TEXT("    %s [Intrinsic] - %s\n"), *PropName.ToString(), *MainTooltipText);
							}
							else
							{
								InputsStr.Appendf(TEXT("    %s - %s\n"), *PropName.ToString(), *MainTooltipText);
							}

							OutputsStr.Appendf(TEXT("    %s [Passthrough] - %s\n"), *PropName.ToString(), *MainTooltipText);
						}					
						else if (Property->HasMetaData(FDataflowNode::DataflowInput))
						{
							if (Property->HasMetaData(FDataflowNode::DataflowIntrinsic))
							{
								InputsStr.Appendf(TEXT("    %s [Intrinsic] - %s\n"), *PropName.ToString(), *MainTooltipText);
							}
							else
							{
								InputsStr.Appendf(TEXT("    %s - %s\n"), *PropName.ToString(), *MainTooltipText);
							}
						}
						else if (Property->HasMetaData(FDataflowNode::DataflowOutput))
						{
							OutputsStr.Appendf(TEXT("    %s - %s\n"), *PropName.ToString(), *MainTooltipText);
						}
					}
				}
			}

			if (InputsStr.Len() > 0)
			{
				OutStr.Appendf(TEXT("\n Input(s) :\n % s"), *InputsStr);
			}
			if (OutputsStr.Len() > 0)
			{
				OutStr.Appendf(TEXT("\n Output(s):\n%s"), *OutputsStr);
			}

			return OutStr;
		}
#endif
	}

	return "";
}

FText FDataflowNode::GetPinDisplayName(const FName& PropertyFullName)
{
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			TArray<const FProperty*> PropertyChain;
			if (FindProperty(Struct, PropertyFullName, &PropertyChain))
			{
				return GetPropertyDisplayNameText(PropertyChain);
			}
		}
	}

	return FText();
}

FString FDataflowNode::GetPinToolTip(const FName& PropertyFullName)
{
#if WITH_EDITORONLY_DATA
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			if (const FProperty* const Property = FindProperty(Struct, PropertyFullName))
			{
				if (Property->HasMetaData(TEXT("Tooltip")))
				{
					const FString ToolTipStr = Property->GetToolTipText(true).ToString();
					if (ToolTipStr.Len() > 0)
					{
						TArray<FString> OutArr;
						const int32 NumElems = ToolTipStr.ParseIntoArray(OutArr, TEXT(":\r\n"));

						if (NumElems == 2)
						{
							return OutArr[1];  // Return tooltip meta text
						}
						else if (NumElems == 1)
						{
							return OutArr[0];  // Return doc comment
						}
					}
				}
			}
		}
	}
#endif

	return "";
}

TArray<FString> FDataflowNode::GetPinMetaData(const FName& PropertyFullName)
{
#if WITH_EDITORONLY_DATA
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			if (const FProperty* const Property = FindProperty(Struct, PropertyFullName))
			{
				TArray<FString> MetaDataStrArr;
				if (Property->HasMetaData(FDataflowNode::DataflowPassthrough))
				{
					MetaDataStrArr.Add("Passthrough");
				}
				if (Property->HasMetaData(FDataflowNode::DataflowIntrinsic))
				{
					MetaDataStrArr.Add("Intrinsic");
				}

				return MetaDataStrArr;
			}
		}
	}
#endif

	return TArray<FString>();
}

void FDataflowNode::CopyNodeProperties(const TSharedPtr<FDataflowNode> CopyFromDataflowNode)
{
	TArray<uint8> NodeData;

	FObjectWriter ArWriter(NodeData);
	CopyFromDataflowNode->SerializeInternal(ArWriter);

	FObjectReader ArReader(NodeData);
	this->SerializeInternal(ArReader);
}







