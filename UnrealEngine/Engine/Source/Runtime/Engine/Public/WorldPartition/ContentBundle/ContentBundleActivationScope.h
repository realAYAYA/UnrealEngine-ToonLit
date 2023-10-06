// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/ContentBundle/ContentBundleEditorSubsystemInterface.h"

class FContentBundleEditor;

#if WITH_EDITOR

class FContentBundleActivationScope
{
public:
	FContentBundleActivationScope(FGuid InContentBundleGuid)
	{
		if (InContentBundleGuid.IsValid())
		{
			ContentBundleGuid = InContentBundleGuid;

			IContentBundleEditorSubsystemInterface* ContentBundleEditorSubsystem = IContentBundleEditorSubsystemInterface::Get();
			ContentBundleEditorSubsystem->PushContentBundleEditing();

			if (TSharedPtr<FContentBundleEditor> ContentBundleEditor = ContentBundleEditorSubsystem->GetEditorContentBundle(ContentBundleGuid))
			{
				ContentBundleEditorSubsystem->ActivateContentBundleEditing(ContentBundleEditor);
			}
		}
	}
	~FContentBundleActivationScope()
	{
		if (ContentBundleGuid.IsValid())
		{
			IContentBundleEditorSubsystemInterface* ContentBundleEditorSubsystem = IContentBundleEditorSubsystemInterface::Get();
			if (TSharedPtr<FContentBundleEditor> ContentBundleEditor = ContentBundleEditorSubsystem->GetEditorContentBundle(ContentBundleGuid))
			{
				ContentBundleEditorSubsystem->DeactivateContentBundleEditing(ContentBundleEditor);
			}

			ContentBundleEditorSubsystem->PopContentBundleEditing();
		}
	}

private:
	FGuid ContentBundleGuid;
};

#endif
