// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleEditorSubsystemInterface.h"
#include "CoreGlobals.h"

#if WITH_EDITOR

IContentBundleEditorSubsystemInterface* IContentBundleEditorSubsystemInterface::Instance = nullptr;

void IContentBundleEditorSubsystemInterface::SetInstance(IContentBundleEditorSubsystemInterface* InInstance)
{ 
	check(InInstance == nullptr || Instance == nullptr); 
	Instance = InInstance; 
}

#endif
