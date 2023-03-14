// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "IVirtualizationBackend.h"

#include "Containers/StringView.h"

class ISourceControlProvider;

namespace UE::Virtualization
{

class FSemaphore;

/**
 * This backend can be used to access payloads stored in source control.
 * The backend doesn't 'check out' a payload file but instead will just download the payload as
 * a binary blob.
 * It is assumed that the files are stored with the same path convention as the file system
 * backend, found in Utils::PayloadIdToPath.
 *
 * Ini file setup:
 * 'Name'=(Type=P4SourceControl, DepotRoot="//XXX/", UsePartitionedClient=X, SubmitFromTempDir=X)
 * Where 'Name' is the backend name in the hierarchy and 'XXX' is the path in the source control
 * depot where the payload files are being stored.
 * 
 * Optional Values:
 * Server [string]:				When set the backend will use this server address to connect to. When not
 *								set the backend will use the environment defaults. [Default=""]
 * ClientStream [string]:		Used when the payloads are stored in a stream based depot. It should contain
 *								the stream name to use when creating a workspace for payload submission. [Default=""]
 * UsePartitionedClient [bool]:	When true the temporary workspace client created to submit payloads 
 *								from will be created as a partitioned workspace which is less overhead
 *								on the source control server. If your server does not support this then
 *								use false. [Default=True]
 * SubmitFromTempDir [bool]:	When set to true, payloads will be submitted from the temp directory of
 *								the current machine and when false the files will be submitted from the
 *								Save directory of the current project. [Default=false]
 * RetryCount [int32]:			How many times we should try to download a payload before giving up with
 *								an error. Useful when the connection is unreliable but does not experience 
 *								frequent persistent outages. [Default=2]
 * RetryWaitTime [int32]:		The length of time the process should wait between each download attempt
 *								in milliseconds. Remember that the max length of time that the process
 *								can stall attempting to download a payload file is 
 *								RetryCount * RetryWaitTime; [Default=100ms]
 * BatchCount [int32]			The max number of payloads that can be pushed to source control in a
 *								single submit. If the number of payloads in a request batch exceeds
 *								this size then it will be split into multiple smaller batches. [Default=100]
 * SuppressNotifications[bool]:	When true the system will not display a pop up notification when a 
 *								connection error occurs, allowing the user to stay unaware of the error
 *								unless it actually causes some sort of problem. [Default=false]
 * 
 * Environment Variables:
 * UE-VirtualizationWorkingDir [string]:	This can be set to a valid directory path that the backend
 *											should use as the root location to submit payloads from.
 *											If the users machine has this set then 'SubmitFromTempDir' 
 *											will be ignored. 
 * 
 * Perforce Setup
 * The backend will attempt to use the current global environment settings for perforce to determine 
 * the P4PORT and P4USER values. If you wish to set them via an ini file then they can be done per
 * user by adding the following to Saved\Config\<platform>Editor\SourceControlSettings.ini
 * 
 * -----
 * [PerforceSourceControl.VirtualizationSettings]
 * Port=<server address>
 * UserName=<username>
 * -----
 * Note that the backend will read from these ini file settings but will not write to them.
 */
class FSourceControlBackend final : public IVirtualizationBackend
{
public:
	explicit FSourceControlBackend(FStringView ProjectName, FStringView ConfigName, FStringView InDebugName);
	virtual ~FSourceControlBackend() = default;
	
private:
	/* IVirtualizationBackend implementation */

	virtual bool Initialize(const FString& ConfigEntry) override;

	virtual EConnectionStatus OnConnect() override;
	
	virtual bool PushData(TArrayView<FPushRequest> Requests) override;
	virtual bool PullData(TArrayView<FPullRequest> Requests) override;

	virtual bool DoesPayloadExist(const FIoHash& Id) override;
	
	virtual bool DoPayloadsExist(TArrayView<const FIoHash> PayloadIds, TArray<bool>& OutResults) override;

private:

	bool TryApplySettingsFromConfigFiles(const FString& ConfigEntry);

	void CreateDepotPath(const FIoHash& PayloadId, FStringBuilderBase& OutPath);

	bool FindSubmissionWorkingDir(const FString& ConfigEntry);

	/** Will display a FMessage notification to the user on the next valid engine tick to try and keep them aware of connection failures */
	void OnConnectionError();

	/** A source control connection owned by the backend*/
	TUniquePtr<ISourceControlProvider> SCCProvider;
	
	/** The name of the current project */
	FString ProjectName;

	/** The root where the virtualized payloads are stored in source control */
	FString DepotRoot;

	/** Address of the server to connect to */
	FString ServerAddress;

	/** The stream containing the DepotRoot where the virtualized payloads are stored in source control */
	FString ClientStream;

	/** The root directory from which payloads are submitted. */
	FString SubmissionRootDir;

	/** Should we try to make the temp client partitioned or not? */
	bool bUsePartitionedClient = true;

	/** When true, the backend will not raise a pop up notification on connection error */
	bool bSuppressNotifications = false;

	/** The maximum number of files to send in a single source control operation */
	int32 MaxBatchCount = 100;

	/** A counted semaphore that will limit the number of concurrent connections that we can make */
	TUniquePtr<UE::Virtualization::FSemaphore> ConcurrentConnectionLimit;
	
	/** The number of times to retry pulling a payload from the depot */
	int32 RetryCount = 2;

	/** The length of time (in milliseconds) to wait after each pull attempt before retrying. */
	int32 RetryWaitTimeMS = 100;
};

} // namespace UE::Virtualization
