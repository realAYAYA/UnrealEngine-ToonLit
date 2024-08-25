// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRW.h"
#include "ClearQuad.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceGrid2DCollection.h"

#include "NiagaraDataInterfaceGrid2DCollectionReader.generated.h"

class FNiagaraSystemInstance;
class UTextureRenderTarget;
class UTextureRenderTargetVolume;

UCLASS(EditInlineNew, Category = "Grid", CollapseCategories, hidecategories = (Grid2DCollection, Grid, Deprecated), meta = (DisplayName = "Grid2D Collection Reader", Experimental), Blueprintable, BlueprintType, MinimalAPI)
class UNiagaraDataInterfaceGrid2DCollectionReader : public UNiagaraDataInterfaceGrid2DCollection
{
	GENERATED_UCLASS_BODY()

public:

	// Name of the emitter to read from
	UPROPERTY(EditAnywhere, Category = "Reader")
	FString EmitterName;

	// Name of the Grid2DCollection Data Interface on the emitter
	UPROPERTY(EditAnywhere, Category = "Reader")
	FString DIName;

	virtual void Serialize(FArchive& Ar) override { UNiagaraDataInterfaceRWBase::Serialize(Ar); }

	NIAGARA_API virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	NIAGARA_API virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	NIAGARA_API virtual void GetEmitterDependencies(UNiagaraSystem* Asset, TArray<FVersionedNiagaraEmitter>& Dependencies) const override;
	NIAGARA_API virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;

protected:
	//~ UNiagaraDataInterface interface
#if WITH_EDITORONLY_DATA
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif
	NIAGARA_API virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	//~ UNiagaraDataInterface interface END
};
