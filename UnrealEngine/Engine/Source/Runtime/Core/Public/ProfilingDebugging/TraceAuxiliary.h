// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "CoreGlobals.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/Build.h"

#if !defined(UE_TRACE_SERVER_CONTROLS_ENABLED)
#	define UE_TRACE_SERVER_CONTROLS_ENABLED (PLATFORM_DESKTOP && !UE_BUILD_SHIPPING && !IS_PROGRAM)
#endif

////////////////////////////////////////////////////////////////////////////////
class FTraceAuxiliary
{
public:
	// In no logging configurations all log categories are of type FNoLoggingCategory, which has no relation with
	// FLogCategoryBase. In order to not need to conditionally set the argument alias the type here.
#if NO_LOGGING
	typedef FNoLoggingCategory FLogCategoryAlias;
#else
	typedef FLogCategoryBase FLogCategoryAlias;
#endif

	enum class EConnectionType : uint8
	{
		/**
		 * Connect to a trace server. Target is IP address or hostname.
		 */
		Network,
		/**
		 * Write to a file. Target string is filename. Absolute or relative current working directory.
		 * If target is null the current date and time is used.
		 */
		File,
		/**
		 * Don't connect, just start tracing to memory.
		 */
		None,
	};

	/**
	 * Callback type when a new connection is established.
	 */
	DECLARE_TS_MULTICAST_DELEGATE(FOnConnection);

