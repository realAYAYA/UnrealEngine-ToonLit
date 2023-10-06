// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWidgetFactory.h"
#include "Templates/SubclassOf.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWidgetFactory)

class UUserWidget;

TSubclassOf<UUserWidget> ULyraWidgetFactory::FindWidgetClassForData_Implementation(const UObject* Data) const
{
	return TSubclassOf<UUserWidget>();
}
