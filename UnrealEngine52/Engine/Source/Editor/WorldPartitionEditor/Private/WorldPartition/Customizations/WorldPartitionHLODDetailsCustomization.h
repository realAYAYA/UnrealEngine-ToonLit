// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class AWorldPartitionHLOD;

class FWorldPartitionHLODDetailsCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	FWorldPartitionHLODDetailsCustomization()
	{}

	FReply OnBuildHLOD();

private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;

	TArray<AWorldPartitionHLOD*> GetSelectedHLODActors() const;

private:
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
};