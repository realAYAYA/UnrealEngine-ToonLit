// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineOutputBase.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MoviePipelineFCPXMLExporterSetting.generated.h"

UENUM(BlueprintType)
enum class FCPXMLExportDataSource : uint8
{
	OutputMetadata,
	SequenceData
};

UCLASS(BlueprintType)
class MOVIERENDERPIPELINECORE_API UMoviePipelineFCPXMLExporter : public UMoviePipelineOutputBase
{
	GENERATED_BODY()
public:
	UMoviePipelineFCPXMLExporter()
		: DataSource(FCPXMLExportDataSource::SequenceData)
		, bHasFinishedExporting(false)
	{}

public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "FCPXMLExporterDisplayName", "Final Cut Pro XML"); }
#endif
protected:
	virtual bool HasFinishedExportingImpl() { return bHasFinishedExporting; }
	virtual void BeginExportImpl() override;

	bool EnsureWritableFile();
	bool bOverwriteFile;
public:
	/** File name format string override. If specified it will override the FileNameFormat from the Output setting. Can include folder prefixes, and format string tags ({sequence_name}, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Output")
	FString FileNameFormatOverride;
	
	/** Whether to build the FCPXML from sequence data directly (for reimporting) or from actual frame output data (for post processing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Output")
	FCPXMLExportDataSource DataSource;

protected:
	/** The file to write to */
	FString FilePath;

	bool bHasFinishedExporting;
};