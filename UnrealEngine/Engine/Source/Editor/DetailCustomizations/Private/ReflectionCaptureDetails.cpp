// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReflectionCaptureDetails.h"

#define LOCTEXT_NAMESPACE "ReflectionCaptureDetails"



TSharedRef<IDetailCustomization> FReflectionCaptureDetails::MakeInstance()
{
	return MakeShareable( new FReflectionCaptureDetails );
}

void FReflectionCaptureDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	
}

#undef LOCTEXT_NAMESPACE
