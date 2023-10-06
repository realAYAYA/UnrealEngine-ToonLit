// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterColorGradingDataModel.h"

class IDetailTreeNode;

/** Color Grading Data Model Generator for the AColorCorrectionRegion actor class */
class FDisplayClusterColorGradingGenerator_ColorCorrectRegion : public IDisplayClusterColorGradingDataModelGenerator
{
public:
	static TSharedRef<IDisplayClusterColorGradingDataModelGenerator> MakeInstance();

	//~ IDisplayClusterColorGradingDataModelGenerator interface
	virtual void Initialize(const TSharedRef<class FDisplayClusterColorGradingDataModel>& ColorGradingDataModel, const TSharedRef<IPropertyRowGenerator>& PropertyRowGenerator) override;
	virtual void Destroy(const TSharedRef<class FDisplayClusterColorGradingDataModel>& ColorGradingDataModel, const TSharedRef<IPropertyRowGenerator>& PropertyRowGenerator) override;
	virtual void GenerateDataModel(IPropertyRowGenerator& PropertyRowGenerator, FDisplayClusterColorGradingDataModel& OutColorGradingDataModel) override;
	//~ End IDisplayClusterColorGradingDataModelGenerator interface

private:
	/** Creates a new color grading element structure for the specified detail tree node, which is expected to have child color properties with the ColorGradingMode metadata set */
	FDisplayClusterColorGradingDataModel::FColorGradingElement CreateColorGradingElement(const TSharedRef<IDetailTreeNode>& GroupNode, FText ElementLabel);

	bool FilterDetailsViewProperties(const TSharedRef<IDetailTreeNode>& InDetailTreeNode);
};