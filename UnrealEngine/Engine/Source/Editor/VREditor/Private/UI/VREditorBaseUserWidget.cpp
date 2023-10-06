// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VREditorBaseUserWidget.h"
#include "UI/VREditorFloatingUI.h"

UVREditorBaseUserWidget::UVREditorBaseUserWidget( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer ),
	  Owner( nullptr )
{
}


void UVREditorBaseUserWidget::SetOwner( class AVREditorFloatingUI* NewOwner )
{
	Owner = NewOwner;
}

