// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterBinding.h"
#include "NiagaraConstants.h"
#include "NiagaraDataInterface.h"

#if WITH_EDITORONLY_DATA
bool FNiagaraParameterBinding::CanBindTo(FNiagaraTypeDefinition TypeDefinition) const
{
	if (AllowedTypeDefinitions.Contains(TypeDefinition))
	{
		return true;
	}

	if (UClass* TypeClass = TypeDefinition.GetClass())
	{
		if (TypeClass->IsChildOf(UNiagaraDataInterface::StaticClass()))
		{
			for (UClass* AllowedClass : AllowedDataInterfaces)
			{
				if (AllowedClass && TypeClass->IsChildOf(AllowedClass))
				{
					return true;
				}
			}

			for (UClass* AllowedInterface : AllowedInterfaces)
			{
				if (AllowedInterface && TypeClass->ImplementsInterface(AllowedInterface))
				{
					return true;
				}
			}
		}
		else
		{
			for (UClass* AllowedClass : AllowedObjects)
			{
				if (AllowedClass && TypeClass->IsChildOf(AllowedClass))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FNiagaraParameterBinding::CanBindTo(FNiagaraVariableBase InVariable, FNiagaraVariableBase& OutAliasedVariable, FStringView EmitterName) const
{
	OutAliasedVariable = InVariable;
	if ( !AllowStaticVariables() && InVariable.GetType().IsStatic() )
	{
		return false;
	}

	if ( !CanBindTo(InVariable.GetType()) )
	{
		return false;
	}

	if (InVariable.IsInNameSpace(FNiagaraConstants::ParticleAttributeNamespaceString))
	{
		return AllowParticleParameters();
	}
	else if (InVariable.IsInNameSpace(EmitterName))
	{
		if ( AllowEmitterParameters() )
		{
			OutAliasedVariable.ReplaceRootNamespace(EmitterName, FNiagaraConstants::EmitterNamespaceString);
			return true;
		}
	}
	else if (InVariable.IsInNameSpace(FNiagaraConstants::SystemNamespaceString))
	{
		return AllowSystemParameters();
	}

	return false;
}

void FNiagaraParameterBinding::OnRenameEmitter(FStringView EmitterName)
{
	if (AliasedParameter.IsInNameSpace(FNiagaraConstants::EmitterNamespaceString))
	{
		Parameter = AliasedParameter;
		Parameter.ReplaceRootNamespace(FNiagaraConstants::EmitterNamespaceString, EmitterName);
	}
}

void FNiagaraParameterBinding::OnRenameVariable(const FNiagaraVariableBase& OldVariable, const FNiagaraVariableBase& NewVariable, FStringView EmitterName)
{
	if (AliasedParameter.GetName() == OldVariable.GetName() && AliasedParameter.GetType() == OldVariable.GetType() && Parameter.IsInNameSpace(EmitterName))
	{
		AliasedParameter = NewVariable;
		Parameter = AliasedParameter;
		Parameter.ReplaceRootNamespace(FNiagaraConstants::EmitterNamespaceString, EmitterName);
	}
}

void FNiagaraParameterBinding::OnRemoveVariable(const FNiagaraVariableBase& OldVariable, FStringView EmitterName)
{
	if (AliasedParameter.GetName() == OldVariable.GetName() && AliasedParameter.GetType() == OldVariable.GetType() && Parameter.IsInNameSpace(EmitterName))
	{
		AliasedParameter = FNiagaraVariableBase();
		Parameter = FNiagaraVariableBase();
	}
}
#endif
