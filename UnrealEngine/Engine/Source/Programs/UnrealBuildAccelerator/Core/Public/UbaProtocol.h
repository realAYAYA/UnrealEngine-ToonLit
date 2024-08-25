// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"

namespace uba
{
	#define UBA_PROCESS_MESSAGES \
		UBA_PROCESS_MESSAGE(Init) \
		UBA_PROCESS_MESSAGE(CreateFile) \
		UBA_PROCESS_MESSAGE(GetFullFileName) \
		UBA_PROCESS_MESSAGE(CloseFile) \
		UBA_PROCESS_MESSAGE(DeleteFile) \
		UBA_PROCESS_MESSAGE(CopyFile) \
		UBA_PROCESS_MESSAGE(MoveFile) \
		UBA_PROCESS_MESSAGE(Chmod) \
		UBA_PROCESS_MESSAGE(CreateDirectory) \
		UBA_PROCESS_MESSAGE(ListDirectory) \
		UBA_PROCESS_MESSAGE(UpdateTables) \
		UBA_PROCESS_MESSAGE(CreateProcess) \
		UBA_PROCESS_MESSAGE(StartProcess) \
		UBA_PROCESS_MESSAGE(ExitChildProcess) \
		UBA_PROCESS_MESSAGE(CreateTempFile) \
		UBA_PROCESS_MESSAGE(OpenTempFile) \
		UBA_PROCESS_MESSAGE(VirtualAllocFailed) \
		UBA_PROCESS_MESSAGE(Log) \
		UBA_PROCESS_MESSAGE(EchoOn) \
		UBA_PROCESS_MESSAGE(InputDependencies) \
		UBA_PROCESS_MESSAGE(Exit) \
		UBA_PROCESS_MESSAGE(FlushWrittenFiles) \
		UBA_PROCESS_MESSAGE(UpdateEnvironment) \
		UBA_PROCESS_MESSAGE(GetNextProcess) \
		UBA_PROCESS_MESSAGE(Custom) \

	enum MessageType : u8
	{
		Unused = 0,
		#define UBA_PROCESS_MESSAGE(type) MessageType_##type,
		UBA_PROCESS_MESSAGES
		#undef UBA_PROCESS_MESSAGE
	};

	inline constexpr u32 ProcessMessageVersion = 1339;

	inline constexpr u32 CommunicationMemSize = IsWindows ? 64*1024 : 64*1024*2; // Macos expands some commandlines to be crazy long

	inline constexpr u32 FileMappingTableMemSize = 16 * 1024 * 1024;
	inline constexpr u32 DirTableMemSize = 40 * 1024 * 1024;
}

// Currently only used for detoured process
#if UBA_DEBUG
#define UBA_DEBUG_LOG_ENABLED 1
#define UBA_DEBUG_VALIDATE 1
#else
#define UBA_DEBUG_LOG_ENABLED 0
#define UBA_DEBUG_VALIDATE 0
#endif
