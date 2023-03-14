// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EnvironmentQueryGraphNode.h"
#include "EnvironmentQueryGraphNode_Option.generated.h"

class UEdGraph;

UCLASS()
class UEnvironmentQueryGraphNode_Option : public UEnvironmentQueryGraphNode
{
	GENERATED_UCLASS_BODY()

	uint32 bStatShowOverlay : 1;
	TArray<FEnvionmentQueryNodeStats> StatsPerGenerator;
	float StatAvgPickRate;

	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetDescription() const override;

	virtual void PrepareForCopying() override;
	virtual void UpdateNodeClassData() override;

	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	void CreateAddTestSubMenu(class UToolMenu* Menu, UEdGraph* Graph) const;

	void CalculateWeights();
	void UpdateNodeData();

protected:

	virtual void ResetNodeOwner() override;
};
