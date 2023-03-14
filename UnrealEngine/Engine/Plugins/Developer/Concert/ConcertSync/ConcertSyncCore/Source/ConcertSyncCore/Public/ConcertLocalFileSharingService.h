// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertFileSharingService.h"

/**
 * Share files using a local directory. It works only for one client and one server running on the same machine. The publisher of
 * a file stores a file for sharing and gets a unique ID to consume the file back. It is the publisher responsibility to share the
 * ID with the consumer (i.e. through a concert msg). The shared files are automatically deleted once consumed or when the file
 * expires.
 * 
 * @note This is mainly designed for the recovery system.
 */
class CONCERTSYNCCORE_API FConcertLocalFileSharingService : public IConcertFileSharingService
{
public:
	/**
	 * Construct the local file sharing service.
	 * @param Role Appended to the shared directory (like DisasterRecovery) as a hint for the temp folder purpose. Client and server must use the same value.
	 * @note The service uses the Engine intermediate directory to share the files. The files are automatically deleted after consumption. In case of crash
	 *       the temporary shared files are deleted on next reboot if no other client/server on the machine are not running.
	 */
	FConcertLocalFileSharingService(const FString& Role);
	virtual ~FConcertLocalFileSharingService();

	/** Copies the specified file in the service storage and return a unique ID required to consume it back. The stored file expires in 1 hour. */
	virtual bool Publish(const FString& Pathname, FString& OutFileId) override;

	/** Copies the content of the archive in the service storage and return a unique ID required to consume it back. The stored file expires in 1 hour. */
	virtual bool Publish(FArchive& SrcAr, int64 Size, FString& OutFileId) override;

	/** Consumes a file corresponding to the unique file ID, if available. The file is deleted from the service storage when the shared pointer goes out of scope. */
	virtual TSharedPtr<FArchive> CreateReader(const FString& InFileId) override;

private:
	/** Load the list of concurrent file sharing service running across all processes. */
	void LoadActiveServices(TArray<uint32>& OutPids);

	/** Saved the list of concurrent file sharing service running across all processes. */
	void SaveActiveServices(const TArray<uint32>& InPids);

	/** Track active services instances across all processes and clean up any left over files that expired if no services are running anymore. */
	void RemoveDeadProcessesAndFiles(TArray<uint32>& InOutPids);

private:
	FString SharedRootPathname;
	FString SystemMutexName;
	FString ActiveServicesRepositoryPathname;
};
