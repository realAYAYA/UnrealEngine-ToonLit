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
	check(GetArguments().Num() == InTotalOperands);
	return GetArguments()[InOperandIndex].GetName();
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

FRigVMFunctionPtr FRigVMDispatchFactory::GetOrCreateDispatchFunction(const FRigVMTemplateTypeMap& InTypes) const
{
	FScopeLock GetTemplateScopeLock(&FRigVMRegistry::GetDispatchFunctionMutex);

	const FString PermutationName = GetPermutationNameImpl(InTypes);
	if(const FRigVMFunction* ExistingFunction = FRigVMRegistry::Get().FindFunction(*PermutationName))
	{
		return ExistingFunction->FunctionPtr;
	}
	
	return CreateDispatchFunction_NoLock(InTypes);
}

FRigVMFunctionPtr FRigVMDispatchFactory::CreateDispatchFunction(const FRigVMTemplateTypeMap& InTypes) const
{
	FScopeLock GetTemplateScopeLock(&FRigVMRegistry::GetDispatchFunctionMutex);
	return CreateDispatchFunction_NoLock(InTypes);
}

FRigVMFunctionPtr FRigVMDispatchFactory::CreateDispatchFunction_NoLock(const FRigVMTemplateTypeMap& InTypes) const
{
	return GetDispatchFunctionImpl(InTypes);
}

FString FRigVMDispatchFactory::GetPermutationName(const FRigVMTemplateTypeMap& InTypes) const
{
#if WITH_EDITOR
	const TArray<FRigVMTemplateArgument> Arguments = GetArguments();
	check(InTypes.Num() == Arguments.Num());
	for(const FRigVMTemplateArgument& Argument : Arguments)
	{
		check(InTypes.Contains(Argument.GetName()));
	}
#endif
	return GetPermutationNameImpl(InTypes);
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

	const TArray<FRigVMTemplateArgument> Arguments = GetArguments();

	// we don't allow execute types on arguments
	for(const FRigVMTemplateArgument& Argument : Arguments)
	{
		for(const TRigVMTypeIndex& TypeIndex : Argument.GetTypeIndices())
		{
			if(Registry.IsExecuteType(TypeIndex))
			{
				UE_LOG(LogRigVM, Error, TEXT("Failed to add template for dispatch '%s'. Argument '%s' is an execute type."),
					*GetFactoryName().ToString(),
					*Argument.GetName().ToString());
				return nullptr;
			}
		}
	}

	FRigVMTemplateDelegates Delegates;
	Delegates.NewArgumentTypeDelegate = FRigVMTemplate_NewArgumentTypeDelegate::CreateLambda(
		[this](const FRigVMTemplate*, const FName& InArgumentName, int32 InTypeIndex)
		{
			return OnNewArgumentType(InArgumentName, InTypeIndex);
		});

	Delegates.GetDispatchFactoryDelegate = FRigVMTemplate_GetDispatchFactoryDelegate::CreateLambda(
[FactoryName]()
	{
		return FRigVMRegistry::Get().FindDispatchFactory(FactoryName);
	});

	Delegates.RequestDispatchFunctionDelegate = FRigVMTemplate_RequestDispatchFunctionDelegate::CreateLambda(
	[this](const FRigVMTemplate*, const FRigVMTemplateTypeMap& InTypes)
	{
		return CreateDispatchFunction(InTypes);
	});

	CachedTemplate = Registry.AddTemplateFromArguments(GetFactoryName(), Arguments, Delegates); 
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
