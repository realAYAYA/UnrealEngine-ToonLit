// Copyright Epic Games, Inc. All Rights Reserved.

#include "VariantManagerClipboard.h"

#include "CoreMinimal.h"


TArray<TStrongObjectPtr<UVariant>> FVariantManagerClipboard::StoredVariants = TArray<TStrongObjectPtr<UVariant>>();
TArray<TStrongObjectPtr<UVariantSet>> FVariantManagerClipboard::StoredVariantSets = TArray<TStrongObjectPtr<UVariantSet>>();
TArray<TStrongObjectPtr<UVariantObjectBinding>> FVariantManagerClipboard::StoredObjectBindings = TArray<TStrongObjectPtr<UVariantObjectBinding>>();
