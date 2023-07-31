// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SSceneOutliner.h"

class FContentBundleEditor;

class SContentBundleOutliner : public SSceneOutliner
{
public:
	void Construct(const FArguments& InArgs, const FSceneOutlinerInitializationOptions& InitOptions)
	{
		SSceneOutliner::Construct(InArgs, InitOptions);
	}

	SContentBundleOutliner() {}
	virtual ~SContentBundleOutliner() {}

	void SelectContentBundle(const TWeakPtr<FContentBundleEditor>& ContentBundle);
};