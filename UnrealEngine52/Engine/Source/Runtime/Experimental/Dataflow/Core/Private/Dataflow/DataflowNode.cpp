// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNode.h"

#include "ChaosLog.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowArchive.h"

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

		if (!Inputs.Contains(InPtr->Property->GetOffset_ForInternal()))
		{
			Inputs.Add(InPtr->Property->GetOffset_ForInternal(), InPtr);
		}
		else
		{
			Inputs[InPtr->Property->GetOffset_ForInternal()] = InPtr;
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

		if (!Outputs.Contains(InPtr->Property->GetOffset_ForInternal()))
		{
			Outputs.Add(InPtr->Property->GetOffset_ForInternal(), InPtr);
		}
		else
		{
			Outputs[InPtr->Property->GetOffset_ForInternal()] = InPtr;
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

void FDataflowNode::Invalidate()
{
	LastModifiedTimestamp = Dataflow::FTimestamp(FPlatformTime::Cycles64());

	for (TPair<uint32, FDataflowOutput*> Elem : Outputs)
	{
		FDataflowOutput* Con = Elem.Value;
		Con->Invalidate();
	}

	OnNodeInvalidatedDelegate.Broadcast(this);
}

void FDataflowNode::RegisterInputConnection(const void* InProperty)
{
	if (TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* Struct = ScriptOnStruct->GetStruct())
		{
			for (TFieldIterator<FProperty> PropertyIt(Struct); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				size_t RealAddress = (size_t)this + Property->GetOffset_ForInternal();
				if (RealAddress == (size_t)InProperty)
				{
					FName PropName(Property->GetName());
					FName PropType(Property->GetCPPType());
					AddInput(new FDataflowInput({ PropType, PropName, this, Property }));
				}
			}
		}
	}
}

void FDataflowNode::RegisterOutputConnection(const void* InProperty, const void* Passthrough)
{
	if (TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* Struct = ScriptOnStruct->GetStruct())
		{
			FDataflowOutput* OutputConnection = nullptr;
			size_t PassthroughOffset = INDEX_NONE;
			for (TFieldIterator<FProperty> PropertyIt(Struct); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				size_t RealAddress = (size_t)this + Property->GetOffset_ForInternal();
				if (RealAddress == (size_t)InProperty)
				{
					FName PropName(Property->GetName());
					FName PropType(Property->GetCPPType());
					OutputConnection = new FDataflowOutput({ PropType, PropName, this, Property });
				}
				if (RealAddress == (size_t)Passthrough)
				{
					PassthroughOffset = (uint32)Property->GetOffset_ForInternal();
				}
			}
			if(OutputConnection != nullptr)
			{
				if(PassthroughOffset != INDEX_NONE)
				{
					OutputConnection->SetPassthroughOffsetAddress(PassthroughOffset);
				}
				AddOutput(OutputConnection);
			}
			
		}
	}
}


