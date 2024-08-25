// Copyright Epic Games, Inc. All Rights Reserved.

#include "StorageServerPlatformFile.h"
#include "Algo/Replace.h"
#include "CookOnTheFly.h"
#include "CookOnTheFlyPackageStore.h"
#include "HAL/FileManagerGeneric.h"
#include "HAL/IPlatformFileModule.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleManager.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StorageServerConnection.h"
#include "StorageServerIoDispatcherBackend.h"
#include "StorageServerPackageStore.h"

DEFINE_LOG_CATEGORY_STATIC(LogStorageServerPlatformFile, Log, All);

#if !UE_BUILD_SHIPPING

FStorageServerFileSystemTOC::~FStorageServerFileSystemTOC()
{
	FWriteScopeLock _(TocLock);
	for (auto& KV : Directories)
	{
		delete KV.Value;
	}
}

FStorageServerFileSystemTOC::FDirectory* FStorageServerFileSystemTOC::AddDirectoriesRecursive(const FString& DirectoryPath)
{
	FDirectory* Directory = new FDirectory();
	Directories.Add(DirectoryPath, Directory);
	FString ParentDirectoryPath = FPaths::GetPath(DirectoryPath);
	FDirectory* ParentDirectory;
	if (ParentDirectoryPath.IsEmpty())
	{
		ParentDirectory = &Root;
	}
	else
	{
		ParentDirectory = Directories.FindRef(ParentDirectoryPath);
		if (!ParentDirectory)
		{
			ParentDirectory = AddDirectoriesRecursive(ParentDirectoryPath);
		}
	}
	ParentDirectory->Directories.Add(DirectoryPath);
	return Directory;
}

void FStorageServerFileSystemTOC::AddFile(const FIoChunkId& FileChunkId, FStringView PathView)
{
	FWriteScopeLock _(TocLock);

	const int32 FileIndex = Files.Num();
	
	FFile& NewFile = Files.AddDefaulted_GetRef();
	NewFile.FileChunkId = FileChunkId;
	NewFile.FilePath = PathView;
	
	FilePathToIndexMap.Add(NewFile.FilePath, FileIndex);
	
	FString DirectoryPath = FPaths::GetPath(NewFile.FilePath);
	FDirectory* Directory = Directories.FindRef(DirectoryPath);
	if (!Directory)
	{
		Directory = AddDirectoriesRecursive(DirectoryPath);
	}
	Directory->Files.Add(FileIndex);
}

bool FStorageServerFileSystemTOC::FileExists(const FString& Path)
{
	FReadScopeLock _(TocLock);
	return FilePathToIndexMap.Contains(Path);
}

bool FStorageServerFileSystemTOC::DirectoryExists(const FString& Path)
{
	FReadScopeLock _(TocLock);
	return Directories.Contains(Path);
}

const FIoChunkId* FStorageServerFileSystemTOC::GetFileChunkId(const FString& Path)
{
	FReadScopeLock _(TocLock);
	if (const int32* FileIndex = FilePathToIndexMap.Find(Path))
	{
		return &Files[*FileIndex].FileChunkId;
	}
	return nullptr;
}

bool FStorageServerFileSystemTOC::IterateDirectory(const FString& Path, TFunctionRef<bool(const FIoChunkId&, const TCHAR*)> Callback)
{
	UE_LOG(LogStorageServerPlatformFile, Verbose, TEXT("IterateDirectory '%s'"), *Path);

	FReadScopeLock _(TocLock);

	FDirectory* Directory = Directories.FindRef(Path);
	if (!Directory)
	{
		return false;
	}
	for (int32 FileIndex : Directory->Files)
	{
		const FFile& File = Files[FileIndex];
		if (!Callback(File.FileChunkId, *File.FilePath))
		{
			return false;
		}
	}
	for (const FString& ChildDirectoryPath : Directory->Directories)
	{
		if (!Callback(FIoChunkId(), *ChildDirectoryPath))
		{
			return false;
		}
	}
	return true;
}

