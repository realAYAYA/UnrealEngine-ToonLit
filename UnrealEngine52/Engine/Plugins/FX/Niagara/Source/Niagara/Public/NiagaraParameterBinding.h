// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EnumClassFlags.h"

#include "NiagaraCommon.h"
#include "NiagaraParameterBinding.generated.h"

// Binding usage flags
// Note that Emitter / Particle bindings are only allowed if the binding variable is outered to a UNiagaraEmitter
enum class ENiagaraParameterBindingUsage
{
	User		= 1 << 0,		// Can we bind to user parameters
	System		= 1 << 1,		// Can we bind to system parameters
	Emitter		= 1 << 2,		// Can we bind to emitter parameters
	Particle	= 1 << 3,		// Can we bind to particle parameters
	StaticVariable = 1 << 4,	// Can we bind to static variables

	NotParticle	= StaticVariable | User | System | Emitter,
	All			= StaticVariable | User | System | Emitter | Particle,
};

ENUM_CLASS_FLAGS(ENiagaraParameterBindingUsage)

USTRUCT()
struct NIAGARA_API FNiagaraParameterBinding
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITORONLY_DATA
	bool CanBindTo(FNiagaraTypeDefinition TypeDefinition) const;
	bool CanBindTo(FNiagaraVariableBase InVariable, FNiagaraVariableBase& OutAliasedVariable, FStringView EmitterName) const;

	void OnRenameEmitter(FStringView EmitterName);
	void OnRenameVariable(const FNiagaraVariableBase& OldVariable, const FNiagaraVariableBase& NewVariable, FStringView EmitterName);
	void OnRemoveVariable(const FNiagaraVariableBase& OldVariable, FStringView EmitterName);

	void SetUsage(ENiagaraParameterBindingUsage InUsage) { BindingUsage = InUsage; }
	void SetAllowedDataInterfaces(TArray<UClass*> InClasses) { AllowedDataInterfaces = InClasses; }
	void SetAllowedObjects(TArray<UClass*> InClasses) { AllowedObjects = InClasses; }
	void SetAllowedInterfaces(TArray<UClass*> InClasses) { AllowedInterfaces = InClasses; }
	void SetAllowedTypeDefinitions(TArray<FNiagaraTypeDefinition> InTypeDefs) { AllowedTypeDefinitions = InTypeDefs; }

	bool AllowUserParameters() const { return EnumHasAllFlags(BindingUsage, ENiagaraParameterBindingUsage::User); }
	bool AllowSystemParameters() const { return EnumHasAllFlags(BindingUsage, ENiagaraParameterBindingUsage::System); }
	bool AllowEmitterParameters() const { return EnumHasAllFlags(BindingUsage, ENiagaraParameterBindingUsage::Emitter); }
	bool AllowParticleParameters() const { return EnumHasAllFlags(BindingUsage, ENiagaraParameterBindingUsage::Particle); }
	bool AllowStaticVariables() const { return EnumHasAllFlags(BindingUsage, ENiagaraParameterBindingUsage::StaticVariable); }

	TConstArrayView<UClass*> GetAllowedDataInterfaces() const { return MakeArrayView(AllowedDataInterfaces); }
	TConstArrayView<UClass*> GetAllowedObjects() const { return MakeArrayView(AllowedObjects); }
	TConstArrayView<UClass*> GetAllowedInterfaces() const { return MakeArrayView(AllowedInterfaces); }
	TConstArrayView<FNiagaraTypeDefinition> GetAllowedTypeDefinitions() const { return MakeArrayView(AllowedTypeDefinitions); }
#endif

	/** Parameter binding used by the runtime fully resolved, contains a fallback value, i.e. NamedEmitter.Parameter */
	UPROPERTY(EditAnywhere, Category = "Parameter Binding")
	FNiagaraVariableBase Parameter;

#if WITH_EDITORONLY_DATA
	/** Parameter binding used in the UI, i.e. Emitter.Parameter */
	UPROPERTY(EditAnywhere, Category = "Parameter Binding")
	FNiagaraVariableBase AliasedParameter;

protected:
	/* Set the usage for the binding. */
	ENiagaraParameterBindingUsage BindingUsage = ENiagaraParameterBindingUsage::NotParticle;

	/** List of data interfaces we can bind to, matches with a valid Cast<>. */
	UPROPERTY(transient)
	TArray<TObjectPtr<UClass>> AllowedDataInterfaces;

	/** List of UObject types we can bind to, matches with a valid Cast<>. */
	UPROPERTY(transient)
	TArray<TObjectPtr<UClass>> AllowedObjects;

	/** List of Interfaces to look for on Object & DataInterfaces */
	UPROPERTY(transient)
	TArray<TObjectPtr<UClass>> AllowedInterfaces;

	/** List of explicit type definitions allowed, must be an exact match. */
	TArray<FNiagaraTypeDefinition>	AllowedTypeDefinitions;
#endif

	//FORCEINLINE bool operator==(const FNiagaraParameterBinding& Other)const
	//{
		//return Other.Parameter == Parameter;
	//}
};