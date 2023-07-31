// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMCore/RigVMTemplate.h"
#include "RigVMModel/RigVMController.h"
#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMTemplateNode)

////////////////////////////////////////////////////////////////////////////////////////////////

void FRigVMTemplatePreferredType::UpdateStringFromIndex()
{
	static constexpr TCHAR Format[] = TEXT("%s,%s");
	const FRigVMTemplateArgumentType Type = FRigVMRegistry::Get().GetType(TypeIndex);
	TypeString = FString::Printf(Format, *Type.CPPType.ToString(), *Type.GetCPPTypeObjectPath().ToString());
}

void FRigVMTemplatePreferredType::UpdateIndexFromString()
{
	// during duplicate we may not have type strings here
	if(TypeString.IsEmpty())
	{
		return;
	}

	FString Left, Right;
	verify(TypeString.Split(TEXT(","), &Left, &Right));

	UObject* CPPTypeObject = nullptr;

	static const FString NoneString = FName(NAME_None).ToString(); 
	if(Right != NoneString)
	{
		CPPTypeObject = URigVMPin::FindObjectFromCPPTypeObjectPath(Right);
	}

	const FRigVMTemplateArgumentType Type(*Left, CPPTypeObject);
	TypeIndex = FRigVMRegistry::Get().FindOrAddType(Type);
}

////////////////////////////////////////////////////////////////////////////////////////////////

URigVMTemplateNode::URigVMTemplateNode()
	: Super()
	, TemplateNotation(NAME_None)
	, ResolvedFunctionName()
	, CachedTemplate(nullptr)
	, CachedFunction(nullptr)
	, ResolvedPermutation(INDEX_NONE)
{
}

void URigVMTemplateNode::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);
	ConvertPreferredTypesToString();
}

void URigVMTemplateNode::PostLoad()
{
	Super::PostLoad();

	for (URigVMPin* Pin : GetPins())
	{
		// GetTypeIndex ensures that the pin type is registered
		// Only need to register for visible pins
		if (Pin->GetDirection() == ERigVMPinDirection::IO ||
			Pin->GetDirection() == ERigVMPinDirection::Input ||
			Pin->GetDirection() == ERigVMPinDirection::Output)
		{
			Pin->GetTypeIndex();
		}
	}
	
	// if there are brackets in the notation remove them
	const FString OriginalNotation = TemplateNotation.ToString();
	const FString SanitizedNotation = OriginalNotation.Replace(TEXT("[]"), TEXT(""));
	if(OriginalNotation != SanitizedNotation)
	{
		TemplateNotation = *SanitizedNotation;
	}

	// upgrade from a previous version where we stored the preferred types as strings
	if(!PreferredPermutationTypes_DEPRECATED.IsEmpty())
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();

		PreferredPermutationPairs.Reserve(PreferredPermutationTypes_DEPRECATED.Num());

		for(const FString& PreferredPermutation : PreferredPermutationTypes_DEPRECATED)
		{
			FString ArgName, CPPType;
			PreferredPermutation.Split(TEXT(":"), &ArgName, &CPPType);

			const FRigVMTemplateArgumentType Type = Registry.FindTypeFromCPPType(CPPType);
			PreferredPermutationPairs.Emplace(*ArgName, Registry.GetTypeIndex(Type));
		}

		PreferredPermutationTypes_DEPRECATED.Reset();
	}

	ConvertPreferredTypesToTypeIndex();
	InvalidateCache();
}

void URigVMTemplateNode::ConvertPreferredTypesToString()
{
	// we rely on the strings being serialized
	for(FRigVMTemplatePreferredType& PreferredType : PreferredPermutationPairs)
	{
		PreferredType.UpdateStringFromIndex();
	}
}

void URigVMTemplateNode::ConvertPreferredTypesToTypeIndex()
{
	const FRigVMTemplate* Template = GetTemplate();
	
	// we rely on the strings being serialized - so on load we need to update the type index again 
	for(FRigVMTemplatePreferredType& PreferredType : PreferredPermutationPairs)
	{
		PreferredType.UpdateIndexFromString();

		if(Template)
		{
			if(const FRigVMTemplateArgument* Argument = Template->FindArgument(PreferredType.GetArgument()))
			{
				TRigVMTypeIndex ResolvedTypeIndex = INDEX_NONE;
				if(Argument->SupportsTypeIndex(PreferredType.GetTypeIndex(), &ResolvedTypeIndex))
				{
					if(ResolvedTypeIndex != PreferredType.GetTypeIndex())
					{
						PreferredType.TypeIndex = ResolvedTypeIndex;
						PreferredType.UpdateStringFromIndex();
					}
				}
			}
		}
	}
}

TRigVMTypeIndex URigVMTemplateNode::GetPreferredType(const FName& ArgumentName) const
{
	for (const FRigVMTemplatePreferredType& PreferredType : PreferredPermutationPairs)
	{
		if (PreferredType.Argument == ArgumentName)
		{
			return PreferredType.GetTypeIndex();
		}
	}
	return INDEX_NONE;
}