class FStorageServerFileHandle
	: public IFileHandle
{
	enum
	{
		BufferSize = 64 << 10
	};
	FStorageServerPlatformFile& Owner;
	FIoChunkId FileChunkId;
	FString Filename;
	int64 FilePos = 0;
	int64 FileSize = -1;
	int64 BufferStart = -1;
	int64 BufferEnd = -1;
	uint8 Buffer[BufferSize];

public:
	FStorageServerFileHandle(FStorageServerPlatformFile& InOwner, FIoChunkId InFileChunkId, const TCHAR* InFilename)
		: Owner(InOwner)
		, FileChunkId(InFileChunkId)
		, Filename(InFilename)
	{
	}

	~FStorageServerFileHandle()
	{
	}

	virtual int64 Size() override
	{
		if (FileSize < 0)
		{
			const FFileStatData FileStatData = Owner.SendGetStatDataMessage(FileChunkId);
			if (FileStatData.bIsValid)
			{
				FileSize = FileStatData.FileSize;
			}
			else
			{
				UE_LOG(LogStorageServerPlatformFile, Warning, TEXT("Failed to obtain size of file '%s'"), *Filename);
				FileSize = 0;
			}
		}
		return FileSize;
	}

	virtual int64 Tell() override
	{
		return FilePos;
	}

	virtual bool Seek(int64 NewPosition) override
	{
		FilePos = NewPosition;
		return true;
	}

	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		return Seek(Size() + NewPositionRelativeToEnd);
	}

	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
		if (BytesToRead == 0)
		{
			return true;
		}

		if (BytesToRead > BufferSize)
		{
			const int64 BytesRead = Owner.SendReadMessage(Destination, FileChunkId, FilePos, BytesToRead);
			if (BytesRead == BytesToRead)
			{
				FilePos += BytesRead;
				return true;
			}
			return false;
		}

		int64 BytesReadFromBuffer = 0;
		if (FilePos >= BufferStart && FilePos < BufferEnd)
		{
			const int64 BufferOffset = FilePos - BufferStart;
			check(BufferOffset < BufferSize);
			BytesReadFromBuffer = FMath::Min(BufferSize - BufferOffset, BytesToRead);
			FMemory::Memcpy(Destination, Buffer + BufferOffset, BytesReadFromBuffer);
			if (BytesReadFromBuffer == BytesToRead)
			{
				FilePos += BytesReadFromBuffer;
				return true;
			}
		}

		const int64 BytesRead = Owner.SendReadMessage(Buffer, FileChunkId, FilePos + BytesReadFromBuffer, BufferSize);
		BufferStart = FilePos + BytesReadFromBuffer;
		BufferEnd = BufferStart + BytesRead;

		const int64 BytesToReadFromBuffer = FMath::Min(BytesRead, BytesToRead - BytesReadFromBuffer);
		FMemory::Memcpy(Destination + BytesReadFromBuffer, Buffer, BytesToReadFromBuffer);
		BytesReadFromBuffer += BytesToReadFromBuffer;
		if (BytesReadFromBuffer == BytesToRead)
		{
			FilePos += BytesReadFromBuffer;
			return true;
		}
		
		return false;
	}

	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
		check(false);
		return false;
	}

	virtual bool Flush(const bool bFullFlush = false) override
	{
		return false;
	}

	virtual bool Truncate(int64 NewSize) override
	{
		return false;
	}
};

FStorageServerPlatformFile::FStorageServerPlatformFile()
{
}

FStorageServerPlatformFile::~FStorageServerPlatformFile()
{
}

TUniquePtr<FArchive> FStorageServerPlatformFile::TryFindProjectStoreMarkerFile(IPlatformFile* Inner) const
{
	if (Inner == nullptr)
	{
		return nullptr;
	}

	FString RelativeStagedPath = TEXT("../../../");
	FString RootPath = FPaths::RootDir();
	FString PlatformName = FPlatformProperties::PlatformName();
	FString CookedOutputPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved"), TEXT("Cooked"), PlatformName);

	TArray<FString> PotentialProjectStorePaths;
	PotentialProjectStorePaths.Add(RelativeStagedPath);
	PotentialProjectStorePaths.Add(CookedOutputPath);
	PotentialProjectStorePaths.Add(RootPath);

	for (const FString& ProjectStorePath : PotentialProjectStorePaths)
	{
		FString ProjectMarkerPath = ProjectStorePath / TEXT("ue.projectstore");
		if (IFileHandle* ProjectStoreMarkerHandle = Inner->OpenRead(*ProjectMarkerPath); ProjectStoreMarkerHandle != nullptr)
		{
			UE_LOG(LogStorageServerPlatformFile, Display, TEXT("Found '%s'"), *ProjectMarkerPath);
			return TUniquePtr<FArchive>(new FArchiveFileReaderGeneric(ProjectStoreMarkerHandle, *ProjectMarkerPath, ProjectStoreMarkerHandle->Size()));
		}
	}
	return nullptr;
}