	/** Callback whenever a trace is started */
	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnTraceStarted, FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination);

	/** 
	* Callback whenever a trace recording is stopped. 
	* TraceType tells what kind of trace it is.
	* TraceDestination will be either the the filename and path for a file trace or the network connection for a network trace
	*/
	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnTraceStopped, FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination);

	/**
	* Callback whenever a trace snapshot is saved.
	* Path is the file system path of the snapshot file.
	*/
	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnSnapshotSaved, FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination);

	struct FOptions
	{
		/** When set, trace will not start a worker thread, instead it is updated from end frame delegate. */
		bool bNoWorkerThread = false;
		/** When set, the target file will be truncated if it already exists. */
		bool bTruncateFile = false;
		/** When set, trace data buffered before starting tracing will not be output to the trace file */
		bool bExcludeTail = false;
	};

	/**
	 * Start tracing to a target (network connection or file) with an active set of channels. If a connection is
	 * already active this call does nothing.
	 * @param Type Type of connection
	 * @param Target String to use for connection. See /ref EConnectionType for details.
	 * @param Channels Comma separated list of channels to enable. Default set of channels are enabled if argument is not specified. If the pointer is null no channels are enabled.
	 * @param Options Optional additional tracing options.
	 * @param LogCategory Log channel to output messages to. Default set to 'Core'.
	 * @return True when successfully starting the trace, false if the data connection could not be made.
	 */
	static CORE_API bool Start(EConnectionType Type, const TCHAR* Target, const TCHAR* Channels = TEXT("default"), FOptions* Options = nullptr, const FLogCategoryAlias& LogCategory = LogCore);

	/**
	 * Stop tracing.
	 * @return True if the trace was stopped, false if there was no data connection.
	 */
	static CORE_API bool Stop();

	/**
	 * Pause all tracing by disabling all active channels.
	 */
	static CORE_API bool Pause();

	/**
	* @return True if trace was paused and the list of channels to resume exists.
	*/
	static CORE_API bool IsPaused();

	/**
	 * Resume tracing by enabling all previously active channels.
	 */
	static CORE_API bool Resume();

	/**
	 * Write tailing memory state to a utrace file.
	 * @param FilePath Path to the file to write the snapshot to. If it is null or empty a file path will be generated.
	 */
	static CORE_API bool WriteSnapshot(const TCHAR* FilePath);
	
	/**
	 * Write tailing memory state to a trace server.
	 * @param FilePath Path to the file to write the snapshot to. If it is null or empty a file path will be generated.
	 */
	static CORE_API bool SendSnapshot(const TCHAR* Host = nullptr, uint32 Port = 0);

	/**
	 * Initialize Trace systems.
	 * @param CommandLine to use for initializing
	 */
	static CORE_API void Initialize(const TCHAR* CommandLine);

	/**
	 * Initialize channels that use the config driven presets.
	 * @param CommandLine to use for initializing
	 */
	static CORE_API void InitializePresets(const TCHAR* CommandLine);

	/**
	 * Shut down Trace systems.
	 */
	static CORE_API void Shutdown();

	/**
	 * Attempts to auto connect to an active trace server if an active session
	 * of Unreal Insights Session Browser is running.
	 */
	static CORE_API void TryAutoConnect();

	/**
	 *  Enable previously selected channels. This method can be called multiple times
	 *  as channels can be announced on module loading.
	 */
	static CORE_API void EnableChannels();

	/**
	* Disable channels to stop recording traces with them.
	* @param Channels List of channels (or a preset) to disable. If null it will disable all active channels.
	*/
	static CORE_API void DisableChannels(const TCHAR* Channels = nullptr);

	/**
	 *  Returns the destination string that is currently being traced to.
	 *  Contains either a file path or network address. Points to an empty string if tracing is disabled.
	 */
	UE_DEPRECATED(5.3, "Use GetTraceDestinationString instead.")
	static CORE_API const TCHAR*	GetTraceDestination();
	
	/**
	 *  Returns the destination string that is currently being traced to.
	 *  Contains either a file path or network address. Empty if tracing is disabled.
	 */
	static CORE_API FString GetTraceDestinationString();
	
	/**
	 *  Returns whether the trace system is currently connected to a trace sink (file or network)
	 */
	static CORE_API bool	IsConnected();


	/**
	 * Returns whether the trace system is currently connected. If connected writes the session/trace identifiers.
	 * @param OutSessionGuid If connected, the session guid
	 * @param OutTraceGuid If connected, the trace guid
	 * @return True if connected
	 */
	static CORE_API bool IsConnected(FGuid& OutSessionGuid, FGuid& OutTraceGuid);

	/**
	*  Returns the current connection type.
	*/
	static CORE_API EConnectionType GetConnectionType();

	/**
	 *  Adds a comma separated list of currently active channels to the passed in StringBuilder
	 */
	static CORE_API void	GetActiveChannelsString(FStringBuilderBase& String);

	/**
	 * Used when process is panicking. Stops all tracing immediately to avoid further allocations. Process is not
	 * expected to continue after this call.
	 */
	 static CORE_API void Panic();

	/**
	 * Delegate that triggers when a connection is established. Gives subscribers a chance to trace events that appear
	 * after important events but before regular events (including tail). The following restrictions apply:
	 *  * Only NoSync event types can be emitted.
	 *  * Important events should not be emitted. They will appear after the events in the tail.
	 *  * Callback is issued from a worker thread. User is responsible to synchronize shared resources.
	 * 
	 * @note This is an advanced feature to avoid using important events in cases where event data can be
	 *		 recalled easily.
	 *		 
	 * @param Callback Delegate to call on new connections.
	 */
	static CORE_API FOnConnection OnConnection;

	/**
	 * Delegate that triggers when a trace session is started.
	 * The type of recording and the destination (filepath or network) is passed to the delegate.
	 */
	static CORE_API FOnTraceStarted OnTraceStarted;

	/**
	 * Delegate that triggers when a trace has finished recording. Useful if you need to collect all completed trace files in a session.
	 * The type of recording and the destination (filepath or network) is passed to the delegate.
	 */
	static CORE_API FOnTraceStopped OnTraceStopped;

	/**
	 * Delegate that triggers when a snapshot has been saved.
	 * The path to the snapshot file is passed to the delegate.
	 */
	static CORE_API FOnSnapshotSaved OnSnapshotSaved;
	
};

#if UE_TRACE_SERVER_CONTROLS_ENABLED

/**
 * Controls for Unreal Trace Server, the standalone server recording and storing traces.
 */
class FTraceServerControls
{
public:
	/**
	 * Launch the server using the "fork" command. This spins off an separate running process.	 * 
	 * @return True if the server was successfully started or already running.
	 */
	static CORE_API bool Start();
	/**
	 * Stop any running instance of the server.
	 * @return True if the stop command was successful. False otherwise.
	 */
	static CORE_API bool Stop();
};

#endif // UE_TRACE_SERVER_CONTROLS_ENABLED