UScriptStruct* URigVMTemplateNode::GetScriptStruct() const
{
	if(const FRigVMFunction* Function = GetResolvedFunction())
	{
		return Function->Struct;
	}
	return nullptr;
}

FString URigVMTemplateNode::GetNodeTitle() const
{
	if(!IsResolved())
	{
		if(const FRigVMTemplate* Template = GetTemplate())
		{
			return Template->GetName().ToString();
		}
	}
	
	FString ResolvedNodeTitle = Super::GetNodeTitle();

	const int32 BracePos = ResolvedNodeTitle.Find(TEXT(" ("));
	if(BracePos != INDEX_NONE)
	{
		ResolvedNodeTitle = ResolvedNodeTitle.Left(BracePos);
	}

	return ResolvedNodeTitle;
}

FName URigVMTemplateNode::GetMethodName() const
{
	if(const FRigVMFunction* Function = GetResolvedFunction())
	{
		return Function->GetMethodName();
	}
	return NAME_None;
}

FText URigVMTemplateNode::GetToolTipText() const
{
	if(const FRigVMTemplate* Template = GetTemplate())
	{
		const TArray<int32> PermutationIndices = GetFilteredPermutationsIndices();
		return Template->GetTooltipText(PermutationIndices);
	}
	return Super::GetToolTipText();
}

FText URigVMTemplateNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	const FText SuperToolTip = Super::GetToolTipTextForPin(InPin);
	if (const FRigVMTemplate* Template = GetTemplate())
	{
		const URigVMPin* RootPin = InPin->GetRootPin();
		if (RootPin->IsWildCard())
		{
			if (const FRigVMTemplateArgument* Arg = Template->FindArgument(RootPin->GetFName()))
			{
				FString Tooltip;
				
				if(FilteredPermutations.Num() == GetTemplate()->NumPermutations())
				{
					if(Arg->GetTypeIndices().Num() > 100)
					{
						Tooltip = TEXT("Supports any type.");
					}
				}
				
				if(Tooltip.IsEmpty())
				{
					FString SupportedTypesJoined;
					TArray<TRigVMTypeIndex> TypesPrinted;
					for (int32 Index=0; Index<Template->NumPermutations(); ++Index)
					{
						const TRigVMTypeIndex TypeIndex = Arg->GetTypeIndices()[Index];
						if (TypesPrinted.Contains(TypeIndex))
						{
							continue;
						}
						TypesPrinted.Add(TypeIndex);
						FString Type = FRigVMRegistry::Get().GetType(TypeIndex).CPPType.ToString();
						if (!FilteredPermutations.Contains(Index))
						{
							Type += TEXT(" : Breaks Connections");
						}
						SupportedTypesJoined += Type + TEXT("\n");
					}
					Tooltip = TEXT("Supported Types:\n\n") + SupportedTypesJoined;
				}

				if(!SuperToolTip.IsEmpty())
				{
					Tooltip += TEXT("\n\n") + SuperToolTip.ToString();
				}
				
				return FText::FromString(Tooltip);
			}
		}
	}
	return SuperToolTip;
}

TArray<URigVMPin*> URigVMTemplateNode::GetAggregateInputs() const
{
	return GetAggregatePins(ERigVMPinDirection::Input);
}

TArray<URigVMPin*> URigVMTemplateNode::GetAggregateOutputs() const
{
	return GetAggregatePins(ERigVMPinDirection::Output);
}

TArray<URigVMPin*> URigVMTemplateNode::GetAggregatePins(const ERigVMPinDirection& InDirection) const
{
	TArray<URigVMPin*> AggregatePins;
#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	if (const FRigVMTemplate* Template = GetTemplate())
	{
		TArray<int32> Permutations = FilteredPermutations;
		if (Permutations.IsEmpty())
		{
			Permutations.Reserve(Template->NumPermutations());
			for (int32 i=0; i<Template->NumPermutations(); ++i)
			{
				Permutations.Add(i);
			}
		}
		
		if (Permutations.IsEmpty())
		{
			return AggregatePins;
		}

		auto FindAggregatePins = [&](const int32& PermutationIndex)
		{
			TArray<URigVMPin*> Inputs;
			if (const FRigVMFunction* Permutation = Template->GetPermutation(PermutationIndex))
			{
				const TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(Permutation->Struct));
				if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(StructOnScope->GetStruct()))
				{
					for (URigVMPin* Pin : GetPins())
					{
						if (Pin->GetDirection() == InDirection)
						{
							if (const FProperty* Property = ScriptStruct->FindPropertyByName(Pin->GetFName()))
							{
								if (Property->HasMetaData(FRigVMStruct::AggregateMetaName))
								{
									Inputs.Add(Pin);
								}
							}			
						}
					}
				}
			}
			return Inputs;
		};
		
		AggregatePins = FindAggregatePins(Permutations[0]);
		for (int32 i=1; i<Permutations.Num(); ++i)
		{
			TArray<URigVMPin*> OtherAggregateInputs = FindAggregatePins(Permutations[i]);
			if (OtherAggregateInputs.Num() != AggregatePins.Num() ||
				OtherAggregateInputs.FilterByPredicate([&](const URigVMPin* OtherPin)
				{
					return !AggregatePins.Contains(OtherPin);
				}).Num() > 0)
			{
				return {};
			}
		}
	}
