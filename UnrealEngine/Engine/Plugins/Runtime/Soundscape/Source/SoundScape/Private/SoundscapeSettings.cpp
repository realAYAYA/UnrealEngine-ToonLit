// Copyright Epic Games, Inc. All Rights Reserved.


#include "SoundscapeSettings.h"

USoundscapeSettings::USoundscapeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#if WITH_EDITOR
void USoundscapeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif