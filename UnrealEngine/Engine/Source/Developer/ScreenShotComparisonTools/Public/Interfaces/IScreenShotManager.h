// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IScreenShotComparisonModule.h: Declares the IScreenShotComparisonModule interface.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "ImageComparer.h"

class IScreenShotManager;
struct FAutomationScreenshotMetadata;

/**
 * Type definition for shared pointers to instances of IScreenShotManager.
 */
typedef TSharedPtr<class IScreenShotManager> IScreenShotManagerPtr;

/**
 * Type definition for shared references to instances of IScreenShotManager.
 */
typedef TSharedRef<class IScreenShotManager> IScreenShotManagerRef;

struct FScreenshotExportResult
{
	bool Success;
	FString ExportPath;

	FScreenshotExportResult()
		: Success(false)
	{
	}
};

/**
 * Describes options available when comparing screenshots
 */
enum class EScreenShotCompareOptions
{
	None,
	DiscardImage,
	KeepImage,
};

/**
 * Interface that defines a class which is capable of comparing screenshots at a provided path with checked in ground truth versions
 * and generate results and reports.
 */
class IScreenShotManager
{
public:
		
	virtual ~IScreenShotManager(){ }

	/**
	 * Takes the file at the provided path and uses the metadata to find and compare it with a ground truth version. 
	 * 
	 * @param IncomingPath		Path to the file. The file can reside anywhere but for best practices it should be under FPaths::AutomationTransientDir()
	 * @param MetaData			Meta data for the image. This should have been created when the image was captured and is usually at the same path but with a json extension
	 * @param Options			Comparison options. Use EScreenShotCompareOptions::DiscardImage if the incoming image does not need to be preserved (implicit if the path is under a transient dir)
	 *
	 * @return TFuture<FImageComparisonResult>
	 */
	virtual TFuture<FImageComparisonResult> CompareScreenshotAsync(const FString& IncomingPath, const FAutomationScreenshotMetadata& MetaData, const EScreenShotCompareOptions Options) = 0;

	/**
	 * Exports target screenshot report to the export location specified
	 */
	virtual FScreenshotExportResult ExportScreenshotComparisonResult(FString ScreenshotName, FString ExportPath = TEXT(""), bool bOnlyIncoming = false) = 0;

	/**
	 * Imports screenshot comparison data from a given path asynchronously.
	 */
	virtual TFuture<TSharedPtr<TArray<FComparisonReport>>> OpenComparisonReportsAsync(const FString& ImportPath) = 0;

	/**
	* Calculate the ideal path for already already approved (ground truth) images. This handles abstracting away where Ground Truth images should be saved
	* based on the platform/rhi/etc.
	*/
	virtual FString GetIdealApprovedFolderForImage(const FAutomationScreenshotMetadata& MetaData) const = 0;

	/**
	* Find the all the files that are approved for the given metadata and file pattern 
	*/
	virtual TArray<FString> FindApprovedFiles(const FAutomationScreenshotMetadata& IncomingMetaData, const FString& FilePattern) const = 0;

	/**
	* Compare a sequence of images. If the returned value is null no comparison failed. Otherwise return the first failing frame.
	*/
	virtual TSharedPtr<FImageComparisonResult> CompareImageSequence(const TMap<FString, FString>& Sequence, const FAutomationScreenshotMetadata& Metadata) = 0;

	/**
	* Notify the automation test framework of an image comparison result
	*/
	virtual void NotifyAutomationTestFrameworkOfImageComparison(const FImageComparisonResult& Result) = 0;
};
