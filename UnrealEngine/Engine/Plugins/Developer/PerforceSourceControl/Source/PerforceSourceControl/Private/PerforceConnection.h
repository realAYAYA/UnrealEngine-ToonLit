// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PerforceSourceControlPrivate.h"
#include "PerforceSourceControlCommand.h"
#include "Memory/SharedBuffer.h"

class FPerforceSourceControlProvider;

/** A map containing result of running Perforce command */
class FP4Record : public TMap<FString, FString>
{
public:
	virtual ~FP4Record() {}

	const FString& operator()(const FString& Key) const
	{
		if (const FString* Found = Find(Key))
		{
			return *Found;
		}
		return EmptyStr;
	}

	const FString& operator()(const TCHAR* Key) const
	{
		if (const FString* Found = FindByHash(GetTypeHash(Key), Key))
		{
			return *Found;
		}
		return EmptyStr;
	}

private:
	static const FString EmptyStr;
};

typedef TArray<FP4Record> FP4RecordSet;

/** Options to be used with FPerforceConnection::EnsureValidConnection */
enum class EConnectionOptions : uint8
{
	/** No options are applied */
	None				= 0,
	/** The connection does not require a workspace to be considered valid*/
	WorkspaceOptional	= 1 << 0,
};
ENUM_CLASS_FLAGS(EConnectionOptions);

/** Optional flags that can be passed in when a command is run */
enum class ERunCommandFlags : uint32
{
	/** No special options selected */
	Default					= 0,
	/** Prevents us writing the full perforce command (once evaluated) to the log file */
	DisableCommandLogging		= 1 << 0,
	/** The same as passing -q to a perforce command as part of the g-opts. Prevents info output from being printed to stdout */
	Quiet					= 1 << 1
};
ENUM_CLASS_FLAGS(ERunCommandFlags);

class FPerforceConnection
{
public:
	//This constructor is strictly for internal questions to perforce (get client spec list, etc)
	FPerforceConnection(const FPerforceConnectionInfo& InConnectionInfo, FPerforceSourceControlProvider& InSCCProvider);
	/** API Specific close of source control project*/
	~FPerforceConnection();

	/** 
	 * Attempts to automatically detect the workspace to use based on the working directory
	 */
	static bool AutoDetectWorkspace(const FPerforceConnectionInfo& InConnectionInfo, FPerforceSourceControlProvider& SCCProvider, FString& OutWorkspaceName);

	/**
	 * Static function in charge of making sure the specified connection is valid or requests that data from the user via dialog
	 * @param InOutPortName			Port name in the inifile.  Out value is the port name from the connection dialog
	 * @param InOutUserName			User name in the inifile.  Out value is the user name from the connection dialog
	 * @param InOutWorkspaceName	Workspace name in the inifile.  Out value is the client spec from the connection dialog
	 * @param InConnectionInfo		Connection credentials
	 * @return - true if the connection, whether via dialog or otherwise, is valid.  False if source control should be disabled
	 */
	static bool EnsureValidConnection(	FString& InOutServerName, FString& InOutUserName, FString& InOutWorkspaceName,
										const FPerforceConnectionInfo& InConnectionInfo, FPerforceSourceControlProvider& SCCProvider, 
										EConnectionOptions Options = EConnectionOptions::None);

	/**
	 * Get List of ClientSpecs
	 * @param InConnectionInfo	Connection credentials
	 * @param InOnIsCancelled	Delegate called to check for operation cancellation.
	 * @param OutWorkspaceList	The workspace list output.
	 * @param OutErrorMessages	Any error messages output.
	 * @return - True if successful
	 */
	bool GetWorkspaceList(const FPerforceConnectionInfo& InConnectionInfo, FOnIsCancelled InOnIsCancelled, TArray<FString>& OutWorkspaceList, TArray<FText>& OutErrorMessages);

	/** Returns true if connection is currently active */
	bool IsValidConnection();

	/** If the connection is valid, disconnect from the server */
	void Disconnect();

