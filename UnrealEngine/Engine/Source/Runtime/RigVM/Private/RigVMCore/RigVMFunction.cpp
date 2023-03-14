// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMFunction.h"
#include "RigVMCore/RigVMTemplate.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMExternalVariable.h"

FString FRigVMFunction::GetName() const
{
	return Name;
}

FName FRigVMFunction::GetMethodName() const
{
	FString FullName(Name);
	FString Right;
	if (FullName.Split(TEXT("::"), nullptr, &Right))
	{
		return *Right;
	}
	return NAME_None;
}

FString FRigVMFunction::GetModuleName() const
{
#if WITH_EDITOR
	if (Struct)
	{
		if (UPackage* Package = Struct->GetPackage())
		{
			return Package->GetName();
		}
	}
#endif
	return FString();
}

FString FRigVMFunction::GetModuleRelativeHeaderPath() const
{
#if WITH_EDITOR
	if (Struct)
	{
		FString ModuleRelativePath;
		if (Struct->GetStringMetaDataHierarchical(TEXT("ModuleRelativePath"), &ModuleRelativePath))
		{
			return ModuleRelativePath;
		}
	}
#endif
	return FString();
}

const TArray<TRigVMTypeIndex>& FRigVMFunction::GetArgumentTypeIndices() const
{
	if(ArgumentTypeIndices.IsEmpty() && !Arguments.IsEmpty())
	{
		if(Struct)
		{
			for(const FRigVMFunctionArgument& Argument : Arguments)
			{
				if(const FProperty* Property = Struct->FindPropertyByName(Argument.Name))
				{
					FName CPPType = NAME_None;
					UObject* CPPTypeObject = nullptr;
					FRigVMExternalVariable::GetTypeFromProperty(Property, CPPType, CPPTypeObject);

					const FRigVMTemplateArgumentType Type(CPPType, CPPTypeObject);
					ArgumentTypeIndices.Add(FRigVMRegistry::Get().FindOrAddType(Type));
				}
			}
		}
		else if(const FRigVMTemplate* Template = GetTemplate())
		{
			const int32 PermutationIndex = Template->FindPermutation(this);
			check(PermutationIndex != INDEX_NONE);

			for(const FRigVMFunctionArgument& FunctionArgument : Arguments)
			{
				const FRigVMTemplateArgument* TemplateArgument = Template->FindArgument(FunctionArgument.Name);
				check(TemplateArgument);
				ArgumentTypeIndices.Add(TemplateArgument->GetTypeIndices()[PermutationIndex]);
			}
		}
		else
		{
			checkNoEntry();
		}
	}
	return ArgumentTypeIndices;
}

bool FRigVMFunction::IsAdditionalArgument(const FRigVMFunctionArgument& InArgument) const
{
#if WITH_EDITOR
	if (Struct)
	{
		return Struct->FindPropertyByName(InArgument.Name) == nullptr;
	}
#endif
	return false;
}

const FRigVMTemplate* FRigVMFunction::GetTemplate() const
{
	if(TemplateIndex == INDEX_NONE)
	{
		return nullptr;
	}

	const FRigVMTemplate* Template = &FRigVMRegistry::Get().GetTemplates()[TemplateIndex];
	if(Template->NumPermutations() <= 1)
	{
		return nullptr;
	}

	return Template;
}
