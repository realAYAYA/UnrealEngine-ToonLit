// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterColorGradingDataModel.h"

class IDetailTreeNode;

/** Color Grading Data Model Generator for the APostProcessVolume actor class */
class FDisplayClusterColorGradingGenerator_PostProcessVolume: public IDisplayClusterColorGradingDataModelGenerator
{
public:
	static TSharedRef<IDisplayClusterColorGradingDataModelGenerator> MakeInstance();

	//~ IDisplayClusterColorGradingDataModelGenerator interface
	virtual void Initialize(const TSharedRef<class FDisplayClusterColorGradingDataModel>& ColorGradingDataModel, const TSharedRef<IPropertyRowGenerator>& PropertyRowGenerator) override;
	virtual void Destroy(const TSharedRef<class FDisplayClusterColorGradingDataModel>& ColorGradingDataModel, const TSharedRef<IPropertyRowGenerator>& PropertyRowGenerator) override;
	virtual void GenerateDataModel(IPropertyRowGenerator& PropertyRowGenerator, FDisplayClusterColorGradingDataModel& OutColorGradingDataModel) override;
	//~ End IDisplayClusterColorGradingDataModelGenerator interface

private:
	void AddPropertyToColorGradingElement(const TSharedPtr<IPropertyHandle>& PropertyHandle, FDisplayClusterColorGradingDataModel::FColorGradingElement& ColorGradingElement);
};