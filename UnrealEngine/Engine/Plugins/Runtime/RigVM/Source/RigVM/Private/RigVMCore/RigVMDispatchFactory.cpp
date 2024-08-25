// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVMMemoryStorage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMDispatchFactory)

const FString FRigVMDispatchFactory::DispatchPrefix = TEXT("DISPATCH_");
const FString FRigVMDispatchFactory::TrueString = TEXT("True");
FCriticalSection FRigVMDispatchFactory::GetTemplateMutex;

FName FRigVMDispatchFactory::GetFactoryName() const
{
	static constexpr TCHAR Format[] = TEXT("%s%s");
	return *FString::Printf(Format, *DispatchPrefix, *GetScriptStruct()->GetName());
}

#if WITH_EDITOR

FName FRigVMDispatchFactory::GetNextAggregateName(const FName& InLastAggregatePinName) const
{
	return FRigVMStruct().GetNextAggregateName(InLastAggregatePinName);
}

FName FRigVMDispatchFactory::GetDisplayNameForArgument(const FName& InArgumentName) const
{
	if(InArgumentName == FRigVMStruct::ExecuteContextName)
	{
		return FRigVMStruct::ExecuteName;
	}
	return NAME_None;
}

FString FRigVMDispatchFactory::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if(InArgumentName == FRigVMStruct::ControlFlowBlockToRunName &&
		InMetaDataKey == FRigVMStruct::SingletonMetaName)
	{
		return TrueString;
	}
	return FString();
}

FLinearColor FRigVMDispatchFactory::GetNodeColor() const
{
	if(const UScriptStruct* ScriptStruct = GetScriptStruct())
	{
		FString NodeColor;
		if (ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::NodeColorMetaName, &NodeColor))
		{
			return FRigVMTemplate::GetColorFromMetadata(NodeColor);
		}
	}
	return FLinearColor::White;
}

FText FRigVMDispatchFactory::GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const
{
	return GetScriptStruct()->GetToolTipText();
}

FString FRigVMDispatchFactory::GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if(FRigVMRegistry::Get().IsArrayType(InTypeIndex))
	{
		static const FString EmptyArrayString = TEXT("()");
		return EmptyArrayString;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::Bool)
	{
		static const FString FalseString = TEXT("False");
		return FalseString;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::Float || InTypeIndex == RigVMTypeUtils::TypeIndex::Double)
	{
		static const FString ZeroString = TEXT("0.000000");
		return ZeroString;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		static const FString ZeroString = TEXT("0");
		return ZeroString;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::FName)
	{
		return FName(NAME_None).ToString();
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FVector2D>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FVector2D::ZeroVector);
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FVector>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FVector::ZeroVector); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FRotator>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FRotator::ZeroRotator); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FQuat>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FQuat::Identity); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FTransform>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FTransform::Identity); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FLinearColor>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FLinearColor::White); 
		return DefaultValueString;
	}
	return FString();
}

FString FRigVMDispatchFactory::GetCategory() const
{
	if(const UScriptStruct* ScriptStruct = GetScriptStruct())
	{
		FString Category;
		if (ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::CategoryMetaName, &Category))
		{
			return Category;
		}
	}
	return FString();
}

FString FRigVMDispatchFactory::GetKeywords() const
{
	if(const UScriptStruct* ScriptStruct = GetScriptStruct())
	{
		FString Keywords;
		if (ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::KeywordsMetaName, &Keywords))
		{
			return Keywords;
		}
	}
	return FString();
}

bool FRigVMDispatchFactory::IsLazyInputArgument(const FName& InArgumentName) const
{
	return HasArgumentMetaData(InArgumentName, FRigVMStruct::ComputeLazilyMetaName);
}

#endif

FName FRigVMDispatchFactory::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	check(GetArgumentInfos().Num() == InTotalOperands);
	return GetArgumentInfos()[InOperandIndex].Name;
}

const TArray<FName>& FRigVMDispatchFactory::GetControlFlowBlocks(const FRigVMDispatchContext& InContext) const
{
	const TArray<FName>& Blocks = GetControlFlowBlocks_Impl(InContext);
#if WITH_EDITOR
	FRigVMStruct::ValidateControlFlowBlocks(Blocks);
#endif
	return Blocks;
}

bool FRigVMDispatchFactory::SupportsExecuteContextStruct(const UScriptStruct* InExecuteContextStruct) const
{
	return InExecuteContextStruct->IsChildOf(GetExecuteContextStruct());
}

const TArray<FName>& FRigVMDispatchFactory::GetControlFlowBlocks_Impl(const FRigVMDispatchContext& InContext) const
{
	static const TArray<FName> EmptyArray;
	return EmptyArray;
}

const TArray<FName>* FRigVMDispatchFactory::UpdateArgumentNameCache(int32 InNumberOperands) const
{
	FScopeLock ArgumentNamesLock(ArgumentNamesMutex);
	return UpdateArgumentNameCache_NoLock(InNumberOperands);
}

