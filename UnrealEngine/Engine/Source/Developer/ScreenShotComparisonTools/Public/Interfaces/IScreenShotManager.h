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
	virtual FScreenshotExportResult ExportScreenshotComparisonResult(FString ScreenshotName, FString ExportPath = TEXT("")) = 0;

	/**
	 * Imports screenshot comparison data from a given path.
	 */
	virtual bool OpenComparisonReports(FString ImportPath, TArray<FComparisonReport>& OutReports) = 0;
	
	/**
	* Calculate the ideal path for already already approved (ground truth) images. This handles abstracting away where Ground Truth images should be saved
	* based on the platform/rhi/etc.
	*/
	virtual FString GetIdealApprovedFolderForImage(const FAutomationScreenshotMetadata& MetaData) const = 0;
};
