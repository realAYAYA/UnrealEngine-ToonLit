// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Containers/StringView.h"
#include "IO/IoDispatcher.h"

#if !UE_BUILD_SHIPPING

class FStorageServerFileHandle;
class FStorageServerConnection;
class IPackageStore;

#if WITH_COTF
namespace UE::Cook
{
	class FCookOnTheFlyMessage;
	class ICookOnTheFlyServerConnection;
}
#endif

class FStorageServerFileSystemTOC
{
public:
	~FStorageServerFileSystemTOC();
	void AddFile(const FIoChunkId& FileChunkId, FStringView Path);
	bool FileExists(const FString& Path);
	bool DirectoryExists(const FString& Path);
	const FIoChunkId* GetFileChunkId(const FString& Path);
	bool IterateDirectory(const FString& Path, TFunctionRef<bool(const FIoChunkId&, const TCHAR*)> Callback);

private:
	struct FDirectory
	{
		TArray<FString> Directories;
		TArray<int32> Files;
	};

	struct FFile
	{
		FIoChunkId FileChunkId;
		FString FilePath;
	};

	FDirectory* AddDirectoriesRecursive(const FString& DirectoryPath);

	FDirectory Root;
	TMap<FString, FDirectory*> Directories;
	TMap<FString, int32> FilePathToIndexMap;
	TArray<FFile> Files;
	FRWLock TocLock;
};

class FStorageServerPlatformFile
	: public IPlatformFile
{
public:
	FStorageServerPlatformFile();
	virtual ~FStorageServerPlatformFile();
	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override;
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;
	virtual void InitializeAfterProjectFilePath() override;

	virtual IPlatformFile* GetLowerLevel() override
	{
		return LowerLevel;
	}

	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override
	{
		LowerLevel = NewLowerLevel;
	}

	virtual const TCHAR* GetName() const override
	{
		return TEXT("StorageServer");
	}

	virtual bool FileExists(const TCHAR* Filename) override;
	virtual int64 FileSize(const TCHAR* Filename) override;
	virtual bool IsReadOnly(const TCHAR* Filename) override;
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override;
	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override;
	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual bool DirectoryExists(const TCHAR* Directory) override;
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override;
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override;
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override;
	virtual bool DeleteFile(const TCHAR* Filename) override;
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override;
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override;
	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override;
	virtual bool CreateDirectory(const TCHAR* Directory) override;
	virtual bool DeleteDirectory(const TCHAR* Directory) override;
	virtual FString ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename) override;
	virtual bool SendMessageToServer(const TCHAR* Message, IPlatformFile::IFileServerMessageHandler* Handler) override;

private:
	friend class FStorageServerFileHandle;

	bool MakeStorageServerPath(const TCHAR* LocalFilenameOrDirectory, FStringBuilderBase& OutPath) const;
	bool MakeLocalPath(const TCHAR* ServerFilenameOrDirectory, FStringBuilderBase& OutPath) const;
	IFileHandle* InternalOpenFile(const FIoChunkId& FileChunkId, const TCHAR* LocalFilename);
	bool SendGetFileListMessage();
	FFileStatData SendGetStatDataMessage(const FIoChunkId& FileChunkId);
	int64 SendReadMessage(uint8* Destination, const FIoChunkId& FileChunkId, int64 Offset, int64 BytesToRead);
#if WITH_COTF
	void OnCookOnTheFlyMessage(const UE::Cook::FCookOnTheFlyMessage& Message);
#endif
	TUniquePtr<FArchive> TryFindProjectStoreMarkerFile(IPlatformFile* Inner) const;

	IPlatformFile* LowerLevel = nullptr;
	FStringView ServerEngineDirView = FStringView(TEXT("/{engine}/"));
	FStringView ServerProjectDirView = FStringView(TEXT("/{project}/"));
	TUniquePtr<FStorageServerConnection> Connection;
#if WITH_COTF
	TSharedPtr<UE::Cook::ICookOnTheFlyServerConnection> CookOnTheFlyServerConnection;
#endif
	FStorageServerFileSystemTOC ServerToc;
	FString ServerProject;
	FString ServerPlatform;
	mutable TArray<FString> HostAddrs;
	mutable uint16 HostPort = 8558;
};

#endif
