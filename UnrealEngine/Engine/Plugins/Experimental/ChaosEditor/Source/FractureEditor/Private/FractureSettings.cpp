// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FractureSettings)

#if WITH_EDITOR

UFractureSettings::UFractureSettings(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, ExplodeAmount(0.0)
	, FractureLevel(-1)
	, bHideUnselected(false)
{}

#endif