#endif
	return AggregatePins;
}

FName URigVMTemplateNode::GetNextAggregateName(const FName& InLastAggregatePinName) const
{
	FName NextName;	

#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	if (const FRigVMTemplate* Template = GetTemplate())
	{
		if (FilteredPermutations.IsEmpty())
		{
			return NextName;
		}

		if (const FRigVMFunction* Permutation = Template->GetPermutation(FilteredPermutations[0]))
		{
			const TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(Permutation->Struct));
			if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(StructOnScope->GetStruct()))
			{
				check(ScriptStruct->IsChildOf(FRigVMStruct::StaticStruct()));

				const FRigVMStruct* StructMemory = (const FRigVMStruct*)StructOnScope->GetStructMemory();
				return StructMemory->GetNextAggregateName(InLastAggregatePinName);
			}
		}
	}
#endif
	return FName();
}

FName URigVMTemplateNode::GetNotation() const
{
	return TemplateNotation;
}

bool URigVMTemplateNode::IsSingleton() const
{
	return GetTemplate() == nullptr;
}

bool URigVMTemplateNode::SupportsType(const URigVMPin* InPin, TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex)
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	static const FString WildCardCPPType = RigVMTypeUtils::GetWildCardCPPType();
	static const FString WildCardArrayCPPType = RigVMTypeUtils::ArrayTypeFromBaseType(WildCardCPPType);

	const URigVMPin* RootPin = InPin->GetRootPin();

	TRigVMTypeIndex TypeIndex = InTypeIndex;

	if(InPin->GetParentPin() == RootPin && RootPin->IsArray())
	{
		TypeIndex = Registry.GetArrayTypeFromBaseTypeIndex(TypeIndex);
	}

	// we always support the unknown type
	if(Registry.IsWildCardType(TypeIndex))
	{
		if(const FRigVMTemplate* Template = GetTemplate())
		{
			if(const FRigVMTemplateArgument* Argument = Template->FindArgument(RootPin->GetFName()))
			{
				// support this only on non-singleton arguments
				if(Argument->IsSingleton())
				{
					return false;
				}

				if(Registry.IsArrayType(TypeIndex))
				{
					if(Argument->GetArrayType() == FRigVMTemplateArgument::EArrayType_SingleValue)
					{
						return false;
					}
				}
				else
				{
					if(Argument->GetArrayType() == FRigVMTemplateArgument::EArrayType_ArrayValue)
					{
						return false;
					}
				}
				
				if(OutTypeIndex)
				{
					*OutTypeIndex = InTypeIndex;
				}
				return true;
			}
			else if (IsA<URigVMFunctionEntryNode>() || IsA<URigVMFunctionReturnNode>())
			{
				if(OutTypeIndex)
				{
					*OutTypeIndex = InTypeIndex;
				}
				return true;
			}
		}
		return false;
	}
	
	if (const FRigVMTemplate* Template = GetTemplate())
	{
		const TPair<FName,TRigVMTypeIndex> CacheKey(RootPin->GetFName(), TypeIndex);
		TRigVMTypeIndex SupportedTypeIndex = INDEX_NONE;
		if (Template->ArgumentSupportsTypeIndex(RootPin->GetFName(), TypeIndex, &SupportedTypeIndex))
		{
			if (OutTypeIndex)
			{
				(*OutTypeIndex) = SupportedTypeIndex;
			}
			return true;
		}

		// an entry/return node that does not contain an argument for the pin will always support the connections
		if (IsA<URigVMFunctionEntryNode>() || IsA<URigVMFunctionReturnNode>())
		{
			if (Template->FindArgument(RootPin->GetFName()) == nullptr)
			{
				return true;
			}
		}
		return false;
	}
	
	if(RootPin->GetTypeIndex() == TypeIndex)
	{
		if(OutTypeIndex)
		{
			*OutTypeIndex = TypeIndex;
		}
		return true;
	}
	return false;
}

