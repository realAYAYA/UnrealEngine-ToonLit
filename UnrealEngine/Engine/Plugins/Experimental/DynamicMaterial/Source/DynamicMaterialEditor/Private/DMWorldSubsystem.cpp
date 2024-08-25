// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "DMWorldSubsystem.h"
#include "IDetailKeyframeHandler.h"
#include "LevelEditor/DMLevelEditorIntegration.h"

UDMWorldSubsystem::UDMWorldSubsystem()
	: KeyframeHandler(nullptr)
{
	// Default fallback implementation
	InvokeTabDelegate.BindWeakLambda(this, [this]() { FDMLevelEditorIntegration::InvokeTabForWorld(GetWorld()); });
}
