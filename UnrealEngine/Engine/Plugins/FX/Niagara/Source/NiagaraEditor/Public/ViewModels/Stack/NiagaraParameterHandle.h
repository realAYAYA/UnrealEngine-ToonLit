// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

class UNiagaraNodeFunctionCall;
struct FNiagaraVariable;

class NIAGARAEDITOR_API FNiagaraParameterHandle
{
public:
	FNiagaraParameterHandle();

	FNiagaraParameterHandle(FName InParameterHandleString);

	FNiagaraParameterHandle(FName InNamespace, FName InName);

	bool operator==(const FNiagaraParameterHandle& Other) const;

	bool operator!=(const FNiagaraParameterHandle& Other) const { return (*this == Other) == false; }

	static FNiagaraParameterHandle CreateAliasedModuleParameterHandle(const FNiagaraParameterHandle& ModuleParameterHandle, const UNiagaraNodeFunctionCall* ModuleNode);
	static FNiagaraParameterHandle CreateAliasedModuleParameterHandle(const FName FullInputName, const FName FunctionName);

	static FNiagaraParameterHandle CreateEngineParameterHandle(const FNiagaraVariable& SystemVariable);

	static FNiagaraParameterHandle CreateEmitterParameterHandle(const FNiagaraVariable& EmitterVariable);

	static FNiagaraParameterHandle CreateParticleAttributeParameterHandle(const FName InName);

	static FNiagaraParameterHandle CreateModuleParameterHandle(const FName InName);

	static FNiagaraParameterHandle CreateInitialParameterHandle(const FNiagaraParameterHandle& Handle);

	bool IsValid() const;

	const FName GetParameterHandleString() const;

	const FName GetName() const;

	const FName GetNamespace() const;

	const TArray<FName> GetHandleParts() const;

	bool IsUserHandle() const;

	bool IsEngineHandle() const;

	bool IsSystemHandle() const;

	bool IsEmitterHandle() const;

	bool IsParticleAttributeHandle() const;

	bool IsModuleHandle() const;

	bool IsOutputHandle() const;

	bool IsLocalHandle() const;

	bool IsParameterCollectionHandle() const;

	bool IsReadOnlyHandle() const;

	bool IsTransientHandle() const;

	bool IsDataInstanceHandle() const;

	bool IsStackContextHandle() const;

private:
	FName ParameterHandleName;
	FName Name;
	FName Namespace;
	mutable TArray<FName> HandlePartsCache;
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraParameterHandle& Var)
{
	return GetTypeHash(Var.GetParameterHandleString());
}