bool FStorageServerPlatformFile::ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
{
#if WITH_COTF
	UE::Cook::ICookOnTheFlyModule& CookOnTheFlyModule = FModuleManager::LoadModuleChecked<UE::Cook::ICookOnTheFlyModule>(TEXT("CookOnTheFly"));
	TSharedPtr<UE::Cook::ICookOnTheFlyServerConnection> DefaultConnection = CookOnTheFlyModule.GetDefaultServerConnection();
	if (DefaultConnection.IsValid() && !DefaultConnection->GetZenProjectName().IsEmpty())
	{
		HostAddrs.Append(DefaultConnection->GetZenHostNames());
		HostPort = DefaultConnection->GetZenHostPort();
		return true;
	}
#endif
	TUniquePtr<FArchive> ProjectStoreMarkerReader = TryFindProjectStoreMarkerFile(Inner);
	if (ProjectStoreMarkerReader != nullptr)
	{
		TSharedPtr<FJsonObject> ProjectStoreObject;
		TSharedRef<TJsonReader<UTF8CHAR>> Reader = TJsonReaderFactory<UTF8CHAR>::Create(ProjectStoreMarkerReader.Get());
		if (FJsonSerializer::Deserialize(Reader, ProjectStoreObject) && ProjectStoreObject.IsValid())
		{
			const TSharedPtr<FJsonObject>* ZenServerObjectPtr = nullptr;
			if (ProjectStoreObject->TryGetObjectField(TEXT("zenserver"), ZenServerObjectPtr) && (ZenServerObjectPtr != nullptr))
			{
				const TSharedPtr<FJsonObject>& ZenServerObject = *ZenServerObjectPtr;
#if PLATFORM_DESKTOP
				FString HostName;
				if (ZenServerObject->TryGetStringField(TEXT("hostname"), HostName) && !HostName.IsEmpty())
				{
					HostAddrs.Add(HostName);
				}
#endif
				const TArray<TSharedPtr<FJsonValue>>* RemoteHostNamesArrayPtr = nullptr;
				if (ZenServerObject->TryGetArrayField(TEXT("remotehostnames"), RemoteHostNamesArrayPtr) && (RemoteHostNamesArrayPtr != nullptr))
				{
					for (TSharedPtr<FJsonValue> RemoteHostName : *RemoteHostNamesArrayPtr)
					{
						if (FString RemoteHostNameStr = RemoteHostName->AsString(); !RemoteHostNameStr.IsEmpty())
						{
							HostAddrs.Add(RemoteHostNameStr);
						}
					}
				}

				uint16 SerializedHostPort = 0;
				if (ZenServerObject->TryGetNumberField(TEXT("hostport"), SerializedHostPort) && (SerializedHostPort != 0))
				{
					HostPort = SerializedHostPort;
				}
				UE_LOG(LogStorageServerPlatformFile, Display, TEXT("Using connection settings from ue.projectstore: HostAddrs='%s' and HostPort='%d'"), *FString::Join(HostAddrs, TEXT("+")), HostPort);
			}
		}
	}

	FString Host;
	if (FParse::Value(FCommandLine::Get(), TEXT("-ZenStoreHost="), Host))
	{
		UE_LOG(LogStorageServerPlatformFile, Display, TEXT("Adding connection settings from command line: -ZenStoreHost='%s'"), *Host);
		if (!Host.ParseIntoArray(HostAddrs, TEXT("+"), true))
		{
			HostAddrs.Add(Host);
		}
	}
	if (FParse::Value(CmdLine, TEXT("-ZenStorePort="), HostPort))
	{
		UE_LOG(LogStorageServerPlatformFile, Display, TEXT("Using connection settings from command line: -ZenStorePort='%d'"), HostPort);
	}
	return HostAddrs.Num() > 0;
}

