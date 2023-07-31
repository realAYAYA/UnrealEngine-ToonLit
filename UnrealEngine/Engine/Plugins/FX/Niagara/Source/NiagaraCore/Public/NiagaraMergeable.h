// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraMergeable.generated.h"

UCLASS()
class NIAGARACORE_API UNiagaraMergeable : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnChanged);
#endif

public:
	UNiagaraMergeable();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);

	bool Equals(const UNiagaraMergeable* Other);

	FOnChanged& OnChanged();

	FGuid GetMergeId();

protected:
	UNiagaraMergeable* StaticDuplicateWithNewMergeIdInternal(UObject* InOuter) const;
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