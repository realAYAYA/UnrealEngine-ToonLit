// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Streams.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/Model.h"
#include "MuR/MutableMemory.h"
#include "MuR/RefCounted.h"
#include "MuT/Platform.h"
#include "MuT/StreamsPrivate.h"

#define MUTABLE_STREAM_BUFFER_SIZE 0x10000

namespace mu
{
	InputFileStream::InputFileStream( const char* strFile )
	{
		m_pD = new Private();

		m_pD->m_pFile = mutable_fopen( strFile, "rb" );
		m_pD->m_size = 0;
		m_pD->m_pos = 0;
        m_pD->m_readBytes = 0;

        m_pD->m_buffer = (uint8_t*)mutable_malloc_aligned(MUTABLE_STREAM_BUFFER_SIZE,16);
	}


	//---------------------------------------------------------------------------------------------
	InputFileStream::~InputFileStream()
	{
        if ( m_pD->m_pFile )
        {
            fclose( m_pD->m_pFile );
        }

        mutable_free_aligned( m_pD->m_buffer, MUTABLE_STREAM_BUFFER_SIZE );
        m_pD->m_buffer = nullptr;

        delete m_pD;
		m_pD = nullptr;
	}
    
    
	//---------------------------------------------------------------------------------------------
	bool InputFileStream::IsOpen() const
    {
        return m_pD->m_pFile!=0;
    }
    

    //---------------------------------------------------------------------------------------------
	uint64 InputFileStream::GetReadBytes() const
    {
        return m_pD->m_readBytes;
    }
    

    //---------------------------------------------------------------------------------------------
	uint64 InputFileStream::Tell() const
    {
        size_t bufferOffset = m_pD->m_size - m_pD->m_pos;
        int64_t filePos = mutable_ftell( m_pD->m_pFile );
        check(filePos>0);
        size_t pos = size_t(filePos) - bufferOffset;
        return pos;
    }


    //---------------------------------------------------------------------------------------------
    void InputFileStream::Seek( uint64 position )
    {
        int64_t result = mutable_fseek( m_pD->m_pFile, int64_t(position), SEEK_SET );
        check(result==0);
        (void)result;

        m_pD->m_size = 0;
        m_pD->m_pos = 0;
    }


