// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "NiagaraCore.generated.h"

typedef uint64 FNiagaraSystemInstanceID;

UENUM()
enum class ENiagaraIterationSource : uint8
{
	/** Iterate over all active particles. */
	Particles = 0,
	/** Iterate over all elements in the data interface. */
	DataInterface,
	/** Iterate over a user provided number of elements. */
	DirectSet,
};

/** A utility class allowing for references to FNiagaraVariableBase outside of the Niagara module. */
USTRUCT()
struct FNiagaraVariableCommonReference
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	TObjectPtr<UObject> UnderlyingType;

	NIAGARACORE_API bool Serialize(FArchive& Ar);
	friend bool operator<<(FArchive& Ar, FNiagaraVariableCommonReference& VariableReference);

	bool operator==(const FNiagaraVariableCommonReference& Other)const
	{
		return Name == Other.Name && UnderlyingType == Other.UnderlyingType;
	}	
};

template<> struct TStructOpsTypeTraits<FNiagaraVariableCommonReference> : public TStructOpsTypeTraitsBase2<FNiagaraVariableCommonReference>
{
	enum
	{
		WithSerializer = true,
	};
};

inline bool operator<<(FArchive& Ar, FNiagaraVariableCommonReference& VariableReference)
{
	return VariableReference.Serialize(Ar);
}