bool FStorageServerPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	LowerLevel = Inner;
	if (HostAddrs.Num() > 0)
	{
		// Don't initialize the connection yet because we want to incorporate project file path information into the initialization.

		TUniquePtr<FArchive> ProjectStoreMarkerReader = TryFindProjectStoreMarkerFile(Inner);
		if (ProjectStoreMarkerReader != nullptr)
		{
			TSharedPtr<FJsonObject> ProjectStoreObject;
			TSharedRef<TJsonReader<UTF8CHAR>> Reader = TJsonReaderFactory<UTF8CHAR>::Create(ProjectStoreMarkerReader.Get());
			if (FJsonSerializer::Deserialize(Reader, ProjectStoreObject) && ProjectStoreObject.IsValid())
			{
				const TSharedPtr<FJsonObject>* ZenServerObjectPtr = nullptr;
				if (ProjectStoreObject->TryGetObjectField(TEXT("zenserver"), ZenServerObjectPtr) && (ZenServerObjectPtr != nullptr))
				{
					const TSharedPtr<FJsonObject>& ZenServerObject = *ZenServerObjectPtr;
					ServerProject = ZenServerObject->GetStringField(TEXT("projectid"));
					ServerPlatform = ZenServerObject->GetStringField(TEXT("oplogid"));
					UE_LOG(LogStorageServerPlatformFile, Display, TEXT("Using settings from ue.projectstore: ServerProject='%s' and ServerPlatform='%s'"), *ServerProject, *ServerPlatform);
				}
			}
		}
	
		if (FParse::Value(CmdLine, TEXT("-ZenStoreProject="), ServerProject))
		{
			UE_LOG(LogStorageServerPlatformFile, Display, TEXT("Using settings from command line: -ZenStoreProject='%s'"), *ServerProject);
		}
		if (FParse::Value(CmdLine, TEXT("-ZenStorePlatform="), ServerPlatform))
		{
			UE_LOG(LogStorageServerPlatformFile, Display, TEXT("Using settings from command line: -ZenStorePlatform='%s'"), *ServerPlatform);
		}
		return true;
	}
	return false;
}

void FStorageServerPlatformFile::InitializeAfterProjectFilePath()
{
#if WITH_COTF
	UE::Cook::ICookOnTheFlyModule& CookOnTheFlyModule = FModuleManager::LoadModuleChecked<UE::Cook::ICookOnTheFlyModule>(TEXT("CookOnTheFly"));
	CookOnTheFlyServerConnection = CookOnTheFlyModule.GetDefaultServerConnection();
	if (CookOnTheFlyServerConnection)
	{
		CookOnTheFlyServerConnection->OnMessage().AddRaw(this, &FStorageServerPlatformFile::OnCookOnTheFlyMessage);
		ServerProject = CookOnTheFlyServerConnection->GetZenProjectName();
		ServerPlatform = CookOnTheFlyServerConnection->GetPlatformName();
	}
#endif
	Connection.Reset(new FStorageServerConnection());
	const TCHAR* ProjectOverride = ServerProject.IsEmpty() ? nullptr : *ServerProject;
	const TCHAR* PlatformOverride = ServerPlatform.IsEmpty() ? nullptr : *ServerPlatform;
	if (Connection->Initialize(HostAddrs, HostPort, ProjectOverride, PlatformOverride))
	{
		if (SendGetFileListMessage())
		{
			FIoDispatcher& IoDispatcher = FIoDispatcher::Get();
			TSharedRef<FStorageServerIoDispatcherBackend> IoDispatcherBackend = MakeShared<FStorageServerIoDispatcherBackend>(*Connection.Get());
			IoDispatcher.Mount(IoDispatcherBackend);
#if WITH_COTF
			if (CookOnTheFlyServerConnection)
			{
				FPackageStore::Get().Mount(MakeShared<FCookOnTheFlyPackageStoreBackend>(*CookOnTheFlyServerConnection.Get()));
			}
			else
#endif
			{
				FPackageStore::Get().Mount(MakeShared<FStorageServerPackageStoreBackend>(*Connection.Get()));
			}
		}
		else
		{
			UE_LOG(LogStorageServerPlatformFile, Fatal, TEXT("Failed to get file list from Zen at '%s'"), *Connection->GetHostAddr());
		}
	}
	else
	{
		if (!FApp::IsUnattended())
		{
			FText FailedConnectionTitle = NSLOCTEXT("StorageServer", "StorageServer_ConnectFailedTitle", "Failed to connect");
			FText FailedConnectionText = FText::Format(NSLOCTEXT("StorageServer", "StorageServer_ConnectFailedText",
				"Network data streaming failed to connect to any of the following data sources:\n\n{0}\n\n"
				"This can be due to the sources being offline, the Unreal Zen Storage process not currently running, "
				"invalid addresses, firewall blocking, or the sources being on a different network from this device. "
				"Please verify that your Unreal Zen Storage process is running using the ZenDashboard utility. "
				"If these issues can't be addressed, you can use an installed build without network data streaming by "
				"building with the '-pak' argument. This process will now exit."),
				FText::FromString(FString::Join(HostAddrs, TEXT("\n"))));
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FailedConnectionText.ToString(), *FailedConnectionTitle.ToString());
		}

		UE_LOG(LogStorageServerPlatformFile, Error, TEXT("Failed to initialize connection to %s"), *FString::Join(HostAddrs, TEXT("\n")));
		FPlatformMisc::RequestExit(true);
	}
}

