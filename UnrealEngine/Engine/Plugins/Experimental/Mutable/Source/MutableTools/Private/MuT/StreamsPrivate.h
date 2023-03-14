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
#include "MuR/MemoryPrivate.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API InputFileStream::Private : public Base
	{
	public:

		FILE* m_pFile;

		uint64 m_size;
		uint64 m_pos;

		uint64 m_readBytes;

    private:
        uint8* m_buffer;

        friend class InputFileStream;
    };


	//---------------------------------------------------------------------------------------------
	class MUTABLETOOLS_API OutputFileStream::Private : public Base
	{
	public:

		FILE* m_pFile;

		uint64 m_pos;

    private:
        uint8* m_buffer;

        friend class OutputFileStream;
    };

    
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    template<class R>
    class MUTABLETOOLS_API ResourceProxyTempFile : public ResourceProxy<R>
    {
    private:
        Ptr<const R> m_resource;
        FString m_fileName;
		uint64 m_fileSize = 0;
		FCriticalSection Mutex;

    public:
        ResourceProxyTempFile( const R* resource )
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

            if (stream.GetBufferSize()<=256*1024)
            {
                m_resource = resource;
            }
            else
            {
                m_fileSize = stream.GetBufferSize();

				FString TempPath = FPlatformProcess::UserTempDir();
				TempPath += TEXT("mut.temp");

				FRandomStream RandomStream;
				RandomStream.GenerateNewSeed();
				
				auto prefix = TempPath;
				
				FString FinalTempPath;
				bool found = true;
                while(found)
                {
					FinalTempPath = TempPath+FString::Printf(TEXT(".%.8x.%.8x.%.8x.%.8x"), RandomStream.GetUnsignedInt(), RandomStream.GetUnsignedInt(), RandomStream.GetUnsignedInt(), RandomStream.GetUnsignedInt());
					found = PlatformFile.FileExists(*FinalTempPath);
                }
				IFileHandle* resourceFile = PlatformFile.OpenWrite(*FinalTempPath);

                check(resourceFile);
				resourceFile->Write( (const uint8*) stream.GetBuffer(), stream.GetBufferSize() );
				delete resourceFile;

                m_fileName = FinalTempPath;
            }
        }

        ~ResourceProxyTempFile()
        {
			FScopeLock Lock(&Mutex);

            if (!m_fileName.IsEmpty())
            {
                // Delete temp file
				FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*m_fileName);
                m_fileName.Empty();
            }
        }

        Ptr<const R> Get() override
        {
			FScopeLock Lock(&Mutex);

            Ptr<const R> r;
            if (m_resource)
            {
                // cached
                r = m_resource;
            }
            else if (!m_fileName.IsEmpty())
            {
				TArray<char> buf;
				buf.SetNumUninitialized(m_fileSize);
                auto resourceFile = FPlatformFileManager::Get().GetPlatformFile().OpenRead( *m_fileName );
                check(resourceFile);
                resourceFile->Read((uint8*)buf.GetData(), m_fileSize);
                delete resourceFile;

                InputMemoryStream stream( buf.GetData(), m_fileSize );
                InputArchive arch(&stream);
                r =  R::StaticUnserialise( arch );
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
        std::string m_fileName;
        uint64 m_filePos = 0;
		FCriticalSection Mutex;

    public:
        ResourceProxyFile( const std::string& fileName, uint64 filePos )
        {
			FScopeLock Lock(&Mutex);
			m_fileName = fileName;
            m_filePos = filePos;
        }

        Ptr<const R> Get() override
        {
			FScopeLock Lock(&Mutex);

            Ptr<const R> r;
            if (!m_fileName.empty())
            {
                InputFileStream stream( m_fileName.c_str() );
                stream.Seek( m_filePos );
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