bool URigVMTemplateNode::FilteredSupportsType(const URigVMPin* InPin, TRigVMTypeIndex InTypeIndex, TRigVMTypeIndex* OutTypeIndex, bool bAllowFloatingPointCasts)
{
	if (OutTypeIndex)
	{
		*OutTypeIndex = INDEX_NONE; 
	}

	const URigVMPin* RootPin = InPin;
	bool bIsArrayElement = false;
	bool bIsStructElement = false;
	if (URigVMPin* ParentPin = InPin->GetParentPin())
	{
		RootPin = ParentPin;
		if (ParentPin->IsArray())
		{
			bIsArrayElement = true;
		}
		else if (ParentPin->IsStruct())
		{
			bIsStructElement = true;
		}
	}

	if (bIsStructElement)
	{
		const bool bResult = InPin->GetTypeIndex() == InTypeIndex;
		if(bResult)
		{
			if(OutTypeIndex)
			{
				*OutTypeIndex = InTypeIndex; 
			}
		}
		return bResult;
	}

	const FRigVMTemplateArgument* Argument = GetTemplate()->FindArgument(RootPin->GetFName());
	if (Argument == nullptr)
	{
		return false;
	}

	const FRigVMRegistry& Registry = FRigVMRegistry::Get();

	TRigVMTypeIndex RootTypeIndex = InTypeIndex;
	if (bIsArrayElement)
	{
		RootTypeIndex = Registry.GetArrayTypeFromBaseTypeIndex(RootTypeIndex);
	}

	if (FilteredPermutations.Num() == GetTemplate()->NumPermutations())
	{
		return GetTemplate()->ArgumentSupportsTypeIndex(RootPin->GetFName(), InTypeIndex, OutTypeIndex);
	}

	const TArray<TRigVMTypeIndex>& TypeIndices = Argument->GetTypeIndices();
	for (const int32& PermutationIndex : FilteredPermutations)
	{
		const TRigVMTypeIndex& FilteredTypeIndex = TypeIndices[PermutationIndex];
		if (Registry.CanMatchTypes(FilteredTypeIndex, RootTypeIndex, bAllowFloatingPointCasts))
		{
			return true;
		}
	}

	return false;
}

TArray<const FRigVMFunction*> URigVMTemplateNode::GetResolvedPermutations() const
{
	TArray<int32> Indices = GetFilteredPermutationsIndices();
	TArray<const FRigVMFunction*> Functions;
	for(const int32 Index : Indices)
	{
		Functions.Add(GetTemplate()->GetPermutation(Index));
	}
	return Functions;
}

const FRigVMTemplate* URigVMTemplateNode::GetTemplate() const
{
	if(CachedTemplate == nullptr)
	{
		CachedTemplate = FRigVMRegistry::Get().FindTemplate(GetNotation());
	}
	return CachedTemplate;
}

const FRigVMFunction* URigVMTemplateNode::GetResolvedFunction() const
{
	if(CachedFunction == nullptr)
	{
		if(!ResolvedFunctionName.IsEmpty())
		{
			CachedFunction = FRigVMRegistry::Get().FindFunction(*ResolvedFunctionName);
		}

		if(CachedFunction == nullptr)
		{
			if(ResolvedPermutation != INDEX_NONE)
			{
				CachedFunction = GetTemplate()->GetPermutation(ResolvedPermutation);
			}
		}
	}
	return CachedFunction;
}

bool URigVMTemplateNode::IsResolved() const
{
	return GetScriptStruct() != nullptr;
}

bool URigVMTemplateNode::IsFullyUnresolved() const
{
	check(GetTemplate());

	// all permutations are available means we haven't resolved any wildcard pin
	return GetFilteredPermutationsIndices().Num() == GetTemplate()->NumPermutations();
}