	/**
	 * Runs internal perforce command, catches exceptions, returns results
	 */
	bool RunCommand(const FString& InCommand, const TArray<FString>& InParameters, FP4RecordSet& OutRecordSet, TArray<FText>&  OutErrorMessage, FOnIsCancelled InIsCancelled, bool& OutConnectionDropped)
	{
		TOptional<FSharedBuffer> UnsetDataBuffer;
		return RunCommand(InCommand, InParameters, OutRecordSet, UnsetDataBuffer, OutErrorMessage, InIsCancelled, OutConnectionDropped);
	}

	/**
	 * Runs internal perforce command, catches exceptions, returns results
	 */
	bool RunCommand(const FString& InCommand, const TArray<FString>& InParameters, FP4RecordSet& OutRecordSet, TOptional<FSharedBuffer>& OutData, TArray<FText>& OutErrorMessage, FOnIsCancelled InIsCancelled, bool& OutConnectionDropped, ERunCommandFlags RunFlags = ERunCommandFlags::Default);

	/**
	 * Creates a changelist with the specified description
	 */
	int32 CreatePendingChangelist(const FText &Description, const TArray<FString>& InFiles, FOnIsCancelled InIsCancelled, TArray<FText>& OutErrorMessages);

	/**
	 * Edits a changelist with a new description
	 */
	int32 EditPendingChangelist(const FText& NewDescription, int32 ChangelistNumber, FOnIsCancelled InIsCancelled, TArray<FText>& OutErrorMessages);

	/** 
	 * Creates a workspace based on the spec provided via the WorkspaceSpec.
	 * 
	 * @param WorkspaceSpec		The specification of the workspace to create
	 * @param InIsCancelled		Delegate allowing the cancelling of the command if needed
	 * @param OutErrorMessages	An array that will be filled with all errors encountered during the command
	 * 
	 * @return Returns true if no errors were encountered, otherwise false
	 */
	bool CreateWorkspace(FStringView WorkspaceSpec, FOnIsCancelled InIsCancelled, TArray<FText>& OutErrorMessages);

	/**
	 * Attempt to login - some servers will require this 
	 * @param	InConnectionInfo		Credentials to use
	 * @return true if successful
	 */
	bool Login(const FPerforceConnectionInfo& InConnectionInfo);

	/**
	 * Make a valid connection if possible
	 * @param InConnectionInfo		Connection credentials
	 */
	void EstablishConnection(const FPerforceConnectionInfo& InConnectionInfo);

	/** 
	 * Return the user of the connection
	 * 
	 * @return The current perforce user, this will be blank if the connect failed.
	 */
	FString GetUser();

public:
	
	/** Perforce API client object */
	ClientApi P4Client;

	/** The current root for the client workspace */
	FString ClientRoot;

	/** true if connection was successfully established */
	bool bEstablishedConnection;

	/** Is this a connection to a unicode server? */ 
	bool bIsUnicode;

private:
	/** The source control provider that the connection is working from */
	FPerforceSourceControlProvider& SCCProvider;
};

/**
 * Connection that is used within specific scope
 */
class FScopedPerforceConnection
{
public:
	/** 
	 * Constructor - establish a connection.
	 * The concurrency of the passed in command determines whether the persistent connection is 
	 * used or another connection is established (connections cannot safely be used across different
	 * threads).
	 */
	FScopedPerforceConnection( class FPerforceSourceControlCommand& InCommand);

	/** 
	 * Constructor - establish a connection.
	 * The concurrency passed in determines whether the persistent connection is used or another 
	 * connection is established (connections cannot safely be used across different threads).
	 */
	FScopedPerforceConnection( EConcurrency::Type InConcurrency, FPerforceSourceControlProvider& SCCProvider);

	/**
	 * Destructor - disconnect if this is a temporary connection
	 */
	~FScopedPerforceConnection();

	/** Get the connection wrapped by this class */
	FPerforceConnection& GetConnection() const
	{
		return *Connection;
	}

	/** Check if this connection is valid */
	bool IsValid() const
	{
		return Connection != NULL;
	}

private:
	/** Set up the connection */
	void Initialize( const FPerforceConnectionInfo& InConnectionInfo, FPerforceSourceControlProvider& SCCProvider);

private:
	/** The perforce connection we are using */
	FPerforceConnection* Connection;

	/** The concurrency of this connection */
	EConcurrency::Type Concurrency;
};
