// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraBakerOutputSparseVolumeTexture.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraBakerOutputSparseVolumeTexture)

bool UNiagaraBakerOutputSparseVolumeTexture::Equals(const UNiagaraBakerOutput& OtherBase) const
{
	const UNiagaraBakerOutputSparseVolumeTexture& Other = *CastChecked<UNiagaraBakerOutputSparseVolumeTexture>(&OtherBase);
	return
		Super::Equals(Other) &&
		SourceBinding.SourceName == Other.SourceBinding.SourceName;
}

#if WITH_EDITOR
FString UNiagaraBakerOutputSparseVolumeTexture::MakeOutputName() const
{
	return FString::Printf(TEXT("SparseVolume_%d"), GetFName().GetNumber());
}
#endif

#if WITH_EDITORONLY_DATA
void UNiagaraBakerOutputSparseVolumeTexture::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

}
#endif

