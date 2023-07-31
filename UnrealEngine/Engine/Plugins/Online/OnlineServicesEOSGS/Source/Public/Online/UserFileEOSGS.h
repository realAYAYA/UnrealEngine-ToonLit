// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/UserFileCommon.h"

#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_playerdatastorage_types.h"

namespace UE::Online {

class FOnlineServicesEOSGS;

struct FUserFileEOSGSConfig
{
	int32 ChunkLengthBytes = 4096;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FUserFileEOSGSConfig)
	ONLINE_STRUCT_FIELD(FUserFileEOSGSConfig, ChunkLengthBytes)
END_ONLINE_STRUCT_META()

/* Meta */ }

class ONLINESERVICESEOSGS_API FUserFileEOSGS : public FUserFileCommon
{
public:
	using Super = FUserFileCommon;

	FUserFileEOSGS(FOnlineServicesEOSGS& InOwningSubsystem);
	virtual ~FUserFileEOSGS() = default;

	// IOnlineComponent
	virtual void Initialize() override;
	virtual void UpdateConfig() override;

	// IUserFile
	virtual TOnlineAsyncOpHandle<FUserFileEnumerateFiles> EnumerateFiles(FUserFileEnumerateFiles::Params&& Params) override;
	virtual TOnlineResult<FUserFileGetEnumeratedFiles> GetEnumeratedFiles(FUserFileGetEnumeratedFiles::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FUserFileReadFile> ReadFile(FUserFileReadFile::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FUserFileWriteFile> WriteFile(FUserFileWriteFile::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FUserFileCopyFile> CopyFile(FUserFileCopyFile::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FUserFileDeleteFile> DeleteFile(FUserFileDeleteFile::Params&& Params) override;

protected:
	EOS_HPlayerDataStorage PlayerDataStorageHandle = nullptr;

	FUserFileEOSGSConfig Config;

	TMap<FAccountId, TArray<FString>> UserToFiles;

	static EOS_PlayerDataStorage_EReadResult EOS_CALL OnReadFileDataStatic(const EOS_PlayerDataStorage_ReadFileDataCallbackInfo* Data);
	static void EOS_CALL OnReadFileCompleteStatic(const EOS_PlayerDataStorage_ReadFileCallbackInfo* Data);

	static EOS_PlayerDataStorage_EWriteResult EOS_CALL OnWriteFileDataStatic(const EOS_PlayerDataStorage_WriteFileDataCallbackInfo* Data, void* OutDataBuffer, uint32_t* OutDataWritten);
	static void EOS_CALL OnWriteFileCompleteStatic(const EOS_PlayerDataStorage_WriteFileCallbackInfo* Data);

	static void EOS_CALL OnFileTransferProgressStatic(const EOS_PlayerDataStorage_FileTransferProgressCallbackInfo* Data);
};

/* UE::Online */ }
