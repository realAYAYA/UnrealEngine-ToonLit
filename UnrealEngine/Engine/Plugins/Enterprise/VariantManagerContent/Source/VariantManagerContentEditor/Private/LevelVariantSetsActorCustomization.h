// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class ALevelVariantSetsActor;
class FReply;
class IDetailLayoutBuilder;

class FLevelVariantSetsActorCustomization : public IDetailCustomization
{
public:
	FLevelVariantSetsActorCustomization();
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:
	FReply OnOpenVariantManagerButtonClicked(ALevelVariantSetsActor* Actor);
	FReply OnCreateLevelVarSetsButtonClicked();
};
