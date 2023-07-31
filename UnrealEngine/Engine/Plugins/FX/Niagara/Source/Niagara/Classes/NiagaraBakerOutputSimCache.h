// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraBakerOutput.h"
#include "NiagaraSimCache.h"
#include "NiagaraBakerOutputSimCache.generated.h"

UCLASS(meta = (DisplayName = "Simulation Cache Output (Experimental)"))
class NIAGARA_API UNiagaraBakerOutputSimCache : public UNiagaraBakerOutput
{
	GENERATED_BODY()

public:
	UNiagaraBakerOutputSimCache(const FObjectInitializer& Init)
	{
	}

	UPROPERTY(EditAnywhere, Category = "Settings")
	FString SimCacheAssetPathFormat = TEXT("{AssetFolder}/{AssetName}_SimCache_{OutputName}");

	UPROPERTY(EditAnywhere, Category = "Settings")
	FNiagaraSimCacheCreateParameters CreateParameters;

	virtual bool Equals(const UNiagaraBakerOutput& Other) const override;

#if WITH_EDITOR
	FString MakeOutputName() const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
