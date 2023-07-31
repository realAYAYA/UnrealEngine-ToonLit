// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRW.h"
#include "ClearQuad.h"
#include "NiagaraComponent.h"
#include "Niagara/Private/NiagaraStats.h"
#include "NiagaraDataInterfaceGrid2DCollection.h"

#include "NiagaraDataInterfaceGrid2DCollectionReader.generated.h"

class FNiagaraSystemInstance;
class UTextureRenderTarget;
class UTextureRenderTargetVolume;

UCLASS(EditInlineNew, Category = "Grid", hidecategories = (Grid2DCollection, Grid, Deprecated), meta = (DisplayName = "Grid2D Collection Reader", Experimental), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceGrid2DCollectionReader : public UNiagaraDataInterfaceGrid2DCollection
{
	GENERATED_UCLASS_BODY()

public:

	// Name of the emitter to read from
	UPROPERTY(EditAnywhere, Category = "Reader")
	FString EmitterName;

	// Name of the Grid2DCollection Data Interface on the emitter
	UPROPERTY(EditAnywhere, Category = "Reader")
	FString DIName;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void GetEmitterDependencies(UNiagaraSystem* Asset, TArray<FVersionedNiagaraEmitter>& Dependencies) const override;
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;

protected:
	//~ UNiagaraDataInterface interface
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	//~ UNiagaraDataInterface interface END
};