FString URigVMTemplateNode::GetInitialDefaultValueForPin(const FName& InRootPinName, const TArray<int32>& InPermutationIndices) const
{
	if(GetTemplate() == nullptr)
	{
		return FString();
	}

	URigVMPin* Pin = FindPin(InRootPinName.ToString());
	if (!Pin)
	{
		return FString();
	}
	
	TArray<int32> PermutationIndices = InPermutationIndices;
	if(PermutationIndices.IsEmpty())
	{
		PermutationIndices = GetFilteredPermutationsIndices();
	}

	FString DefaultValue;
	const FRigVMTemplate* Template = GetTemplate();
	const FRigVMDispatchFactory* Factory = Template->GetDispatchFactory(); 
	const FRigVMTemplateArgument* Argument = Template->FindArgument(InRootPinName);

	if (Argument)
	{
		for(const int32 PermutationIndex : PermutationIndices)
		{
			FString NewDefaultValue;

			const TRigVMTypeIndex TypeIndex = Argument->GetTypeIndices()[PermutationIndex];

			// INDEX_NONE indicates deleted permutation
			if (TypeIndex == INDEX_NONE)
			{
				continue;
			}
			
			if(Factory)
			{
				NewDefaultValue = Factory->GetArgumentDefaultValue(Argument->GetName(), TypeIndex);
			}
			else if(const FRigVMFunction* Permutation = Template->GetPermutation(PermutationIndex))
			{
				const TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(Permutation->Struct));
				const FRigVMStruct* DefaultStruct = (const FRigVMStruct*)StructOnScope->GetStructMemory();

				const bool bUseQuotes = TypeIndex != RigVMTypeUtils::TypeIndex::FString && TypeIndex != RigVMTypeUtils::TypeIndex::FName;
				NewDefaultValue = DefaultStruct->ExportToFullyQualifiedText(
					Cast<UScriptStruct>(StructOnScope->GetStruct()), InRootPinName, nullptr, bUseQuotes);
			}
			else
			{
				if (FRigVMRegistry::Get().IsArrayType(TypeIndex))
				{
					NewDefaultValue = TEXT("()");
				}
				else
				{
					const FRigVMTemplateArgumentType& Type = FRigVMRegistry::Get().GetType(TypeIndex);
					if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Type.CPPTypeObject))
					{
						TArray<uint8, TAlignedHeapAllocator<16>> TempBuffer;
						TempBuffer.AddUninitialized(ScriptStruct->GetStructureSize());

						// call the struct constructor to initialize the struct
						ScriptStruct->InitializeDefaultValue(TempBuffer.GetData());

						ScriptStruct->ExportText(NewDefaultValue, TempBuffer.GetData(), nullptr, nullptr, PPF_None, nullptr);
						ScriptStruct->DestroyStruct(TempBuffer.GetData());				
					}
					else if (const UEnum* Enum = Cast<UEnum>(Type.CPPTypeObject))
					{
						NewDefaultValue = Enum->GetNameStringByValue(0);
					}
					else if(const UClass* Class = Cast<UClass>(Type.CPPTypeObject))
					{
						// not supporting objects yet
						ensure(false);
					}
					else if (Type.CPPType == RigVMTypeUtils::FloatTypeName)
					{
						NewDefaultValue = TEXT("0.000000");				
					}
					else if (Type.CPPType == RigVMTypeUtils::DoubleTypeName)
					{
						NewDefaultValue = TEXT("0.000000");
					}
					else if (Type.CPPType == RigVMTypeUtils::Int32TypeName)
					{
						NewDefaultValue = TEXT("0");
					}
					else if (Type.CPPType == RigVMTypeUtils::BoolTypeName)
					{
						NewDefaultValue = TEXT("False");
					}
					else if (Type.CPPType == RigVMTypeUtils::FStringTypeName)
					{
						NewDefaultValue = TEXT("");
					}
					else if (Type.CPPType == RigVMTypeUtils::FNameTypeName)
					{
						NewDefaultValue = TEXT("");
					}
				}			
			}

			if(!NewDefaultValue.IsEmpty())
			{
				if(DefaultValue.IsEmpty())
				{
					DefaultValue = NewDefaultValue;
				}
				else if(!NewDefaultValue.Equals(DefaultValue))
				{
					return FString();
				}
			}
		}
	}
	
	return DefaultValue;
}

FName URigVMTemplateNode::GetDisplayNameForPin(const FName& InRootPinName,
	const TArray<int32>& InPermutationIndices) const
{
#if WITH_EDITOR
	if(const FRigVMTemplate* Template = GetTemplate())
	{
		const TArray<int32>* PermutationIndicesPtr = &InPermutationIndices;
		if(PermutationIndicesPtr->IsEmpty())
		{
			PermutationIndicesPtr = &GetFilteredPermutationsIndices();
		}

		const FText DisplayNameText = Template->GetDisplayNameForArgument(InRootPinName, *PermutationIndicesPtr);
		if(DisplayNameText.IsEmpty())
		{
			return InRootPinName;
		}

		const FName DisplayName = *DisplayNameText.ToString();
		if(DisplayName.IsEqual(InRootPinName))
		{
			return NAME_None;
		}
		return DisplayName;
	}
#endif
	return NAME_None;
}

const TArray<int32>& URigVMTemplateNode::GetFilteredPermutationsIndices() const
{
	return FilteredPermutations;
}

