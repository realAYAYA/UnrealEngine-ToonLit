// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMDispatchFactory)

const FString FRigVMDispatchFactory::DispatchPrefix = TEXT("DISPATCH_");

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

#endif

FRigVMFunctionPtr FRigVMDispatchFactory::GetDispatchFunction(const FRigVMTemplateTypeMap& InTypes) const
{
	const FString PermutationName = GetPermutationNameImpl(InTypes);
	if(const FRigVMFunction* ExistingFunction = FRigVMRegistry::Get().FindFunction(*PermutationName))
	{
		return ExistingFunction->FunctionPtr;
	}
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
	const FString TypePairStrings = FRigVMTemplate::GetStringFromArgumentTypes(InTypes);
	static constexpr TCHAR Format[] = TEXT("%s::%s");
	return FString::Printf(Format, *GetFactoryName().ToString(), *TypePairStrings);
}

const FRigVMTemplate* FRigVMDispatchFactory::GetTemplate() const
{
	static bool bIsDispatchingTemplate = false;
	if(bIsDispatchingTemplate)
	{
			return nullptr;
	}
	TGuardValue<bool> ReEntryGuard(bIsDispatchingTemplate, true);
	
	if(CachedTemplate)
	{
		return CachedTemplate;
	}
	
	FRigVMRegistry& Registry = FRigVMRegistry::Get();

	const FName FactoryName = GetFactoryName();
	const TArray<FRigVMTemplateArgument> Arguments = GetArguments();
	
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
		return GetDispatchFunction(InTypes);
	});

	CachedTemplate = Registry.GetOrAddTemplateFromArguments(GetFactoryName(), Arguments, Delegates); 
	return CachedTemplate;
}