bool FDataflowNode::ValidateConnections()
{
	bValid = true;
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* Struct = ScriptOnStruct->GetStruct())
		{
			for (TFieldIterator<FProperty> PropertyIt(Struct); PropertyIt; ++PropertyIt)
			{
				const FProperty* Property = *PropertyIt;
				FName PropName(Property->GetName());
#if WITH_EDITORONLY_DATA
				if (Property->HasMetaData(FDataflowNode::DataflowInput))
				{
					if (!FindInput(PropName))
					{
						UE_LOG(LogChaos, Warning, TEXT("Missing dataflow RegisterInputConnection in constructor for (%s:%s)"), *GetName().ToString(), *PropName.ToString())
						bValid = false;
					}
				}
				if (Property->HasMetaData(FDataflowNode::DataflowOutput))
				{
					const FDataflowOutput* OutputConnection = FindOutput(PropName);
					if(!OutputConnection)
					{
						UE_LOG(LogChaos, Warning, TEXT("Missing dataflow RegisterOutputConnection in constructor for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
						bValid = false;
					}

					// Validate passthrough connections if they exist
					if (const FString* PassthroughName = Property->FindMetaData(FDataflowNode::DataflowPassthrough))
					{
						void* PassthroughConnectionAddress = OutputConnection->GetPassthroughRealAddress();
						if(PassthroughConnectionAddress == nullptr)
						{
							UE_LOG(LogChaos, Warning, TEXT("Missing DataflowPassthrough registration for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
							bValid = false;
						}

						const FDataflowInput* PassthroughConnectionInput = FindInput(FName(*PassthroughName));
						const FDataflowInput* PassthroughConnectionInputFromArg = FindInput(PassthroughConnectionAddress);

						if(PassthroughConnectionInputFromArg != PassthroughConnectionInput)
						{
							UE_LOG(LogChaos, Warning, TEXT("Mismatch in declared and registered DataflowPassthrough connection; (%s:%s vs %s)"), *GetName().ToString(), *PropName.ToString(), *PassthroughConnectionInputFromArg->GetName().ToString());
							bValid = false;
						}

						if(!PassthroughConnectionInput)
						{
							UE_LOG(LogChaos, Warning, TEXT("Incorrect DataflowPassthrough Connection set for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
							bValid = false;
						}

						else if(OutputConnection->GetType() != PassthroughConnectionInput->GetType())
						{
							UE_LOG(LogChaos, Warning, TEXT("DataflowPassthrough connection types mismatch for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
							bValid = false;
						}
					}
					else if(OutputConnection->GetPassthroughRealAddress()) 
					{
						UE_LOG(LogChaos, Warning, TEXT("Missing DataflowPassthrough declaration for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
						bValid = false;
					}
				}
#endif
			}
		}
	}
	return bValid;
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
			
			for (TFieldIterator<FProperty> PropertyIt(Struct); PropertyIt; ++PropertyIt)
			{
				const FProperty* Property = *PropertyIt;

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
						const FString& MainTooltipText = (OutArr.Num() > 1) ? OutArr[1] : OutArr[0];

						if (Property->HasMetaData(FDataflowNode::DataflowInput) &&
							Property->HasMetaData(FDataflowNode::DataflowOutput) &&
							Property->HasMetaData(FDataflowNode::DataflowPassthrough))
						{
							if (Property->HasMetaData(FDataflowNode::DataflowIntrinsic))
							{
								InputsStr.Appendf(TEXT("    %s [Intrinsic] - %s\n"), *Property->GetName(), *MainTooltipText);
							}
							else
							{
								InputsStr.Appendf(TEXT("    %s - %s\n"), *Property->GetName(), *MainTooltipText);
							}

							OutputsStr.Appendf(TEXT("    %s [Passthrough] - %s\n"), *Property->GetName(), *MainTooltipText);
						}					
						else if (Property->HasMetaData(FDataflowNode::DataflowInput))
						{
							if (Property->HasMetaData(FDataflowNode::DataflowIntrinsic))
							{
								InputsStr.Appendf(TEXT("    %s [Intrinsic] - %s\n"), *Property->GetName(), *MainTooltipText);
							}
							else
							{
								InputsStr.Appendf(TEXT("    %s - %s\n"), *Property->GetName(), *MainTooltipText);
							}
						}
						else if (Property->HasMetaData(FDataflowNode::DataflowOutput))
						{
							OutputsStr.Appendf(TEXT("    %s - %s\n"), *Property->GetName(), *MainTooltipText);
						}
						
						if (!Property->HasMetaData(FDataflowNode::DataflowInput) && !Property->HasMetaData(FDataflowNode::DataflowOutput))
						{
							InputsStr.Appendf(TEXT("    %s - %s\n"), *Property->GetName(), *MainTooltipText);
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

FString FDataflowNode::GetPinToolTip(const FName& PropertyName)
{
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* Struct = ScriptOnStruct->GetStruct())
		{
			for (TFieldIterator<FProperty> PropertyIt(Struct); PropertyIt; ++PropertyIt)
			{
				const FProperty* Property = *PropertyIt;

#if WITH_EDITORONLY_DATA
				if (Property->GetName() == PropertyName.ToString())
				{
					if (Property->HasMetaData(TEXT("Tooltip")))
					{
						FString ToolTipStr = Property->GetToolTipText(true).ToString();
						if (ToolTipStr.Len() > 0)
						{
							TArray<FString> OutArr;
							int32 NumElems = ToolTipStr.ParseIntoArray(OutArr, TEXT(":\r\n"));

							if (NumElems == 2)
							{
								return OutArr[1];
							}
						}
					}
				}
#endif
			}
		}
	}

	return "";
}

TArray<FString> FDataflowNode::GetPinMetaData(const FName& PropertyName)
{
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* Struct = ScriptOnStruct->GetStruct())
		{
			for (TFieldIterator<FProperty> PropertyIt(Struct); PropertyIt; ++PropertyIt)
			{
				const FProperty* Property = *PropertyIt;

#if WITH_EDITORONLY_DATA
				if (Property->GetName() == PropertyName.ToString())
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
#endif
			}
		}
	}

	return TArray<FString>();
}






