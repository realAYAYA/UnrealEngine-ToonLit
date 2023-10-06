// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGLoadAlembic.h"

#include "Elements/PCGLoadAlembicElement.h"

void UPCGLoadAlembicFunctionLibrary::LoadAlembicFileToPCG(const FPCGLoadAlembicBPData& InSettings, FPCGDataCollection& Data, UObject* TargetOuter)
{
	// Simulate execution of the FPCGLoadAlembicElement.
	UPCGLoadAlembicSettings* Settings = NewObject<UPCGLoadAlembicSettings>();
	Settings->AlembicFilePath = InSettings.AlembicFilePath;
	Settings->ConversionScale = InSettings.ConversionSettings.Scale;
	Settings->ConversionRotation = InSettings.ConversionSettings.Rotation;
	Settings->bConversionFlipHandedness = InSettings.bConversionFlipHandedness;
	Settings->AttributeMapping = InSettings.AttributeMapping;

	FPCGLoadAlembicElement Element;
	FPCGContext* Context = Element.Initialize(FPCGDataCollection(), TWeakObjectPtr<UPCGComponent>(), nullptr);
	Context->AsyncState.NumAvailableTasks = 1; // TODO : change this
	FPCGTaggedData& DataSettings = Context->InputData.TaggedData.Emplace_GetRef();
	DataSettings.Data = Settings;

	while (!Element.Execute(Context))
	{
		// Nothing
	}

	Data = Context->OutputData;
	delete Context;

	// Reouter the data if needed
	if (TargetOuter)
	{
		for (FPCGTaggedData& TaggedData : Data.TaggedData)
		{
			if (TaggedData.Data)
			{
				Cast<UObject>(TaggedData.Data)->Rename(nullptr, TargetOuter, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors);
			}
		}
	}
}

void UPCGLoadAlembicFunctionLibrary::SetupFromStandard(FPCGLoadAlembicBPData& Data, EPCGLoadAlembicStandardSetup InSetup)
{
	if (InSetup != EPCGLoadAlembicStandardSetup::None)
	{
		Data.ConversionSettings.Preset = EAbcConversionPreset::Custom;
		UPCGLoadAlembicSettings::SetupFromStandard(InSetup, Data.ConversionSettings.Scale, Data.ConversionSettings.Rotation, Data.bConversionFlipHandedness, Data.AttributeMapping);
	}	
}