TArray<TRigVMTypeIndex> URigVMTemplateNode::GetFilteredTypesForPin(URigVMPin* InPin) const
{
	ensureMsgf(InPin->GetNode() == this, TEXT("GetFilteredTypesForPin of %s with pin from another node %s"), *GetNodePath(), *InPin->GetPinPath(true));
	TArray<TRigVMTypeIndex> FilteredTypes;

	if (IsSingleton())
	{
		return {InPin->GetTypeIndex()};
	}

	if (FilteredPermutations.IsEmpty())
	{
		return FilteredTypes;	
	}

	if (InPin->IsStructMember())
	{
		return {InPin->GetTypeIndex()};
	}

	if (!PreferredPermutationPairs.IsEmpty())
	{
		for (const FRigVMTemplatePreferredType& PreferredType : PreferredPermutationPairs)
		{
			if (InPin->GetFName() == PreferredType.GetArgument())
			{
				const FRigVMTemplateArgument* Argument = GetTemplate()->FindArgument(PreferredType.GetArgument());
				for (const TRigVMTypeIndex& TypeIndex : Argument->GetTypeIndices())
				{
					if (TypeIndex == PreferredType.GetTypeIndex())
					{
						if (InPin->IsArrayElement())
						{
							return {FRigVMRegistry::Get().GetArrayTypeFromBaseTypeIndex(TypeIndex)};
						}
						else
						{
							return {TypeIndex};
						}
					}
				}	
			}
		}
	}
	
	const URigVMPin* RootPin = InPin;
	bool bIsArrayElement = false;
	if (URigVMPin* ParentPin = InPin->GetParentPin())
	{
		RootPin = ParentPin;
		bIsArrayElement = true;
	}
	if (const FRigVMTemplateArgument* Argument = GetTemplate()->FindArgument(RootPin->GetFName()))
	{
		FilteredTypes = Argument->GetSupportedTypeIndices(FilteredPermutations);
		if (bIsArrayElement)
		{
			for (TRigVMTypeIndex& ArrayType : FilteredTypes)
			{
				ArrayType = FRigVMRegistry::Get().GetBaseTypeFromArrayTypeIndex(ArrayType);
			}
		}
	}
	return FilteredTypes;
}

TRigVMTypeIndex URigVMTemplateNode::TryReduceTypesToSingle(const TArray<TRigVMTypeIndex>& InTypes, const TRigVMTypeIndex PreferredType) const
{
	if (InTypes.IsEmpty())
	{
		return INDEX_NONE;
	}

	if (InTypes.Num() == 1)
	{
		return InTypes[0];
	}
	
	for (int32 i=1; i<InTypes.Num(); ++i)
	{
		if (!FRigVMRegistry::Get().CanMatchTypes(InTypes[0], InTypes[i], true))
		{
			return INDEX_NONE;
		}
	}

	if (InTypes.Contains(PreferredType))
	{
		return PreferredType;
	}

	return InTypes[0];
}

TArray<int32> URigVMTemplateNode::GetNewFilteredPermutations(URigVMPin* InPin, URigVMPin* LinkedPin)
{	
	TArray<int32> NewFilteredPermutations;
	if (InPin->GetNode() != this)
	{
		return NewFilteredPermutations;
	}

	NewFilteredPermutations.Reserve(FilteredPermutations.Num());
	
	bool bIsArrayElement = false;
	bool bIsStructElement = false;
	URigVMPin* RootPin = InPin;
	if (URigVMPin* ParentPin = InPin->GetParentPin())
	{
		RootPin = ParentPin;
		bIsArrayElement = RootPin->IsArray();
		bIsStructElement = RootPin->IsStruct();
	}

	if (bIsStructElement)
	{
		if (FRigVMRegistry::Get().CanMatchTypes(InPin->GetTypeIndex(), LinkedPin->GetTypeIndex(), true))
		{
			return FilteredPermutations;
		}
	}

	TArray<int32> PermutationsToTry = FilteredPermutations;

	// Reduce permutations to the ones respecting the preferred types
	const TArray<int32> PreferredPermutations = FindPermutationsForTypes(PreferredPermutationPairs, true);
	PermutationsToTry = PermutationsToTry.FilterByPredicate([&](const int32& OtherPermutation) { return PreferredPermutations.Contains(OtherPermutation); });
	
	bool bLinkedIsTemplate = false;
	if (URigVMTemplateNode* LinkedTemplate = Cast<URigVMTemplateNode>(LinkedPin->GetNode()))
	{
		if (!LinkedTemplate->IsSingleton() && !LinkedPin->IsStructMember())
		{
			bLinkedIsTemplate = true;
			if (const FRigVMTemplateArgument* Argument = GetTemplate()->FindArgument(RootPin->GetFName()))
			{
				for(int32 PermutationIndex : PermutationsToTry)
				{
					TRigVMTypeIndex TypeIndex = Argument->GetTypeIndices()[PermutationIndex];
					if (bIsArrayElement)
					{
						TypeIndex = FRigVMRegistry::Get().GetBaseTypeFromArrayTypeIndex(TypeIndex);
					}
					if (LinkedTemplate->FilteredSupportsType(LinkedPin, TypeIndex))
					{
						NewFilteredPermutations.Add(PermutationIndex);
					}
				}
			}
		}
	}
	
	if (!bLinkedIsTemplate)
	{
		if (const FRigVMTemplateArgument* Argument = GetTemplate()->FindArgument(RootPin->GetFName()))
		{
			const TRigVMTypeIndex LinkedTypeIndex = LinkedPin->GetTypeIndex();
			for(int32 PermutationIndex : PermutationsToTry)
			{
				TRigVMTypeIndex TypeIndex = Argument->GetTypeIndices()[PermutationIndex];
				if (bIsArrayElement)
				{
					TypeIndex = FRigVMRegistry::Get().GetBaseTypeFromArrayTypeIndex(TypeIndex);
				}

				if (FRigVMRegistry::Get().CanMatchTypes(TypeIndex, LinkedTypeIndex, true))
				{
					NewFilteredPermutations.Add(PermutationIndex);
				}
			}
		}
	}
	return NewFilteredPermutations;
}