bool FStorageServerPlatformFile::FileExists(const TCHAR* Filename)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename) && ServerToc.FileExists(*StorageServerFilename))
	{
		return true;
	}
	return LowerLevel->FileExists(Filename);
}

FDateTime FStorageServerPlatformFile::GetTimeStamp(const TCHAR* Filename)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename))
	{
		if (const FIoChunkId* FileChunkId = ServerToc.GetFileChunkId(*StorageServerFilename))
		{
			const FFileStatData FileStatData = SendGetStatDataMessage(*FileChunkId);
			check(FileStatData.bIsValid);
			return FileStatData.ModificationTime;
		}
	}
	return LowerLevel->GetTimeStamp(Filename);
}

FDateTime FStorageServerPlatformFile::GetAccessTimeStamp(const TCHAR* Filename)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename))
	{
		if (const FIoChunkId* FileChunkId = ServerToc.GetFileChunkId(*StorageServerFilename))
		{
			const FFileStatData FileStatData = SendGetStatDataMessage(*FileChunkId);
			check(FileStatData.bIsValid);
			return FileStatData.AccessTime;
		}
	}
	return LowerLevel->GetAccessTimeStamp(Filename);
}

int64 FStorageServerPlatformFile::FileSize(const TCHAR* Filename)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename))
	{
		if (const FIoChunkId* FileChunkId = ServerToc.GetFileChunkId(*StorageServerFilename))
		{
			const FFileStatData FileStatData = SendGetStatDataMessage(*FileChunkId);
			check(FileStatData.bIsValid);
			return FileStatData.FileSize;
		}
	}
	return LowerLevel->FileSize(Filename);
}

bool FStorageServerPlatformFile::IsReadOnly(const TCHAR* Filename)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename) && ServerToc.FileExists(*StorageServerFilename))
	{
		return true;
	}
	return LowerLevel->IsReadOnly(Filename);
}

FFileStatData FStorageServerPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	TStringBuilder<1024> StorageServerFilenameOrDirectory;
	if (MakeStorageServerPath(FilenameOrDirectory, StorageServerFilenameOrDirectory))
	{
		if (const FIoChunkId* FileChunkId = ServerToc.GetFileChunkId(*StorageServerFilenameOrDirectory))
		{
			return SendGetStatDataMessage(*FileChunkId);
		}
		else if (ServerToc.DirectoryExists(*StorageServerFilenameOrDirectory))
		{
			return FFileStatData(
				FDateTime::MinValue(),
				FDateTime::MinValue(),
				FDateTime::MinValue(),
				0,
				true,
				true);
		}
	}
	return LowerLevel->GetStatData(FilenameOrDirectory);
}

IFileHandle* FStorageServerPlatformFile::InternalOpenFile(const FIoChunkId& FileChunkId, const TCHAR* LocalFilename)
{
	return new FStorageServerFileHandle(*this, FileChunkId, LocalFilename);
}

IFileHandle* FStorageServerPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename))
	{
		if (const FIoChunkId* FileChunkId = ServerToc.GetFileChunkId(*StorageServerFilename))
		{
			return InternalOpenFile(*FileChunkId, Filename);
		}
	}
	return LowerLevel->OpenRead(Filename, bAllowWrite);
}

bool FStorageServerPlatformFile::IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor)
{
	TStringBuilder<1024> StorageServerDirectory;
	bool bResult = false;
	if (MakeStorageServerPath(Directory, StorageServerDirectory) && ServerToc.DirectoryExists(*StorageServerDirectory))
	{
		bResult |= ServerToc.IterateDirectory(*StorageServerDirectory, [this, &Visitor](const FIoChunkId& FileChunkId, const TCHAR* FilenameOrDirectory)
		{
			TStringBuilder<1024> LocalPath;
			bool bConverted = MakeLocalPath(FilenameOrDirectory, LocalPath);
			check(bConverted);
			const bool bDirectory = !FileChunkId.IsValid();
			return Visitor.CallShouldVisitAndVisit(*LocalPath, bDirectory);
		});
	}
	else
	{
		bResult |= LowerLevel->IterateDirectory(Directory, Visitor);
	}
	return bResult;
}