	//---------------------------------------------------------------------------------------------
    void InputFileStream::Read( void* pData, uint64 size )
	{
        uint8_t* pDest = (uint8_t*) pData;
        while (size && m_pD->m_pFile )
		{			
			int available = FMath::Min( int(m_pD->m_size) - int(m_pD->m_pos), int(size) );

			// Copy from what is already loaded
			if ( available>0 )
			{
                FMemory::Memcpy( pDest, m_pD->m_buffer+m_pD->m_pos, available );
				m_pD->m_pos += available;
				size -= available;
				pDest += available;
			}

			// Load more data if necessary
			if ( m_pD->m_pos == m_pD->m_size && size )
			{
                m_pD->m_size = fread( m_pD->m_buffer, 1, MUTABLE_STREAM_BUFFER_SIZE, m_pD->m_pFile );
                check( !ferror( m_pD->m_pFile ) );
                check( m_pD->m_size );
				m_pD->m_pos = 0;
                m_pD->m_readBytes += m_pD->m_size;
            }
		}

	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	OutputFileStream::OutputFileStream( const char* strFile )
	{
		m_pD = new Private();
		m_pD->m_pFile = mutable_fopen( strFile, "wb" );
		m_pD->m_pos = 0;
        m_pD->m_buffer = (uint8_t*)mutable_malloc_aligned(MUTABLE_STREAM_BUFFER_SIZE,16);

		check( m_pD->m_pFile );
	}


	//---------------------------------------------------------------------------------------------
    void OutputFileStream::Write( const void* pData, uint64 size )
	{
        const uint8_t* pSrc = (const uint8_t*) pData;

		while (size)
		{
            int available = FMath::Min( int(MUTABLE_STREAM_BUFFER_SIZE) - int(m_pD->m_pos), int(size) );

			// Copy from what fits in the current buffer
			if ( available>0 )
			{
                memcpy( m_pD->m_buffer+m_pD->m_pos, pSrc, available );
				m_pD->m_pos += available;
				size -= available;
				pSrc += available;
			}

			// Flush the data if necessary
            if ( m_pD->m_pos == MUTABLE_STREAM_BUFFER_SIZE )
			{
				Flush();
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void OutputFileStream::Flush()
	{
		if ( m_pD->m_pos )
		{
            std::size_t count = fwrite( m_pD->m_buffer,
										1,
										m_pD->m_pos,
										m_pD->m_pFile );

			check( count == m_pD->m_pos );
            (void)count;
        }

		m_pD->m_pos = 0;
	}


	//---------------------------------------------------------------------------------------------
	OutputFileStream::~OutputFileStream()
	{
		Flush();

		fclose( m_pD->m_pFile );

        mutable_free_aligned( m_pD->m_buffer, MUTABLE_STREAM_BUFFER_SIZE );
        m_pD->m_buffer = nullptr;

        delete m_pD;
		m_pD = 0;
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    class FileModelStreamer::Private : public Base
    {
    public:
        OPERATION_ID m_lastOperationID = 0;

        IFileHandle* m_writeFile = nullptr;

        // For debug
        size_t m_totalWritten = 0;

        static inline FString MakeDataFileName(
                const FString& strModelName,
                uint64_t key0
                )
        {
			FString name = strModelName;
			int extensionPos = 0;
			if (name.FindLastChar('.', extensionPos))
			{
				name = name.Left(extensionPos);
			}

            FString temp = FString::Printf(TEXT(".%08x-%08x.mutable_data"),
                               uint32_t((key0>>32) & 0xffffffff), uint32(key0 & 0xffffffff) );
            name += temp;

            return name;
        }


    };


    //---------------------------------------------------------------------------------------------
    FileModelStreamer::FileModelStreamer()
    {
        m_pD = new Private();
    }


    //---------------------------------------------------------------------------------------------
    FileModelStreamer::~FileModelStreamer()
    {
        delete m_pD;
    }


    //---------------------------------------------------------------------------------------------
    void FileModelStreamer::GetFileName( const FString& strModelName, uint64 key0, FString& outFileName )
    {
		outFileName = Private::MakeDataFileName( strModelName, key0 );
    }


    //---------------------------------------------------------------------------------------------
    ModelStreamer::OPERATION_ID FileModelStreamer::BeginReadBlock(const mu::Model* Model, uint64 key0, void* pBuffer, uint64 size)
    {
        //UE_LOG(LogMutableCore,Log,"Streamer OpenRead");

		check(Model);

		OPERATION_ID id = ++m_pD->m_lastOperationID;

        IFileHandle* pFile = nullptr;

        if ( !key0 )
        {
            // It is the main model file
            pFile = FPlatformFileManager::Get().GetPlatformFile().OpenRead( *FString(Model->GetLocation()) );

            // Read the file identifying tag.
            char tag[4];
            size_t read = pFile->Read((uint8*)&tag[0], 4);
            check( read==4 );
            check( !memcmp(tag,MUTABLE_COMPILED_MODEL_FILETAG,4) );

            uint32_t codeVersion = 0;
            read = pFile->Read( (uint8*)&codeVersion, sizeof(uint32_t) );
            check( read==4 );
            check( codeVersion == MUTABLE_COMPILED_MODEL_CODE_VERSION  );

        }
        else
        {
            // It is a data file from a split model
            FString name = m_pD->MakeDataFileName( Model->GetLocation(), key0);
            pFile = FPlatformFileManager::Get().GetPlatformFile().OpenRead( *name );
        }

        if( !pFile )
		{
			return -1;
		}

        size_t read = 1;
        size_t totalRead = 0;
        while (read && totalRead<size)
        {
            read = pFile->Read( ((uint8*)pBuffer)+totalRead, (size_t)size-totalRead  );
            totalRead += read;
        }

		delete pFile;

        check( totalRead==size );

        return id;
    }


    //---------------------------------------------------------------------------------------------
    bool FileModelStreamer::IsReadCompleted( OPERATION_ID )
    {
        return true;
    }


    //---------------------------------------------------------------------------------------------
    void FileModelStreamer::EndRead( OPERATION_ID )
    {
	}


    //---------------------------------------------------------------------------------------------
    void FileModelStreamer::OpenWriteFile( const char* strModelName, uint64 key0 )
    {
        check( !m_pD->m_writeFile );

        IFileHandle* pFile = nullptr;
        m_pD->m_totalWritten = 0;

        if ( !key0 )
        {
            // It is the main model file
            pFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite( *FString(strModelName) );
			check(pFile);
            if (!pFile)
                return;

            // Write the file identifying tag.
            bool success = pFile->Write( (const uint8*)MUTABLE_COMPILED_MODEL_FILETAG, 4 );
            check(success);

            uint32_t codeVersion = MUTABLE_COMPILED_MODEL_CODE_VERSION;
			success = pFile->Write( (const uint8*)&codeVersion, sizeof(uint32_t) );
            check(success);
        }
        else
        {
            // It is a data file from a split model
            FString name = m_pD->MakeDataFileName( strModelName, key0 );
            pFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite( *name );
        }

        check( pFile );

        m_pD->m_writeFile = pFile;
    }


    //---------------------------------------------------------------------------------------------
    void FileModelStreamer::Write( const void* pBuffer, uint64 size )
    {
        check( m_pD->m_writeFile );

        m_pD->m_writeFile->Write( ((const uint8*)pBuffer), size );
        m_pD->m_totalWritten += size;
    }


    //---------------------------------------------------------------------------------------------
    void FileModelStreamer::CloseWriteFile()
    {
        check( m_pD->m_writeFile );

        delete m_pD->m_writeFile;
        m_pD->m_writeFile = 0;
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    Ptr<ResourceProxy<Image>> ProxyFactoryFiles::NewImageProxy(InputArchive& arch)
    {        
        Ptr<Image> t = Image::StaticUnserialise( arch );
        return new ResourceProxyTempFile<Image>(t.get());
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    class ProxyFactoryMutableSourceFile::Private : public Base
    {
    public:
        // \todo: pimpl this out
        InputFileStream* m_pStream = nullptr;
        std::string m_filename;
    };


    //---------------------------------------------------------------------------------------------
    ProxyFactoryMutableSourceFile::ProxyFactoryMutableSourceFile( const char* strFileName, 
                                                                  InputFileStream* pStream )
    {
        m_pD = new Private();

        m_pD->m_pStream = pStream;
        m_pD->m_filename = strFileName;
    }


    //---------------------------------------------------------------------------------------------
    ProxyFactoryMutableSourceFile::~ProxyFactoryMutableSourceFile()
    {
        delete m_pD;
        m_pD = nullptr;
    }


    //---------------------------------------------------------------------------------------------
    Ptr<ResourceProxy<Image>> ProxyFactoryMutableSourceFile::NewImageProxy(InputArchive& arch)
    {
        // assert: the archive should beusing m_pStream
        size_t position = m_pD->m_pStream->Tell();

        // we are unserialising just to skip the image. It would be much better if we had the size
        // and we could skip it.
        Ptr<Image> t = Image::StaticUnserialise( arch );

        return new ResourceProxyFile<Image>( m_pD->m_filename, position );
    }


}

