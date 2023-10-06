// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraMergeable.generated.h"

UCLASS(MinimalAPI)
class UNiagaraMergeable : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnChanged);
#endif

public:
	NIAGARACORE_API UNiagaraMergeable();

#if WITH_EDITOR
	NIAGARACORE_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);

	NIAGARACORE_API bool Equals(const UNiagaraMergeable* Other);

	NIAGARACORE_API FOnChanged& OnChanged();

	NIAGARACORE_API FGuid GetMergeId() const;

protected:
	NIAGARACORE_API UNiagaraMergeable* StaticDuplicateWithNewMergeIdInternal(UObject* InOuter) const;
#endif

private:
#if WITH_EDITOR
	FOnChanged OnChangedDelegate;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid MergeId;
#endif
};