bool FStorageServerPlatformFile::IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor)
{
	TStringBuilder<1024> StorageServerDirectory;
	bool bResult = false;
	if (MakeStorageServerPath(Directory, StorageServerDirectory) && ServerToc.DirectoryExists(*StorageServerDirectory))
	{
		bResult |= ServerToc.IterateDirectory(*StorageServerDirectory, [this, &Visitor](const FIoChunkId& FileChunkId, const TCHAR* ServerFilenameOrDirectory)
		{
			TStringBuilder<1024> LocalPath;
			bool bConverted = MakeLocalPath(ServerFilenameOrDirectory, LocalPath);
			check(bConverted);
			FFileStatData FileStatData;
			if (FileChunkId.IsValid())
			{
				FileStatData = SendGetStatDataMessage(FileChunkId);
				check(FileStatData.bIsValid);
			}
			else
			{
				FileStatData = FFileStatData(
					FDateTime::MinValue(),
					FDateTime::MinValue(),
					FDateTime::MinValue(),
					0,
					true,
					true);
			}
			return Visitor.CallShouldVisitAndVisit(*LocalPath, FileStatData);
		});
	}
	else
	{
		bResult |= LowerLevel->IterateDirectoryStat(Directory, Visitor);
	}
	return bResult;
}

bool FStorageServerPlatformFile::DirectoryExists(const TCHAR* Directory)
{
	TStringBuilder<1024> StorageServerDirectory;
	if (MakeStorageServerPath(Directory, StorageServerDirectory) && ServerToc.DirectoryExists(*StorageServerDirectory))
	{
		return true;
	}
	return LowerLevel->DirectoryExists(Directory);
}

FString FStorageServerPlatformFile::GetFilenameOnDisk(const TCHAR* Filename)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename) && ServerToc.FileExists(*StorageServerFilename))
	{
		UE_LOG(LogStorageServerPlatformFile, Warning, TEXT("Attempting to get disk filename of remote file '%s'"), Filename);
		return Filename;
	}
	return LowerLevel->GetFilenameOnDisk(Filename);
}

bool FStorageServerPlatformFile::DeleteFile(const TCHAR* Filename)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename) && ServerToc.FileExists(*StorageServerFilename))
	{
		return false;
	}
	return LowerLevel->DeleteFile(Filename);
}

bool FStorageServerPlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
	TStringBuilder<1024> StorageServerTo;
	if (MakeStorageServerPath(To, StorageServerTo) && ServerToc.FileExists(*StorageServerTo))
	{
		return false;
	}
	TStringBuilder<1024> StorageServerFrom;
	if (MakeStorageServerPath(From, StorageServerFrom))
	{
		if (const FIoChunkId* FromFileChunkId = ServerToc.GetFileChunkId(*StorageServerFrom))
		{
			TUniquePtr<IFileHandle> ToFile(LowerLevel->OpenWrite(To, false, false));
			if (!ToFile)
			{
				return false;
			}

			TUniquePtr<IFileHandle> FromFile(InternalOpenFile(*FromFileChunkId, *StorageServerFrom));
			if (!FromFile)
			{
				return false;
			}
			const int64 BufferSize = 64 << 10;
			TArray<uint8> Buffer;
			Buffer.SetNum(BufferSize);
			int64 BytesLeft = FromFile->Size();
			while (BytesLeft)
			{
				int64 BytesToWrite = FMath::Min(BufferSize, BytesLeft);
				if (!FromFile->Read(Buffer.GetData(), BytesToWrite))
				{
					return false;
				}
				if (!ToFile->Write(Buffer.GetData(), BytesToWrite))
				{
					return false;
				}
				BytesLeft -= BytesToWrite;
			}
			return true;
		}
	}
	return LowerLevel->MoveFile(To, From);
}

bool FStorageServerPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename) && ServerToc.FileExists(*StorageServerFilename))
	{
		return bNewReadOnlyValue;
	}
	return LowerLevel->SetReadOnly(Filename, bNewReadOnlyValue);
}

void FStorageServerPlatformFile::SetTimeStamp(const TCHAR* Filename, FDateTime DateTime)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename) && ServerToc.FileExists(*StorageServerFilename))
	{
		return;
	}
	LowerLevel->SetTimeStamp(Filename, DateTime);
}

