// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRigDefines.h"
#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigUnitContext.h"
#include "ControlRig.h"
#include "RigDispatchFactory.generated.h"

/** Base class for all rig dispatch factories */
USTRUCT(BlueprintType, meta=(Abstract, BlueprintInternalUseOnlyHierarchical))
struct CONTROLRIG_API FRigDispatchFactory : public FRigVMDispatchFactory
{
	GENERATED_BODY()

	FORCEINLINE virtual UScriptStruct* GetExecuteContextStruct() const override
	{
		return FControlRigExecuteContext::StaticStruct();
	}

	FORCEINLINE virtual void RegisterDependencyTypes() const override
	{
		FRigVMRegistry::Get().FindOrAddType(FControlRigExecuteContext::StaticStruct());
		FRigVMRegistry::Get().FindOrAddType(FRigElementKey::StaticStruct());
    	FRigVMRegistry::Get().FindOrAddType(FCachedRigElement::StaticStruct());
	}

	FORCEINLINE virtual TArray<TPair<FName,FString>> GetOpaqueArguments() const override
	{
		static const TArray<TPair<FName,FString>> OpaqueArguments = {
			TPair<FName,FString>(TEXT("Context"), TEXT("const FRigUnitContext&"))
		};
		return OpaqueArguments;
	}

#if WITH_EDITOR

	virtual FString GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;

#endif

	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	FORCEINLINE static FString GetDefaultValueForStruct(const T& InValue)
	{
		static FString ValueString;
		if(ValueString.IsEmpty())
		{
			TBaseStructure<T>::Get()->ExportText(ValueString, &InValue, &InValue, nullptr, PPF_None, nullptr);
		}
		return ValueString;
	}

	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type * = nullptr
	>
	FORCEINLINE static FString GetDefaultValueForStruct(const T& InValue)
	{
		static FString ValueString;
		if(ValueString.IsEmpty())
		{
			T::StaticStruct()->ExportText(ValueString, &InValue, &InValue, nullptr, PPF_None, nullptr);
		}
		return ValueString;
	}

	FORCEINLINE static const FRigUnitContext& GetRigUnitContext(const FRigVMExtendedExecuteContext& InContext)
	{
		return *(const FRigUnitContext*)InContext.OpaqueArguments[0];
	}

#if WITH_EDITOR
	FORCEINLINE bool CheckArgumentType(bool bCondition, const FName& InArgumentName) const
	{
		if(!bCondition)
		{
			UE_LOG(LogControlRig, Error, TEXT("Fatal: '%s' Argument '%s' has incorrect type."), *GetFactoryName().ToString(), *InArgumentName.ToString())
		}
		return bCondition;
	}
#endif
};

