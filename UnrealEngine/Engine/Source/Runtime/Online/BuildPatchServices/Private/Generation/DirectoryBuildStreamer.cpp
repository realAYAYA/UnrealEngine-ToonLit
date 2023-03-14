// Copyright Epic Games, Inc. All Rights Reserved.
#include "Generation/BuildStreamer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/ScopeLock.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "Core/RingBuffer.h"
#include "Common/FileSystem.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBuildStreamer, Log, All);
DEFINE_LOG_CATEGORY(LogBuildStreamer);

namespace BuildPatchServices
{
	// Buffer sizes
	enum
	{
		FileBufferSize = 1024 * 1024 * 10,      // 10 MiB
		StreamBufferSize = 1024 * 1024 * 100    // 100 MiB
	};

	static bool IsUnixExecutable(const TCHAR* Filename)
	{
#if PLATFORM_MAC
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(Filename), &FileInfo) == 0)
		{
			return (FileInfo.st_mode & S_IXUSR) != 0;
		}
#endif
		return false;
	}

	static FString GetSymlinkTarget(const TCHAR* Filename)
	{
#if PLATFORM_MAC
		ANSICHAR SymlinkTarget[MAC_MAX_PATH] = { 0 };
		if (readlink(TCHAR_TO_UTF8(Filename), SymlinkTarget, MAC_MAX_PATH) != -1)
		{
			return UTF8_TO_TCHAR(SymlinkTarget);
		}
#endif
		return TEXT("");
	}

	class FDataStream
	{
	public:
		FDataStream();
		~FDataStream();
		void Clear();
		uint32 FreeSpace() const;
		uint32 UsedSpace() const;
		uint64 TotalDataPushed() const;
		void EnqueueData(const uint8* Buffer, const uint32& Len);
		uint32 DequeueData(uint8* Buffer, const uint32& ReqSize, bool WaitForData);
		bool IsEndOfStream() const;
		void SetEndOfStream();

	private:
		mutable FCriticalSection BuildDataStreamCS;
		TRingBuffer<uint8> BuildDataStream;
		FThreadSafeBool EndOfStream;
	};

	class FDirectoryBuildStreamer
		: public IDirectoryBuildStreamer
	{

	public:
		FDirectoryBuildStreamer(FDirectoryBuildStreamerConfig Config, FDirectoryBuildStreamerDependencies Dependencies);
		virtual ~FDirectoryBuildStreamer();

		// IBuildStreamer interface begin.
		virtual uint32 DequeueData(uint8* Buffer, uint32 ReqSize, bool WaitForData = true) override;
		virtual bool IsEndOfData() const override;
		// IBuildStreamer interface end.

		// IDirectoryBuildStreamer interface begin.
		virtual bool GetFileSpan(uint64 StartingIdx, FFileSpan& FileSpan) const override;
		virtual TArray<FString> GetEmptyFiles() const override;
		virtual TArray<FString> GetAllFilenames() const override;
		virtual uint64 GetBuildSize() const override;
		virtual TArray<FFileSpan> GetAllFiles() const override;
		virtual bool HasAborted() const override;
		// IDirectoryBuildStreamer interface end.

	private:
		void ReadData();
		void AddFile(FFileSpan FileSpan);
		void AddEmptyFile(FString Filename);
		void SetFileHash(uint64 StartIdx, FSHA1& FileHash);
		void ReadInputFileList(TArray<FString>& AllFiles);
		void StripIgnoredFiles(TArray<FString>& AllFiles);
		void SetEnumeratedFiles(const TArray<FString>& AllFiles);

	private:
		const FDirectoryBuildStreamerConfig Config;
		const FDirectoryBuildStreamerDependencies Dependencies;
		FDataStream DataStream;
		mutable FCriticalSection FilesCS;
		TMap<uint64, FFileSpan> Files;
		mutable FCriticalSection EnumeratedFilesCS;
		TArray<FString> EnumeratedFiles;
		TSet<FString> EmptyFiles;
		mutable FSHA1 EmptyHasher; // FSHA1 is not const correct.
		TFuture<void> Future;
		FThreadSafeBool bShouldAbort;
		FThreadSafeBool bFilesEnumerated;
	};

	FDataStream::FDataStream()
		: BuildDataStream(BuildPatchServices::StreamBufferSize)
		, EndOfStream(false)
	{
	}

	FDataStream::~FDataStream()
	{
	}

	void FDataStream::Clear()
	{
		BuildDataStreamCS.Lock();
		BuildDataStream.Empty();
		BuildDataStreamCS.Unlock();
	}

	uint32 FDataStream::FreeSpace() const
	{
		uint32 rtn;
		BuildDataStreamCS.Lock();
		rtn = BuildDataStream.RingDataSize() - BuildDataStream.RingDataUsage();
		BuildDataStreamCS.Unlock();
		return rtn;
	}

	uint32 FDataStream::UsedSpace() const
	{
		uint32 rtn;
		BuildDataStreamCS.Lock();
		rtn = BuildDataStream.RingDataUsage();
		BuildDataStreamCS.Unlock();
		return rtn;
	}

	uint64 FDataStream::TotalDataPushed() const
	{
		uint64 rtn;
		BuildDataStreamCS.Lock();
		rtn = BuildDataStream.TotalDataPushed();
		BuildDataStreamCS.Unlock();
		return rtn;
	}

	void FDataStream::EnqueueData(const uint8* Buffer, const uint32& Len)
	{
		checkf(!EndOfStream, TEXT("More data was added after specifying the end of stream"));
		BuildDataStreamCS.Lock();
		while (FreeSpace() < Len)
		{
			BuildDataStreamCS.Unlock();
			FPlatformProcess::Sleep(0.01f);
			BuildDataStreamCS.Lock();
		}
		BuildDataStream.Enqueue(Buffer, Len);
		BuildDataStreamCS.Unlock();
	}

	uint32 FDataStream::DequeueData(uint8* Buffer, const uint32& ReqSize, bool WaitForData)
	{
		BuildDataStreamCS.Lock();
		uint32 ReadLen = BuildDataStream.Dequeue(Buffer, ReqSize);
		if (WaitForData && ReadLen < ReqSize)
		{
			while (ReadLen < ReqSize && !EndOfStream)
			{
				BuildDataStreamCS.Unlock();
				FPlatformProcess::Sleep(0.01f);
				BuildDataStreamCS.Lock();
				ReadLen += BuildDataStream.Dequeue(&Buffer[ReadLen], ReqSize - ReadLen);
			}
		}
		BuildDataStreamCS.Unlock();
		return ReadLen;
	}

	bool FDataStream::IsEndOfStream() const
	{
		return EndOfStream;
	}

	void FDataStream::SetEndOfStream()
	{
		EndOfStream = true;
	}

	FDirectoryBuildStreamer::FDirectoryBuildStreamer(FDirectoryBuildStreamerConfig InConfig, FDirectoryBuildStreamerDependencies InDependencies)
		: Config(MoveTemp(InConfig))
		, Dependencies(MoveTemp(InDependencies))
		, bShouldAbort(false)
		, bFilesEnumerated(false)
	{
		EmptyHasher.Final();
		TFunction<void()> Task = [this]() { ReadData(); };
		Future = Async(EAsyncExecution::Thread, MoveTemp(Task));
	}

	FDirectoryBuildStreamer::~FDirectoryBuildStreamer()
	{
		bShouldAbort = true;
		Future.Wait();
	}

	uint32 FDirectoryBuildStreamer::DequeueData(uint8* Buffer, uint32 ReqSize, bool WaitForData /*= true*/)
	{
		return DataStream.DequeueData(Buffer, ReqSize, WaitForData);
	}

	bool FDirectoryBuildStreamer::GetFileSpan(uint64 StartingIdx, FFileSpan& FileSpan) const
	{
		FScopeLock ScopeLock(&FilesCS);
		if (Files.Contains(StartingIdx))
		{
			FileSpan = Files[StartingIdx];
			return true;
		}
		return false;
	}

	TArray<FString> FDirectoryBuildStreamer::GetEmptyFiles() const
	{
		FScopeLock ScopeLock(&FilesCS);
		return EmptyFiles.Array();
	}

	TArray<FString> FDirectoryBuildStreamer::GetAllFilenames() const
	{
		while (!bFilesEnumerated && !bShouldAbort)
		{
			FPlatformProcess::Sleep(0.1f);
		}
		TArray<FString> AllFiles;
		EnumeratedFilesCS.Lock();
		AllFiles.Append(EnumeratedFiles);
		EnumeratedFilesCS.Unlock();
		return AllFiles;
	}

	bool FDirectoryBuildStreamer::IsEndOfData() const
	{
		return DataStream.IsEndOfStream() && DataStream.UsedSpace() == 0;
	}

	uint64 FDirectoryBuildStreamer::GetBuildSize() const
	{
		check(DataStream.IsEndOfStream());
		return DataStream.TotalDataPushed();
	}

	TArray<FFileSpan> FDirectoryBuildStreamer::GetAllFiles() const
	{
		TArray<FFileSpan> AllFiles;
		check(DataStream.IsEndOfStream());
		FScopeLock ScopeLock(&FilesCS);
		Files.GenerateValueArray(AllFiles);
		for(const FString& EmptyFile : EmptyFiles)
		{
			FFileSpan FileSpan(EmptyFile, 0, 0, false, TEXT(""));
			EmptyHasher.GetHash(FileSpan.SHAHash.Hash);
			AllFiles.Add(MoveTemp(FileSpan));
		}
		return AllFiles;
	}

	bool FDirectoryBuildStreamer::HasAborted() const
	{
		return bShouldAbort;
	}

	void FDirectoryBuildStreamer::ReadData()
	{
		// Stats
		volatile int64* StatFileOpenTime = Dependencies.StatsCollector->CreateStat(TEXT("Build Stream: Open Time"), EStatFormat::Timer);
		volatile int64* StatFileReadTime = Dependencies.StatsCollector->CreateStat(TEXT("Build Stream: Read Time"), EStatFormat::Timer);
		volatile int64* StatFileHashTime = Dependencies.StatsCollector->CreateStat(TEXT("Build Stream: Hash Time"), EStatFormat::Timer);
		volatile int64* StatDataEnqueueTime = Dependencies.StatsCollector->CreateStat(TEXT("Build Stream: Enqueue Time"), EStatFormat::Timer);
		volatile int64* StatDataAccessSpeed = Dependencies.StatsCollector->CreateStat(TEXT("Build Stream: Data Access Speed"), EStatFormat::DataSpeed);
		volatile int64* StatPotentialThroughput = Dependencies.StatsCollector->CreateStat(TEXT("Build Stream: Potential Throughput"), EStatFormat::DataSpeed);
		volatile int64* StatTotalDataRead = Dependencies.StatsCollector->CreateStat(TEXT("Build Stream: Total Data Read"), EStatFormat::DataSize);
		uint64 TempValue;

		// Clear the build stream
		DataStream.Clear();

		// Enumerate build files
		TArray<FString> AllFiles;
		uint32 FileEnumerationStart = FStatsCollector::GetCycles();
		if (Config.InputListFile.IsEmpty())
		{
			Dependencies.FileSystem->FindFilesRecursively(AllFiles, *Config.BuildRoot);
		}
		else
		{
			ReadInputFileList(AllFiles);
		}
		uint32 FileEnumerationEnd = FStatsCollector::GetCycles();
		uint32 FileEnumerationTime = FileEnumerationEnd - FileEnumerationStart;
		UE_LOG(LogBuildStreamer, Log, TEXT("Enumerated %d files in %s"), AllFiles.Num(), *FPlatformTime::PrettyTime(FStatsCollector::CyclesToSeconds(FileEnumerationTime)));

		// Remove the files that appear in the ignore list
		AllFiles.Sort();
		StripIgnoredFiles(AllFiles);

		// Preserve our sorted, stripped list of files, so it can be retrieved later via GetAllFilenames().
		SetEnumeratedFiles(AllFiles);

		// Track file hashes
		FSHA1 FileHash;

		// Allocate our file read buffer
		uint8* FileReadBuffer = new uint8[BuildPatchServices::FileBufferSize];

		for (FString& SourceFile : AllFiles)
		{
			if (bShouldAbort)
			{
				break;
			}

			// Read the file
			FStatsCollector::AccumulateTimeBegin(TempValue);
			TUniquePtr<FArchive> FileReader = Dependencies.FileSystem->CreateFileReader(*SourceFile);
			bool bIsUnixExe = IsUnixExecutable(*SourceFile);
			FString SymlinkTarget = GetSymlinkTarget(*SourceFile);
			FStatsCollector::AccumulateTimeEnd(StatFileOpenTime, TempValue);
			// Not being able to load a required file from the build would be fatal, hard fault.
			if(!FileReader.IsValid())
			{
				UE_LOG(LogBuildStreamer, Error, TEXT("Failed to open file reader for %s"), *SourceFile);
				bShouldAbort = true;
				break;
			}

			// Make SourceFile the format we want it in and start a new file.
			FPaths::MakePathRelativeTo(SourceFile, *(Config.BuildRoot + TEXT("/")));
			int64 FileSize = FileReader->TotalSize();
			// Process files that have bytes.
			if (FileSize > 0)
			{
				FileHash.Reset();
				uint64 FileStartIdx = DataStream.TotalDataPushed();
				AddFile(FFileSpan(SourceFile, FileSize, FileStartIdx, bIsUnixExe, SymlinkTarget));
				while (!FileReader->AtEnd() && !bShouldAbort)
				{
					// Read data file
					const int64 SizeLeft = FileSize - FileReader->Tell();
					const uint32 ReadLen = FMath::Min< int64 >(BuildPatchServices::FileBufferSize, SizeLeft);
					FStatsCollector::AccumulateTimeBegin(TempValue);
					FileReader->Serialize(FileReadBuffer, ReadLen);
					FStatsCollector::AccumulateTimeEnd(StatFileReadTime, TempValue);
					FStatsCollector::Accumulate(StatTotalDataRead, ReadLen);

					// Hash data file
					FStatsCollector::AccumulateTimeBegin(TempValue);
					FileHash.Update(FileReadBuffer, ReadLen);
					FStatsCollector::AccumulateTimeEnd(StatFileHashTime, TempValue);

					// Copy into data stream
					FStatsCollector::AccumulateTimeBegin(TempValue);
					DataStream.EnqueueData(FileReadBuffer, ReadLen);
					FStatsCollector::AccumulateTimeEnd(StatDataEnqueueTime, TempValue);

					// Calculate other stats
					FStatsCollector::Set(StatDataAccessSpeed, *StatTotalDataRead / FStatsCollector::CyclesToSeconds(*StatFileOpenTime + *StatFileReadTime));
					FStatsCollector::Set(StatPotentialThroughput, *StatTotalDataRead / FStatsCollector::CyclesToSeconds(*StatFileOpenTime + *StatFileReadTime + *StatFileHashTime));
				}
				FileHash.Final();
				SetFileHash(FileStartIdx, FileHash);
			}
			// Special case zero byte files.
			else if (FileSize == 0)
			{
				AddEmptyFile(SourceFile);
			}
			FileReader->Close();
		}

		// Mark end of build
		DataStream.SetEndOfStream();

		// Deallocate our file read buffer
		delete[] FileReadBuffer;
	}

	void FDirectoryBuildStreamer::AddFile(FFileSpan FileSpan)
	{
		FScopeLock ScopeLock(&FilesCS);
		Files.Add(FileSpan.StartIdx, MoveTemp(FileSpan));
	}

	void FDirectoryBuildStreamer::AddEmptyFile(FString Filename)
	{
		FScopeLock ScopeLock(&FilesCS);
		EmptyFiles.Add(MoveTemp(Filename));
	}

	void FDirectoryBuildStreamer::SetFileHash(uint64 StartIdx, FSHA1& FileHash)
	{
		FScopeLock ScopeLock(&FilesCS);
		FileHash.GetHash(Files[StartIdx].SHAHash.Hash);
	}

	void FDirectoryBuildStreamer::ReadInputFileList(TArray<FString>& AllFiles)
	{
		FString InputFileList;
		FFileHelper::LoadFileToString(InputFileList, *Config.InputListFile);

		TArray<FString> InputFiles;
		InputFileList.ParseIntoArrayLines(InputFiles, true);

		for (FString& InputFile : InputFiles)
		{
			InputFile.TrimStartAndEndInline();
			if (InputFile.Len() > 0)
			{
				FString FullInputFile = Config.BuildRoot / InputFile;
				FPaths::NormalizeFilename(FullInputFile);
				AllFiles.Add(FullInputFile);
			}
		}
	}

	void FDirectoryBuildStreamer::StripIgnoredFiles(TArray<FString>& AllFiles)
	{
		struct FRemoveMatchingStrings
		{
			const TSet<FString>& IgnoreList;
			FRemoveMatchingStrings(const TSet<FString>& InIgnoreList)
				: IgnoreList(InIgnoreList) {}

			bool operator()(const FString& RemovalCandidate) const
			{
				const bool bRemove = IgnoreList.Contains(RemovalCandidate);
				if (bRemove)
				{
					UE_LOG(LogBuildStreamer, Log, TEXT("    - %s"), *RemovalCandidate);
				}
				return bRemove;
			}
		};

		UE_LOG(LogBuildStreamer, Log, TEXT("Stripping ignorable files"));
		const int32 OriginalNumFiles = AllFiles.Num();
		FString IgnoreFileList = TEXT("");
		FFileHelper::LoadFileToString(IgnoreFileList, *Config.IgnoreListFile);
		TArray< FString > IgnoreFiles;
		IgnoreFileList.ParseIntoArrayLines(IgnoreFiles, true);

		// Normalize all paths first
		for (FString& Filename : AllFiles)
		{
			FPaths::NormalizeFilename(Filename);
		}
		for (FString& Filename : IgnoreFiles)
		{
			int32 TabLocation = Filename.Find(TEXT("\t"), ESearchCase::CaseSensitive);
			if (TabLocation != INDEX_NONE)
			{
				// Strip tab separated timestamp if it exists
				Filename.LeftInline(TabLocation, false);
			}
			Filename = Config.BuildRoot / Filename;
			FPaths::NormalizeFilename(Filename);
		}

		// Convert ignore list to set
		TSet<FString> IgnoreSet(MoveTemp(IgnoreFiles));

		// Filter file list
		FRemoveMatchingStrings FileFilter(IgnoreSet);
		AllFiles.RemoveAll(FileFilter);

		const int32 NewNumFiles = AllFiles.Num();
		UE_LOG(LogBuildStreamer, Log, TEXT("Stripped %d ignorable file(s)"), (OriginalNumFiles - NewNumFiles));
	}

	void FDirectoryBuildStreamer::SetEnumeratedFiles(const TArray<FString>& AllFiles)
	{
		FScopeLock ScopeLock(&EnumeratedFilesCS);
		EnumeratedFiles = AllFiles;
		bFilesEnumerated = true;
	}

	IDirectoryBuildStreamer* FBuildStreamerFactory::Create(FDirectoryBuildStreamerConfig Config, FDirectoryBuildStreamerDependencies Dependencies)
	{
		check(Dependencies.StatsCollector != nullptr);
		check(Dependencies.FileSystem != nullptr);
		return new FDirectoryBuildStreamer(MoveTemp(Config), MoveTemp(Dependencies));
	}
}