IFileHandle* FStorageServerPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	TStringBuilder<1024> StorageServerFilename;
	if (MakeStorageServerPath(Filename, StorageServerFilename) && ServerToc.FileExists(*StorageServerFilename))
	{
		return nullptr;
	}
	return LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
}

bool FStorageServerPlatformFile::CreateDirectory(const TCHAR* Directory)
{
	TStringBuilder<1024> StorageServerDirectory;
	if (MakeStorageServerPath(Directory, StorageServerDirectory) && ServerToc.DirectoryExists(*StorageServerDirectory))
	{
		return true;
	}
	return LowerLevel->CreateDirectory(Directory);
}

bool FStorageServerPlatformFile::DeleteDirectory(const TCHAR* Directory)
{
	TStringBuilder<1024> StorageServerDirectory;
	if (MakeStorageServerPath(Directory, StorageServerDirectory) && ServerToc.DirectoryExists(*StorageServerDirectory))
	{
		return false;
	}
	return LowerLevel->DeleteDirectory(Directory);
}

FString FStorageServerPlatformFile::ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename)
{
#if PLATFORM_DESKTOP && (UE_GAME || UE_SERVER)
	TStringBuilder<1024> Result;

	// New code should not end up in here and should instead be written in such a
	// way that data can be served from a (remote) server.

	// Some data must exist in files on disk such that it can be accessed by external
	// APIs. Any such data required by a title should have been written to Saved/Cooked
	// at cook time. If a file prefix with UE's canonical ../../../ is requested we
	// look inside Saved/Cooked. A read-only filesystem overlay if you will.

	static FString* CookedDir = nullptr;
	if (CookedDir == nullptr)
	{
		static FString Inner;
		CookedDir = &Inner;

		Result << *FPaths::ProjectDir();
		Result << TEXT("Saved/Cooked/");
		Result << FPlatformProperties::PlatformName();
		Result << TEXT("/");
		Inner = Result.ToString();
	}
	else
	{
		Result << *(*CookedDir);
	}

	const TCHAR* DotSlashSkip = Filename;
	for (; *DotSlashSkip == '.' || *DotSlashSkip == '/'; ++DotSlashSkip);

	if (PTRINT(DotSlashSkip - Filename) == 9) // 9 == ../../../
	{
		Result << DotSlashSkip;
		if (LowerLevel->FileExists(Result.ToString()))
		{
			return FString(Result.GetData(), Result.Len());
		}
	}
#endif

	return LowerLevel->ConvertToAbsolutePathForExternalAppForRead(Filename);
}

bool FStorageServerPlatformFile::MakeStorageServerPath(const TCHAR* LocalFilenameOrDirectory, FStringBuilderBase& OutPath) const
{
	FStringView LocalEngineDirView(FPlatformMisc::EngineDir());
	FStringView LocalProjectDirView(FPlatformMisc::ProjectDir());
	FStringView LocalFilenameOrDirectoryView(LocalFilenameOrDirectory);
	bool bValid = false;

	if (LocalFilenameOrDirectoryView.StartsWith(LocalEngineDirView, ESearchCase::IgnoreCase))
	{
		OutPath.Append(ServerEngineDirView);
		OutPath.Append(LocalFilenameOrDirectoryView.RightChop(LocalEngineDirView.Len()));
		bValid = true;
	}
	else if (LocalFilenameOrDirectoryView.StartsWith(LocalProjectDirView, ESearchCase::IgnoreCase))
	{
		OutPath.Append(ServerProjectDirView);
		OutPath.Append(LocalFilenameOrDirectoryView.RightChop(LocalProjectDirView.Len()));
		bValid = true;
	}

	if (bValid)
	{
		Algo::Replace(MakeArrayView(OutPath), '\\', '/');
		OutPath.RemoveSuffix(LocalFilenameOrDirectoryView.EndsWith('/') ? 1 : 0);
	}

	return bValid;
}