TArray<int32> URigVMTemplateNode::GetNewFilteredPermutations(URigVMPin* InPin, const TArray<TRigVMTypeIndex>& InTypeIndices)
{
	TArray<int32> NewFilteredPermutations;
	NewFilteredPermutations.Reserve(FilteredPermutations.Num());

	URigVMPin* RootPin = InPin;
	TArray<TRigVMTypeIndex> RootTypes = InTypeIndices;
	if (URigVMPin* ParentPin = InPin->GetParentPin())
	{
		RootPin = ParentPin;
		for (TRigVMTypeIndex& TypeIndex : RootTypes)
		{
			TypeIndex = FRigVMRegistry::Get().GetArrayTypeFromBaseTypeIndex(TypeIndex);
		}
	}

	TArray<int32> PermutationsToTry = FilteredPermutations;

	// Reduce permutations to the ones respecting the preferred types
	const TArray<int32> PreferredPermutations = FindPermutationsForTypes(PreferredPermutationPairs, true);
	PermutationsToTry = PermutationsToTry.FilterByPredicate([&](const int32& OtherPermutation) { return PreferredPermutations.Contains(OtherPermutation); });
	
	if (const FRigVMTemplateArgument* Argument = GetTemplate()->FindArgument(RootPin->GetFName()))
	{
		const TArray<TRigVMTypeIndex>& TypeIndices = Argument->GetTypeIndices();
		for(int32 PermutationIndex : PermutationsToTry)
		{
			for (const TRigVMTypeIndex& RootTypeIndex : RootTypes)
			{				
				if (FRigVMRegistry::Get().CanMatchTypes(TypeIndices[PermutationIndex], RootTypeIndex, true))
				{
					NewFilteredPermutations.Add(PermutationIndex);
					break;
				}
			}			
		}
	}
	return NewFilteredPermutations;
}

TArray<int32> URigVMTemplateNode::FindPermutationsForTypes(const TArray<FRigVMTemplatePreferredType>& ArgumentTypes, bool bAllowCasting) const
{
	TArray<int32> Permutations;
	if (const FRigVMTemplate* Template = GetTemplate())
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();
		
		TArray<const FRigVMTemplateArgument*> Args;
		TArray<TRigVMTypeIndex> TypeIndices;
		for (const FRigVMTemplatePreferredType& ArgumentType : ArgumentTypes)
		{
			if (const FRigVMTemplateArgument* Argument = Template->FindArgument(ArgumentType.GetArgument()))
			{
				Args.Add(Argument);
				TypeIndices.Add(ArgumentType.GetTypeIndex());
			}
			else
			{
				return {};
			}
		}
		
		for (int32 i=0; i<Template->NumPermutations(); ++i)
		{
			bool bAllArgsMatched = true;
			for (int32 ArgIndex = 0; ArgIndex < Args.Num(); ++ArgIndex)
			{
				const FRigVMTemplateArgument* Argument = Args[ArgIndex];
				if (Argument)
				{
					if ((bAllowCasting && !Registry.CanMatchTypes(Argument->GetTypeIndices()[i], TypeIndices[ArgIndex], true)) ||
						(!bAllowCasting && Argument->GetTypeIndices()[i] != TypeIndices[ArgIndex]))
					{
						bAllArgsMatched = false;
						break;
					}
				}
				else
				{
					// The preferred type doesn't own an argument yet. Supports all types.
				}
			}

			if (bAllArgsMatched)
			{
				Permutations.Add(i);
			}
		}
	}
	return Permutations;
}

TArray<FRigVMTemplatePreferredType> URigVMTemplateNode::GetPreferredTypesForPermutation(const int32 InPermutationIndex) const
{
	TArray<FRigVMTemplatePreferredType> ArgTypes;
	if (const FRigVMTemplate* Template = GetTemplate())
	{
		for (int32 ArgIndex = 0; ArgIndex < Template->NumArguments(); ++ArgIndex)
		{
			const FRigVMTemplateArgument* Argument = Template->GetArgument(ArgIndex);
			if (Argument->GetTypeIndices().Num() > InPermutationIndex)
			{
				ArgTypes.Emplace(Argument->GetName(), Argument->GetTypeIndices()[InPermutationIndex]);
			}
			else
			{
				return {};
			}
		}
	}

	return ArgTypes;
}

