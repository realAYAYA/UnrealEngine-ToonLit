// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OptimusDataDomain.h"
#include "OptimusDataType.h"
#include "OptimusValidatedName.h"

#include "OptimusBindingTypes.generated.h"

USTRUCT()
struct UE_DEPRECATED(5.1, "Replaced with FOptimusParameterBinding") FOptimus_ShaderBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Binding)
	FName Name;

	UPROPERTY(EditAnywhere, Category = Binding, meta=(UseInResource))
	FOptimusDataTypeRef DataType;

	/** Returns true if the binding is valid and has defined entries */
	bool IsValid() const
	{
		return !Name.IsNone() && DataType.IsValid();
	}
};


USTRUCT()
struct FOptimusParameterBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=Binding)
	FOptimusValidatedName Name;

	UPROPERTY(EditAnywhere, Category = Binding, meta=(UseInResource))
	FOptimusDataTypeRef DataType;

	/** Returns true if the binding is valid and has defined entries */
	bool IsValid() const
	{
		return !Name.Name.IsNone() && DataType.IsValid();
	}
	
	UPROPERTY(EditAnywhere, Category = Binding)
	FOptimusDataDomain DataDomain;
};


USTRUCT()
struct FOptimusParameterBindingArray
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Binding)
	TArray<FOptimusParameterBinding> InnerArray;

	template <typename Predicate>
	const FOptimusParameterBinding* FindByPredicate(Predicate Pred) const
	{
		return InnerArray.FindByPredicate(Pred);
	}

	bool IsEmpty() const
	{
		return InnerArray.IsEmpty();
	}

	FOptimusParameterBinding& Last(int32 IndexFromTheEnd = 0)
	{
		return InnerArray.Last(IndexFromTheEnd);
	}

	const FOptimusParameterBinding& Last(int32 IndexFromTheEnd = 0) const
	{
		return InnerArray.Last(IndexFromTheEnd);
	}

	FOptimusParameterBindingArray& operator=(const TArray<FOptimusParameterBinding>& Rhs)
	{
		InnerArray = Rhs;
		return *this;
	}
	
	int32 Num() const { return InnerArray.Num(); }
	bool IsValidIndex(int32 Index) const { return Index < InnerArray.Num() && Index >= 0; }
	const FOptimusParameterBinding& operator[](int32 InIndex) const { return InnerArray[InIndex]; }
	FOptimusParameterBinding& operator[](int32 InIndex) { return InnerArray[InIndex]; }
	FORCEINLINE	TArray<FOptimusParameterBinding>::RangedForIteratorType      begin()       { return InnerArray.begin(); }
	FORCEINLINE	TArray<FOptimusParameterBinding>::RangedForConstIteratorType begin() const { return InnerArray.begin(); }
	FORCEINLINE	TArray<FOptimusParameterBinding>::RangedForIteratorType      end()         { return InnerArray.end();   }
	FORCEINLINE	TArray<FOptimusParameterBinding>::RangedForConstIteratorType end() const   { return InnerArray.end();   }
};