bool FStorageServerPlatformFile::MakeLocalPath(const TCHAR* ServerFilenameOrDirectory, FStringBuilderBase& OutPath) const
{
	FStringView ServerFilenameOrDirectoryView(ServerFilenameOrDirectory);
	if (ServerFilenameOrDirectoryView.StartsWith(ServerEngineDirView, ESearchCase::IgnoreCase))
	{
		OutPath.Append(FPlatformMisc::EngineDir());
		OutPath.Append(ServerFilenameOrDirectoryView.RightChop(ServerEngineDirView.Len()));
		return true;
	}
	else if (ServerFilenameOrDirectoryView.StartsWith(ServerProjectDirView, ESearchCase::IgnoreCase))
	{
		OutPath.Append(FPlatformMisc::ProjectDir());
		OutPath.Append(ServerFilenameOrDirectoryView.RightChop(ServerProjectDirView.Len()));
		return true;
	}
	return false;
}

bool FStorageServerPlatformFile::SendGetFileListMessage()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerPlatformFileGetFileList);
	
	Connection->FileManifestRequest([&](FIoChunkId Id, FStringView Path)
	{
		ServerToc.AddFile(Id, Path);
	});

	return true;
}

FFileStatData FStorageServerPlatformFile::SendGetStatDataMessage(const FIoChunkId& FileChunkId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerPlatformFileGetStatData);
	const int64 FileSize = Connection->ChunkSizeRequest(FileChunkId);
	if (FileSize < 0)
	{
		return FFileStatData();
	}

	FDateTime CreationTime = FDateTime::Now();
	FDateTime AccessTime = FDateTime::Now();
	FDateTime ModificationTime = FDateTime::Now();

	return FFileStatData(CreationTime, AccessTime, ModificationTime, FileSize, false, true);
}

int64 FStorageServerPlatformFile::SendReadMessage(uint8* Destination, const FIoChunkId& FileChunkId, int64 Offset, int64 BytesToRead)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerPlatformFileRead);
	int64 BytesRead = 0;
	Connection->ReadChunkRequest(FileChunkId, Offset, BytesToRead, [Destination, Offset, BytesToRead, &BytesRead](FStorageServerResponse& Response)
	{
		BytesRead = Response.SerializeChunkTo(MakeMemoryView(Destination, BytesToRead), Offset);
	});
	return BytesRead;
}

bool FStorageServerPlatformFile::SendMessageToServer(const TCHAR* Message, IPlatformFile::IFileServerMessageHandler* Handler)
{
#if WITH_COTF
	if (!CookOnTheFlyServerConnection->IsConnected())
	{
		return false;
	}
	if (FCString::Stricmp(Message, TEXT("RecompileShaders")) == 0)
	{
		UE::Cook::FCookOnTheFlyRequest Request(UE::Cook::ECookOnTheFlyMessage::RecompileShaders);
		{
			TUniquePtr<FArchive> Ar = Request.WriteBody();
			Handler->FillPayload(*Ar);
		}

		UE::Cook::FCookOnTheFlyResponse Response = CookOnTheFlyServerConnection->SendRequest(Request).Get();
		if (Response.IsOk())
		{
			TUniquePtr<FArchive> Ar = Response.ReadBody();
			Handler->ProcessResponse(*Ar);
		}

		return Response.IsOk();
	}
#endif
	return false;
}

#if WITH_COTF
void FStorageServerPlatformFile::OnCookOnTheFlyMessage(const UE::Cook::FCookOnTheFlyMessage& Message)
{
	switch (Message.GetHeader().MessageType)
	{
		case UE::Cook::ECookOnTheFlyMessage::FilesAdded:
		{
			UE_LOG(LogCookOnTheFly, Verbose, TEXT("Received '%s' message"), LexToString(Message.GetHeader().MessageType));

			TArray<FString> Filenames;
			TArray<FIoChunkId> ChunkIds;

			{
				TUniquePtr<FArchive> Ar = Message.ReadBody();
				*Ar << Filenames;
				*Ar << ChunkIds;
			}

			check(Filenames.Num() == ChunkIds.Num());

			for (int32 Idx = 0, Num = Filenames.Num(); Idx < Num; ++Idx)
			{
				UE_LOG(LogCookOnTheFly, Verbose, TEXT("Adding file '%s'"), *Filenames[Idx]);
				ServerToc.AddFile(ChunkIds[Idx], Filenames[Idx]);
			}

			break;
		}
	}
}
#endif

class FStorageServerClientFileModule
	: public IPlatformFileModule
{
public:

	virtual IPlatformFile* GetPlatformFile() override
	{
		static TUniquePtr<IPlatformFile> AutoDestroySingleton = MakeUnique<FStorageServerPlatformFile>();
		return AutoDestroySingleton.Get();
	}
};

IMPLEMENT_MODULE(FStorageServerClientFileModule, StorageServerClient);

#endif
