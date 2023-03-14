// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MediaIOCoreDefinitions.h"


struct MEDIAIOEDITOR_API FMediaIOPermutationsSelectorBuilder
{
	static const FName NAME_DeviceIdentifier;
	static const FName NAME_TransportType;
	static const FName NAME_QuadType;
	static const FName NAME_Resolution;
	static const FName NAME_Standard;
	static const FName NAME_FrameRate;

	static const FName NAME_InputType;
	static const FName NAME_OutputType;
	static const FName NAME_KeyPortSource;
	static const FName NAME_OutputReference;
	static const FName NAME_SyncPortSource;

	static bool IdenticalProperty(FName ColumnName, const FMediaIOConnection& Left, const FMediaIOConnection& Right);
	static bool Less(FName ColumnName, const FMediaIOConnection& Left, const FMediaIOConnection& Right);
	static FText GetLabel(FName ColumnName, const FMediaIOConnection& Item);
	static FText GetTooltip(FName ColumnName, const FMediaIOConnection& Item);
	
	static bool IdenticalProperty(FName ColumnName, const FMediaIOMode& Left, const FMediaIOMode& Right);
	static bool Less(FName ColumnName, const FMediaIOMode& Left, const FMediaIOMode& Right);
	static FText GetLabel(FName ColumnName, const FMediaIOMode& Item);
	static FText GetTooltip(FName ColumnName, const FMediaIOMode& Item);

	static bool IdenticalProperty(FName ColumnName, const FMediaIOConfiguration& Left, const FMediaIOConfiguration& Right);
	static bool Less(FName ColumnName, const FMediaIOConfiguration& Left, const FMediaIOConfiguration& Right);
	static FText GetLabel(FName ColumnName, const FMediaIOConfiguration& Item);
	static FText GetTooltip(FName ColumnName, const FMediaIOConfiguration& Item);

	static bool IdenticalProperty(FName ColumnName, const FMediaIOInputConfiguration& Left, const FMediaIOInputConfiguration& Right);
	static bool Less(FName ColumnName, const FMediaIOInputConfiguration& Left, const FMediaIOInputConfiguration& Right);
	static FText GetLabel(FName ColumnName, const FMediaIOInputConfiguration& Item);
	static FText GetTooltip(FName ColumnName, const FMediaIOInputConfiguration& Item);

	static bool IdenticalProperty(FName ColumnName, const FMediaIOOutputConfiguration& Left, const FMediaIOOutputConfiguration& Right);
	static bool Less(FName ColumnName, const FMediaIOOutputConfiguration& Left, const FMediaIOOutputConfiguration& Right);
	static FText GetLabel(FName ColumnName, const FMediaIOOutputConfiguration& Item);
	static FText GetTooltip(FName ColumnName, const FMediaIOOutputConfiguration& Item);
};
