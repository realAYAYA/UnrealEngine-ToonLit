// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMSettings)

URigVMEditorSettings::URigVMEditorSettings(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bAutoLinkMutableNodes = false;
#endif
}

