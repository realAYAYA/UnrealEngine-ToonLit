// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureTypesFwd.h"

class IDetailLayoutBuilder;
class SErrorText;
class IPlugin;

//////////////////////////////////////////////////////////////////////////
// FGameFeatureDataDetailsCustomization

class FGameFeatureDataDetailsCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

protected:
	void ChangeDesiredState(EGameFeaturePluginState State);

	EGameFeaturePluginState GetCurrentState() const;

	FText GetInitialStateText() const;
	FText GetTagConfigPathText() const;

	static void OnOperationCompletedOrFailed(const UE::GameFeatures::FResult& Result, const TWeakPtr<FGameFeatureDataDetailsCustomization> WeakThisPtr);
protected:
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	FString PluginURL;
	TSharedPtr<IPlugin> PluginPtr;

	TSharedPtr<SErrorText> ErrorTextWidget;
};
