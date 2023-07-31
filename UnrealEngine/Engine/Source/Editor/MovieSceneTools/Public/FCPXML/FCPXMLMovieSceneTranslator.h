// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MovieScene.h"
#include "MovieSceneTranslator.h"

class FMovieSceneTranslatorContext;

/** 
 * The FFCPXMLImporter class is the entry point for launching an import of data from an XML file into Sequencer. 
 */
class MOVIESCENETOOLS_API FFCPXMLImporter : public FMovieSceneImporter
{
public:
	FFCPXMLImporter();

	virtual ~FFCPXMLImporter();

	/** Format description. */
	virtual FText GetFileTypeDescription() const;
	/** Import window title. */
	virtual FText GetDialogTitle() const;
	/** Scoped transaction description. */
	virtual FText GetTransactionDescription() const;
	/** Message log window title. */
	virtual FName GetMessageLogWindowTitle() const;
	/** Message log list label. */
	virtual FText GetMessageLogLabel() const;

public:
	/*
	 * Import FCP 7 XML
	 *
	 * @param InMovieScene The movie scene to import the XML file into
	 * @param InFrameRate The frame rate to import the XML at
	 * @param InFilename The filename to import
	 * @param OutError The return error message
	 * @return Whether the import was successful
	 */
	bool Import(UMovieScene* InMovieScene, FFrameRate InFrameRate, FString InFilename, TSharedRef<FMovieSceneTranslatorContext> InContext);
};

/** 
 * The FFCPXMLExporter class is the entry point for launching an export of data from Sequencer into an XML file. 
 */
class MOVIESCENETOOLS_API FFCPXMLExporter : public FMovieSceneExporter
{
public:
	/** Constructor */
	FFCPXMLExporter();
	/** Destructor */
	virtual ~FFCPXMLExporter();

	/** Format description. */
	virtual FText GetFileTypeDescription() const;
	/** Export dialog window title. */
	virtual FText GetDialogTitle() const;
	/** Default format file extension. */
	virtual FText GetDefaultFileExtension() const;
	/** Notification when export completes. */
	virtual FText GetNotificationExportFinished() const;
	/** Notification hyperlink to exported file path. */
	virtual FText GetNotificationHyperlinkText() const;
	/** Message log window title. */
	virtual FName GetMessageLogWindowTitle() const;
	/** Message log list label. */
	virtual FText GetMessageLogLabel() const;

public:
	/*
	 * Export FCP 7 XML
	 *
	 * @param InMovieScene The movie scene with the cinematic shot track and audio tracks to export
	 * @param InFilenameFormat The last filename format used to render shots.
	 * @param InFrameRate The frame rate for export.
	 * @param InResX Sequence resolution x.
	 * @param InResY Sequence resolution y.
	 * @param InHandleFrames The number of handle frames to include for each shot.
	 * @param InSaveFilename The file path to save to.
	 * @param OutError The return error message
	 * @param MovieExtension The movie extension for the shot filenames (ie. .avi, .mov, .mp4)
	 * @param InMetadata (optional) Metadata from export to override movie output file list
	 * @return Whether the export was successful
	 */
	
	virtual bool Export(const UMovieScene* InMovieScene, FString InFilenameFormat, FFrameRate InFrameRate, uint32 InResX, uint32 InResY, int32 InHandleFrames, FString InSaveFilename, TSharedRef<FMovieSceneTranslatorContext> InContext, FString InMovieExtension, const FMovieSceneExportMetadata* InMetadata=nullptr);
};
