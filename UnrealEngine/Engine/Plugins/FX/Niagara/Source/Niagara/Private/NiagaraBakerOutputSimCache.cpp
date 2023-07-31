// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraBakerOutputSimCache.h"
#include "NiagaraBakerSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraBakerOutputSimCache)

bool UNiagaraBakerOutputSimCache::Equals(const UNiagaraBakerOutput& OtherBase) const
{
	const UNiagaraBakerOutputSimCache& Other = *CastChecked<UNiagaraBakerOutputSimCache>(&OtherBase);
	return
		Super::Equals(Other);
}

#if WITH_EDITOR
FString UNiagaraBakerOutputSimCache::MakeOutputName() const
{
	return FString::Printf(TEXT("SimCache_%d"), GetFName().GetNumber());
}
#endif

#if WITH_EDITORONLY_DATA
void UNiagaraBakerOutputSimCache::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

}
#endif