const TArray<FName>* FRigVMDispatchFactory::UpdateArgumentNameCache_NoLock(int32 InNumberOperands) const
{
	TSharedPtr<TArray<FName>>& ArgumentNames = ArgumentNamesMap.FindOrAdd(InNumberOperands);
	if(!ArgumentNames.IsValid())
	{
		ArgumentNames = MakeShareable(new TArray<FName>()); 
		ArgumentNames->Reserve(InNumberOperands);
		for(int32 OperandIndex = 0; OperandIndex < InNumberOperands; OperandIndex++)
		{
			ArgumentNames->Add(GetArgumentNameForOperandIndex(OperandIndex, InNumberOperands));
		}
	}
	return ArgumentNames.Get();
}

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatchFactory::GetArgumentInfos() const
{
	static const TArray<FRigVMTemplateArgumentInfo> EmptyArguments;
	return EmptyArguments;
}

TArray<FRigVMExecuteArgument> FRigVMDispatchFactory::GetExecuteArguments(const FRigVMDispatchContext& InContext) const
{
	TArray<FRigVMExecuteArgument> Arguments = GetExecuteArguments_Impl(InContext);
	for(FRigVMExecuteArgument& Argument : Arguments)
	{
		if(Argument.TypeIndex != INDEX_NONE && FRigVMRegistry::Get().IsArrayType(Argument.TypeIndex))
		{
			Argument.TypeIndex = RigVMTypeUtils::TypeIndex::ExecuteArray;
		}
		else
		{
			Argument.TypeIndex = RigVMTypeUtils::TypeIndex::Execute;
		}
	}
	return Arguments;
}

const TArray<FRigVMExecuteArgument>& FRigVMDispatchFactory::GetExecuteArguments_Impl(
	const FRigVMDispatchContext& InContext) const
{
	static TArray<FRigVMExecuteArgument> EmptyArguments;
	return EmptyArguments;
}

FRigVMFunctionPtr FRigVMDispatchFactory::GetOrCreateDispatchFunction(const FRigVMTemplateTypeMap& InTypes) const
{
	FScopeLock DispatchFunctionScopeLock(&FRigVMRegistry::DispatchFunctionMutex);

	const FString PermutationName = GetPermutationNameImpl(InTypes);
	if(const FRigVMFunction* ExistingFunction = FRigVMRegistry::Get().FindFunction(*PermutationName))
	{
		return ExistingFunction->FunctionPtr;
	}
	
	return CreateDispatchFunction_NoLock(InTypes);
}

FRigVMFunctionPtr FRigVMDispatchFactory::CreateDispatchFunction(const FRigVMTemplateTypeMap& InTypes) const
{
	FScopeLock DispatchFunctionScopeLock(&FRigVMRegistry::DispatchFunctionMutex);
	return CreateDispatchFunction_NoLock(InTypes);
}

FRigVMFunctionPtr FRigVMDispatchFactory::CreateDispatchFunction_NoLock(const FRigVMTemplateTypeMap& InTypes) const
{
	return GetDispatchFunctionImpl(InTypes);
}

TArray<FRigVMFunction> FRigVMDispatchFactory::CreateDispatchPredicates(const FRigVMTemplateTypeMap& InTypes) const
{
	FScopeLock DispatchPredicatesScopeLock(&FRigVMRegistry::DispatchPredicatesMutex);
	return CreateDispatchPredicates_NoLock(InTypes);
}

TArray<FRigVMFunction> FRigVMDispatchFactory::CreateDispatchPredicates_NoLock(const FRigVMTemplateTypeMap& InTypes) const
{
	return GetDispatchPredicatesImpl(InTypes);
}

FString FRigVMDispatchFactory::GetPermutationName(const FRigVMTemplateTypeMap& InTypes) const
{
#if WITH_EDITOR
	const TArray<FRigVMTemplateArgumentInfo>& Arguments = GetArgumentInfos();
	
	checkf(InTypes.Num() == Arguments.Num(), TEXT("Failed getting permutation names for '%s' "), *GetFactoryName().ToString());
	
	for(const FRigVMTemplateArgumentInfo& Argument : Arguments)
	{
		check(InTypes.Contains(Argument.Name));
	}
#endif
	return GetPermutationNameImpl(InTypes);
}

