// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformProcess.h"
#include "Math/RandomStream.h"
#include "Misc/ScopeLock.h"

#include "MuT/Streams.h"
#include "MuR/Platform.h"
#include "MuR/Operations.h"
#include "MuR/MutableRuntimeModule.h"

#include <atomic>
#include <inttypes.h>

namespace mu
{


	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API InputFileStream::Private
	{
	public:

		TSharedPtr<IFileHandle> File;

		uint64 FileSize;
		uint64 BytesInBuffer;
		uint64 BufferPosition;
		uint64 FilePosition;

    private:
        TArray<uint8> Buffer;

        friend class InputFileStream;
    };


	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API OutputFileStream::Private
	{
	public:

		TSharedPtr<IFileHandle> File;

		uint64 BufferPosition;

    private:
		TArray<uint8> Buffer;

        friend class OutputFileStream;
    };


	/** Statistics about the proxy file usage. */
	struct FProxyFileContext
	{
		FProxyFileContext();

		/** Options */

		/** Minimum data size in bytes to dumpt it to the disk. */
		uint64 MinProxyFileSize = 1024 * 1024;

		/** When creating temporary files, number of retries in case the OS-level call fails. */
		uint64 MaxFileCreateAttempts = 256;

		/** Statistics */
		std::atomic<uint64> FilesWritten = 0;
		std::atomic<uint64> FilesRead = 0;
		std::atomic<uint64> BytesWritten = 0;
		std::atomic<uint64> BytesRead = 0;

		/** Internal data. */
		std::atomic<uint64> CurrentFileIndex = 0;
	};
    
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    template<class R>
    class MUTABLETOOLS_API ResourceProxyTempFile : public ResourceProxy<R>
    {
    private:
        Ptr<const R> Resource;
        FString FileName;
		uint64 FileSize = 0;
		FCriticalSection Mutex;

		FProxyFileContext& Options;
		
    public:
        ResourceProxyTempFile( const R* resource, FProxyFileContext& InOptions )
			: Options(InOptions)
        {
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

            if (!resource)
            {
                return;
            }

			FScopeLock Lock(&Mutex);

            OutputMemoryStream stream;
            OutputArchive arch(&stream);
            R::Serialise( resource, arch );

            if (stream.GetBufferSize()<=Options.MinProxyFileSize)
            {
                Resource = resource;
            }
            else
            {
                FileSize = stream.GetBufferSize();

				FString Prefix = FPlatformProcess::UserTempDir();

				uint32 PID = FPlatformProcess::GetCurrentProcessId();
				Prefix += FString::Printf(TEXT("mut.temp.%u"), PID);
								
				FString FinalTempPath;
				IFileHandle* ResourceFile = nullptr;
				uint64 AttemptCount = 0;
                while(!ResourceFile && AttemptCount< Options.MaxFileCreateAttempts)
                {
					uint64 ThisThreadFileIndex = Options.CurrentFileIndex.load();
					while (!Options.CurrentFileIndex.compare_exchange_strong(ThisThreadFileIndex, ThisThreadFileIndex + 1));

					FinalTempPath = Prefix +FString::Printf(TEXT(".%.16" PRIx64), ThisThreadFileIndex);
					ResourceFile = PlatformFile.OpenWrite(*FinalTempPath);
					++AttemptCount;
                }

				if (!ResourceFile)
				{
					UE_LOG(LogMutableCore, Error, TEXT("Failed to create temporary file. Disk full?"));
					check(false);
				}

				ResourceFile->Write( (const uint8*) stream.GetBuffer(), stream.GetBufferSize() );
				delete ResourceFile;

                FileName = FinalTempPath;
				Options.FilesWritten++;
				Options.BytesWritten += stream.GetBufferSize();
			}
        }

        ~ResourceProxyTempFile()
        {
			FScopeLock Lock(&Mutex);

            if (!FileName.IsEmpty())
            {
                // Delete temp file
				FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*FileName);
				FileName.Empty();
            }
        }

        Ptr<const R> Get() override
        {
			FScopeLock Lock(&Mutex);

            Ptr<const R> r;
            if (Resource)
            {
                // cached
                r = Resource;
            }
            else if (!FileName.IsEmpty())
            {
				TArray<char> buf;
				buf.SetNumUninitialized(FileSize);
				IFileHandle* resourceFile = FPlatformFileManager::Get().GetPlatformFile().OpenRead( *FileName);
                check(resourceFile);
                resourceFile->Read((uint8*)buf.GetData(), FileSize);
                delete resourceFile;

                InputMemoryStream stream( buf.GetData(), FileSize );
                InputArchive arch(&stream);
                r =  R::StaticUnserialise( arch );
			
				Options.FilesRead++;
				Options.BytesRead += FileSize;
			}
            return r;
        }

    };


    //---------------------------------------------------------------------------------------------
    //! Resource proxy for resources that are located in mutable_source files and have deferred
    //! access: they are not loaded immeditely, but only when needed.
    //---------------------------------------------------------------------------------------------
    template<class R>
    class MUTABLETOOLS_API ResourceProxyFile : public ResourceProxy<R>
    {
    private:
		FString FileName;
        uint64 FilePos = 0;
		FCriticalSection Mutex;

    public:
        ResourceProxyFile( const FString& InFileName, uint64 InFilePos )
        {
			FileName = InFileName;
            FilePos = InFilePos;
        }

        Ptr<const R> Get() override
        {
			FScopeLock Lock(&Mutex);

            Ptr<const R> r;
            if (!FileName.IsEmpty())
            {
                InputFileStream stream(FileName);
                stream.Seek( FilePos );
                InputArchive arch(&stream);
                r =  R::StaticUnserialise( arch );
            }
            return r;
        }

    };


}

//! Utility function
namespace {
template <class T> inline void hash_combine(uint64 &seed, const T &v) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
} // namespace


