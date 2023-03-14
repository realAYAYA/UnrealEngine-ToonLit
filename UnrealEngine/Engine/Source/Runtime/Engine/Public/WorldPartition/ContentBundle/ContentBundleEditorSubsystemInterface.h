// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FContentBundleEditor;

#if WITH_EDITOR
class ENGINE_API IContentBundleEditorSubsystemInterface
{
public:
	static IContentBundleEditorSubsystemInterface* Get() { return Instance; }

	virtual void NotifyContentBundleAdded(const FContentBundleEditor* ContentBundle) = 0;
	virtual void NotifyContentBundleRemoved(const FContentBundleEditor* ContentBundle) = 0;
	virtual void NotifyContentBundleInjectedContent(const FContentBundleEditor* ContentBundle) = 0;
	virtual void NotifyContentBundleRemovedContent(const FContentBundleEditor* ContentBundle) = 0;
	virtual void NotifyContentBundleChanged(const FContentBundleEditor* ContentBundle) = 0;

protected:
	static void SetInstance(IContentBundleEditorSubsystemInterface* InInstance);
	static IContentBundleEditorSubsystemInterface* Instance;
};
#endif