TArray<FRigVMTemplateArgumentInfo> FRigVMDispatchFactory::BuildArgumentListFromPrimaryArgument(const TArray<FRigVMTemplateArgumentInfo>& InInfos, const FName& InPrimaryArgumentName) const
{
	TArray<FRigVMTemplateArgumentInfo> NewInfos;
	
	const FRigVMTemplateArgumentInfo* PrimaryInfo = InInfos.FindByPredicate([InPrimaryArgumentName](const FRigVMTemplateArgumentInfo& Arg)
	{
		return Arg.Name == InPrimaryArgumentName;
	});

	if (!PrimaryInfo)
	{
		return NewInfos;
	}

	const int32 NumInfos = InInfos.Num();
	TArray< TArray<TRigVMTypeIndex> > TypeIndicesArray;
	TypeIndicesArray.SetNum(NumInfos);

	const FRigVMTemplateArgument PrimaryArgument = PrimaryInfo->GetArgument();
	bool bFoundArg = true;
	PrimaryArgument.ForEachType([&](const TRigVMTypeIndex Type)
	{
		if (bFoundArg)
		{
			TArray<FRigVMTemplateTypeMap, TInlineAllocator<1>> Permutations;
			GetPermutationsFromArgumentType(InPrimaryArgumentName, Type, Permutations);
			for (const FRigVMTemplateTypeMap& Permutation : Permutations)
			{
				for (int32 Index=0; Index < InInfos.Num(); ++Index)
				{
					if (const TRigVMTypeIndex* PermutationArg = Permutation.Find(InInfos[Index].Name))
					{
						TypeIndicesArray[Index].Add(*PermutationArg);
					}
					else
					{
						bFoundArg = false;
						break;
					}
				}
				if (!bFoundArg)
				{
					break;
				}
			}
		}
		return true;
	});

	if (!bFoundArg)
	{
		return NewInfos;	
	}

	NewInfos.Reserve(NumInfos);
	for (int32 Index=0; Index < NumInfos; ++Index)
	{
		const FRigVMTemplateArgument Argument = InInfos[Index].GetArgument();
		const TArray<TRigVMTypeIndex>& TypeIndices = TypeIndicesArray[Index];
		if (TypeIndices.IsEmpty())
		{
			NewInfos.Emplace(InInfos[Index].Name, Argument.Direction, Argument.TypeCategories);
		}
		else
		{
			NewInfos.Emplace(InInfos[Index].Name, Argument.Direction, TypeIndices);
		}
	}
	
	return NewInfos;
}

FString FRigVMDispatchFactory::GetPermutationNameImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	static constexpr TCHAR Format[] = TEXT("%s::%s");
	const FString TypePairStrings = FRigVMTemplate::GetStringFromArgumentTypes(InTypes);
	return FString::Printf(Format, *GetFactoryName().ToString(), *TypePairStrings);
}

bool FRigVMDispatchFactory::CopyProperty(const FProperty* InTargetProperty, uint8* InTargetPtr,
                                         const FProperty* InSourceProperty, const uint8* InSourcePtr)
{
	return URigVMMemoryStorage::CopyProperty(InTargetProperty, InTargetPtr, InSourceProperty, InSourcePtr);
}

const FRigVMTemplate* FRigVMDispatchFactory::GetTemplate() const
{
	// make sure to rely on the instance of this factory that's stored under the registry
	FRigVMRegistry& Registry = FRigVMRegistry::Get();
	const FName FactoryName = GetFactoryName();
	const FRigVMDispatchFactory* ThisFactory = Registry.FindDispatchFactory(FactoryName);
	if(ThisFactory != this)
	{
		return ThisFactory->GetTemplate();
	}

	FScopeLock GetTemplateScopeLock(&GetTemplateMutex);
	
	if(CachedTemplate)
	{
		return CachedTemplate;
	}

	
	// we don't allow execute types on arguments	
	const TArray<FRigVMTemplateArgumentInfo>& Infos = GetArgumentInfos();
	for (const FRigVMTemplateArgumentInfo& Info : Infos)
	{
		const FRigVMTemplateArgument Argument = Info.GetArgument();
		const int32 Index = Argument.IndexOfByPredicate([&](const TRigVMTypeIndex TypeIndex)
		{
			return Registry.IsExecuteType(TypeIndex);
		});
		
		if (Index != INDEX_NONE)
		{
			UE_LOG(LogRigVM, Error, TEXT("Failed to add template for dispatch '%s'. Argument '%s' is an execute type."),
				*FactoryName.ToString(), *Info.Name.ToString());
			return nullptr;			
		}
	}

	FRigVMTemplateDelegates Delegates;
	Delegates.GetDispatchFactoryDelegate = FRigVMTemplate_GetDispatchFactoryDelegate::CreateLambda(
	[FactoryName]()
	{
		return FRigVMRegistry::Get().FindDispatchFactory(FactoryName);
	});

	CachedTemplate = Registry.AddTemplateFromArguments(GetFactoryName(), Infos, Delegates); 
	return CachedTemplate;
}

FName FRigVMDispatchFactory::GetTemplateNotation() const
{
	if(const FRigVMTemplate* Template = GetTemplate())
	{
		return Template->GetNotation();
	}
	return NAME_None;
}
