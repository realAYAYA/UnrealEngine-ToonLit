// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"

namespace uba
{
	// Default constants
	static constexpr u16 DefaultPort = 1345;
	static constexpr u16 DefaultStorageProxyPort = DefaultPort + 1;
	static constexpr u32 SendMaxSize = 256*1024;
	static constexpr u32 SendDefaultSize = 256*1024;

	static constexpr u8 SystemServiceId = 0;
	static constexpr u8 StorageServiceId = 1;
	static constexpr u8 SessionServiceId = 2;
	
	static constexpr u32 SystemNetworkVersion = 1339;
	static constexpr u32 StorageNetworkVersion = 3;
	static constexpr u32 SessionNetworkVersion = 30;


	// Messages used over network between client/server (system, storage and session)

	enum SystemMessageType : u8
	{
		SystemMessageType_SetConnectionCount,
		SystemMessageType_KeepAlive,
	};

	#define UBA_STORAGE_MESSAGES \
		UBA_STORAGE_MESSAGE(FetchBegin) \
		UBA_STORAGE_MESSAGE(FetchSegment) \
		UBA_STORAGE_MESSAGE(FetchEnd) \
		UBA_STORAGE_MESSAGE(ExistsOnServer) \
		UBA_STORAGE_MESSAGE(StoreBegin) \
		UBA_STORAGE_MESSAGE(StoreSegment) \
		UBA_STORAGE_MESSAGE(StoreEnd) \
		UBA_STORAGE_MESSAGE(Connect) \

	enum StorageMessageType : u8
	{
		#define UBA_STORAGE_MESSAGE(x) StorageMessageType_##x,
		UBA_STORAGE_MESSAGES
		#undef UBA_STORAGE_MESSAGE
	};

	inline const tchar* ToString(StorageMessageType type)
	{
		switch (type)
		{
			#define UBA_STORAGE_MESSAGE(x) case StorageMessageType_##x: return TC("")#x;
			UBA_STORAGE_MESSAGES
			#undef UBA_STORAGE_MESSAGE
		default:
			return TC("Unknown"); // Should never happen
		}
	}


	#define UBA_SESSION_MESSAGES \
		UBA_SESSION_MESSAGE(Connect) \
		UBA_SESSION_MESSAGE(EnsureBinaryFile) \
		UBA_SESSION_MESSAGE(GetApplication) \
		UBA_SESSION_MESSAGE(GetFileFromServer) \
		UBA_SESSION_MESSAGE(SendFileToServer) \
		UBA_SESSION_MESSAGE(DeleteFile) \
		UBA_SESSION_MESSAGE(CopyFile) \
		UBA_SESSION_MESSAGE(CreateDirectory) \
		UBA_SESSION_MESSAGE(ListDirectory) \
		UBA_SESSION_MESSAGE(GetDirectoriesFromServer) \
		UBA_SESSION_MESSAGE(GetNameToHashFromServer) \
		UBA_SESSION_MESSAGE(ProcessAvailable) \
		UBA_SESSION_MESSAGE(ProcessFinished) \
		UBA_SESSION_MESSAGE(ProcessReturned) \
		UBA_SESSION_MESSAGE(VirtualAllocFailed) \
		UBA_SESSION_MESSAGE(GetTraceInformation) \
		UBA_SESSION_MESSAGE(Ping) \
		UBA_SESSION_MESSAGE(Notification) \
		UBA_SESSION_MESSAGE(GetNextProcess) \
		UBA_SESSION_MESSAGE(Custom) \
		UBA_SESSION_MESSAGE(UpdateEnvironment) \
		UBA_SESSION_MESSAGE(Summary) \
		UBA_SESSION_MESSAGE(Command) \

	enum SessionMessageType : u8 
	{
		#define UBA_SESSION_MESSAGE(x) SessionMessageType_##x,
		UBA_SESSION_MESSAGES
		#undef UBA_SESSION_MESSAGE
	};

	// Response types for SessionMessageType_ProcessAvailable
	enum SessionProcessAvailableResponse : u32
	{
		SessionProcessAvailableResponse_None = 0,
		SessionProcessAvailableResponse_Disconnect = ~u32(0),
		SessionProcessAvailableResponse_RemoteExecutionDisabled = ~u32(0) - 1,
	};

	inline constexpr const char EncryptionHandshakeString[] = "This is a test string used to check so encryption keys matches between client and server. This string is 128 characters long...";
}