FRigVMTemplateTypeMap URigVMTemplateNode::GetTypesForPermutation(const int32 InPermutationIndex) const
{
	if (const FRigVMTemplate* Template = GetTemplate())
	{
		return Template->GetTypesForPermutation(InPermutationIndex);
	}

	return FRigVMTemplateTypeMap();
}

bool URigVMTemplateNode::PinNeedsFilteredTypesUpdate(URigVMPin* InPin, const TArray<TRigVMTypeIndex>& InTypeIndices)
{
	const TArray<int32> NewFilteredPermutations = GetNewFilteredPermutations(InPin, InTypeIndices);
	if (NewFilteredPermutations.Num() == FilteredPermutations.Num())
	{
		return false;
	}
	return true;
}

bool URigVMTemplateNode::PinNeedsFilteredTypesUpdate(URigVMPin* InPin, URigVMPin* LinkedPin)
{
	const TArray<int32> NewFilteredPermutations = GetNewFilteredPermutations(InPin, LinkedPin);
	if (NewFilteredPermutations.Num() == FilteredPermutations.Num())
	{
		return false;
	}

	return true;
}

bool URigVMTemplateNode::UpdateFilteredPermutations(URigVMPin* InPin, URigVMPin* LinkedPin)
{
	ensureMsgf(InPin->GetNode() == this, TEXT("Updating filtered permutations of %s with pin from another node %s"), *GetNodePath(), *InPin->GetPinPath(true));
	ensureMsgf(LinkedPin->GetNode() != this, TEXT("Updating filtered permutations of %s with linked pin from same node %s"), *GetNodePath(), *LinkedPin->GetPinPath(true));

	const TArray<int32> NewFilteredPermutations = GetNewFilteredPermutations(InPin, LinkedPin);	
	if (NewFilteredPermutations.Num() == FilteredPermutations.Num())
	{
		return false;
	}

	if (NewFilteredPermutations.IsEmpty())
	{
		return false;
	}
	
	FilteredPermutations = NewFilteredPermutations;
	return true;
}

bool URigVMTemplateNode::UpdateFilteredPermutations(URigVMPin* InPin, const TArray<TRigVMTypeIndex>& InTypeIndices)
{
	const TArray<int32> NewFilteredPermutations = GetNewFilteredPermutations(InPin, InTypeIndices);
	if (NewFilteredPermutations.Num() == FilteredPermutations.Num())
	{
		return false;
	}

	if (NewFilteredPermutations.IsEmpty())
	{
		return false;
	}
	
	FilteredPermutations = NewFilteredPermutations;
	return true;
}

void URigVMTemplateNode::InvalidateCache()
{
	Super::InvalidateCache();
	
	CachedFunction = nullptr;
	CachedTemplate = nullptr;

	if (HasWildCardPin())
	{
		ResolvedFunctionName.Reset();
	}
}

void URigVMTemplateNode::InitializeFilteredPermutations()
{
	if (const FRigVMTemplate* Template = GetTemplate())
	{
		if (!PreferredPermutationPairs.IsEmpty())
		{
			FilteredPermutations = FindPermutationsForTypes(PreferredPermutationPairs, true);
		}
		else
		{
			FilteredPermutations.SetNumUninitialized(Template->NumPermutations());
			for (int32 i=0; i<FilteredPermutations.Num(); ++i)
			{
				FilteredPermutations[i] = i;
			}
		}
	}	
}

void URigVMTemplateNode::InitializeFilteredPermutationsFromTypes(bool bAllowCasting)
{
	if (IsSingleton())
	{
		return;
	}

	if (IsA<URigVMFunctionEntryNode>() || IsA<URigVMFunctionReturnNode>())
	{
		TArray<FRigVMTemplatePreferredType> ArgTypes;
		for (URigVMPin* Pin : GetPins())
		{
			if (!Pin->IsWildCard())
			{
				ArgTypes.Emplace(Pin->GetFName(), Pin->GetTypeIndex());
			}
		}
		
		PreferredPermutationPairs = ArgTypes;
	}
	else
	{
		if (const FRigVMTemplate* Template = GetTemplate())
		{
			TArray<FRigVMTemplatePreferredType> ArgTypes;
			for (int32 ArgIndex = 0; ArgIndex < Template->NumArguments(); ++ArgIndex)
			{
				const FRigVMTemplateArgument* Argument = Template->GetArgument(ArgIndex);
				if (URigVMPin* Pin = FindPin(Argument->GetName().ToString()))
				{
					if (!Pin->IsWildCard())
					{
						ArgTypes.Emplace(Argument->GetName(), Pin->GetTypeIndex());
					}
				}
			}

			const TArray<int32> Permutations = FindPermutationsForTypes(ArgTypes, bAllowCasting);
			if (!Permutations.IsEmpty())
			{
				FilteredPermutations = Permutations;
				PreferredPermutationPairs = ArgTypes;
			}
			else
			{
				InitializeFilteredPermutations();
			}
		}
	}
}

