// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackgroundHttpFileHashHelper.h"
#include "Containers/Array.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "Interfaces/IBackgroundHttpManager.h"
#include "Interfaces/IBackgroundHttpRequest.h"
#include "Interfaces/IBackgroundHttpResponse.h"
#include "Logging/LogMacros.h"
#include "Templates/Atomic.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBackgroundHttpManager, Log, All)

class FBackgroundHttpFileHashHelper;

/**
 * Contains implementation of some common functions that don't vary between implementation
 */
class BACKGROUNDHTTP_API FBackgroundHttpManagerImpl 
	: public IBackgroundHttpManager
	, public FTSTickerObjectBase
{
public:
	FBackgroundHttpManagerImpl();
	virtual ~FBackgroundHttpManagerImpl();

	virtual void AddRequest(const FBackgroundHttpRequestPtr Request) override;
	virtual void RemoveRequest(const FBackgroundHttpRequestPtr Request) override;

	virtual void Initialize() override;
	virtual void Shutdown() override;

	virtual void DeleteAllTemporaryFiles() override;
	
	virtual int GetMaxActiveDownloads() const override;
	virtual void SetMaxActiveDownloads(int MaxActiveDownloads) override;

	
	virtual FString GetTempFileLocationForURL(const FString& URL) override;
	
	virtual void CleanUpDataAfterCompletingRequest(const FBackgroundHttpRequestPtr Request) override;
	
	//FTSTickerObjectBase implementation
	virtual bool Tick(float DeltaTime) override;
	
protected:
	virtual bool AssociateWithAnyExistingRequest(const FBackgroundHttpRequestPtr Request) override;

	virtual bool CheckForExistingCompletedDownload(const FBackgroundHttpRequestPtr Request, FString& ExistingFilePathOut, int64& ExistingFileSizeOut);
	virtual void ActivatePendingRequests();
	
	//Different from DeleteAllTemporaryFiles as this doesn't delete all files but rather cleans up specific bad files that have gone stale
	virtual void DeleteStaleTempFiles();
	
	//Gets a list of full filenames for all temp files.
	virtual void GatherAllTempFilenames(TArray<FString>& OutAllTempFilenames, bool bOutputAsFullPaths = false) const;
	
	//Helper function that converts the supplied list of Temp folder filenames to a list that is always all full paths.
	virtual void ConvertAllTempFilenamesToFullPaths(TArray<FString>& OutFilenamesAsFullPaths, const TArray<FString>& FilenamesToConvertToFullPaths) const;
	
	//Gather a list of any temp files that have timed out of our BackgroundHTTP settings.
	//SecondsToConsiderOld should be a double representing how many seconds old a temp file needs to be to be returned by this function
	//If OptionalFileListToCheck is empty will check all temp files in the backgroundhttp temp file folder. If supplied only the given file paths are checked
	virtual void GatherTempFilesOlderThen(TArray<FString>& OutTimedOutTempFilenames,double SecondsToConsiderOld, TArray<FString>* OptionalFileList = nullptr) const;
	
	//Gather a list of any temp files that have no corresponding URL Mapping entry
	//If OptionalFileListToCheck is empty will check all temp files in the backgroundhttp temp file folder. If supplied only the given file paths are checked
	virtual void GatherTempFilesWithoutURLMappings(TArray<FString>& OutTempFilesMissingURLMappings, TArray<FString>* OptionalFileList = nullptr) const;
	
	//Gets our FileHashHelper to compute temp file mappings
	virtual BackgroundHttpFileHashHelperRef GetFileHashHelper(){ return FileHashHelper; }
	virtual const BackgroundHttpFileHashHelperRef GetFileHashHelper() const { return FileHashHelper; }
	
protected:
	/** List of Background Http requests that we have called AddRequest on, but have not yet started due to platform active download limits **/
	TArray<FBackgroundHttpRequestPtr> PendingStartRequests;
	FRWLock PendingRequestLock;

	/** List of Background Http requests that are actively being processed **/
	TArray<FBackgroundHttpRequestPtr> ActiveRequests;
	FRWLock ActiveRequestLock;

	/** Count of how many requests we have active **/
	volatile int NumCurrentlyActiveRequests;
	TAtomic<int> MaxActiveDownloads;
	
private:
	BackgroundHttpFileHashHelperRef FileHashHelper;
};
