// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UBrushBuilder.cpp: UnrealEd brush builder.
=============================================================================*/

#include "Engine/BrushBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BrushBuilder)

/*-----------------------------------------------------------------------------
	UBrushBuilder.
-----------------------------------------------------------------------------*/
UBrushBuilder::UBrushBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BitmapFilename = TEXT("BBGeneric");
	ToolTip = TEXT("BrushBuilderName_Generic");
	NotifyBadParams = true;
}

