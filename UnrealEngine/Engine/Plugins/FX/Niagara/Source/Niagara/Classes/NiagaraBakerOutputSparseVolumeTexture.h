// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraBakerOutput.h"
#include "NiagaraBakerOutputSparseVolumeTexture.generated.h"

UCLASS(meta = (DisplayName = "SparseVolume Texture Output"), MinimalAPI)
class UNiagaraBakerOutputSparseVolumeTexture : public UNiagaraBakerOutput
{
	GENERATED_BODY()

public:
	UNiagaraBakerOutputSparseVolumeTexture(const FObjectInitializer& Init)	
	{
	}

	UPROPERTY(EditAnywhere, Category = "Settings")
	FNiagaraBakerTextureSource SourceBinding;

	/**
	When enabled a volume atlas is created, the atlas is along X & Y not Z based on baker settings.
	*/
	UPROPERTY(EditAnywhere, Category = "Settings")
	FString SparseVolumeTextureAssetPathFormat = TEXT("{AssetFolder}/{AssetName}_BakedSVT_{OutputName}");

	NIAGARA_API virtual bool Equals(const UNiagaraBakerOutput& Other) const override;

#if WITH_EDITOR
	NIAGARA_API FString MakeOutputName() const override;
#endif

#if WITH_EDITORONLY_DATA
	NIAGARA_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
