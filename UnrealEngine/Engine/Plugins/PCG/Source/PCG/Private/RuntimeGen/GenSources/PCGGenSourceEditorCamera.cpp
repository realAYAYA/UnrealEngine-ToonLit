// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeGen/GenSources/PCGGenSourceEditorCamera.h"

#if WITH_EDITOR
#include "EditorViewportClient.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGGenSourceEditorCamera)

TOptional<FVector> UPCGGenSourceEditorCamera::GetPosition() const
{
#if WITH_EDITOR
	if (EditorViewportClient)
	{
		return EditorViewportClient->GetViewLocation();
	}
#endif

	return TOptional<FVector>();
}

TOptional<FVector> UPCGGenSourceEditorCamera::GetDirection() const
{
#if WITH_EDITOR
	if (EditorViewportClient)
	{
		return EditorViewportClient->GetViewRotation().Vector();
	}
#endif

	return TOptional<FVector>();
}
