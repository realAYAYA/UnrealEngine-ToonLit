// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Streams.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformTime.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Math/RandomStream.h"
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
	FProxyFileContext::FProxyFileContext()
	{
		uint32 Seed = FPlatformTime::Cycles();
		FRandomStream RandomStream = FRandomStream((int32)Seed);
		CurrentFileIndex = RandomStream.GetUnsignedInt();
	}


	InputFileStream::InputFileStream( const FString& File )
	{
		m_pD = new Private();

		TSharedPtr<IFileHandle> FilePtr(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*File));
		m_pD->File = FilePtr;
		
		if (m_pD->File)
		{
			m_pD->File->SeekFromEnd();
			m_pD->FileSize = m_pD->File->Tell();
			m_pD->File->Seek(0);
		}
		
		m_pD->BytesInBuffer = 0;
		m_pD->BufferPosition = 0;
		m_pD->FilePosition = 0;

		m_pD->Buffer.SetNum(MUTABLE_STREAM_BUFFER_SIZE);
	}


	//---------------------------------------------------------------------------------------------
	InputFileStream::~InputFileStream()
	{
        delete m_pD;
		m_pD = nullptr;
	}
    
    
	//---------------------------------------------------------------------------------------------
	bool InputFileStream::IsOpen() const
    {
        return m_pD->File != nullptr;
    }
    

    //---------------------------------------------------------------------------------------------
	uint64 InputFileStream::Tell() const
    {
		check(IsOpen());

        size_t bufferOffset = m_pD->BytesInBuffer - m_pD->BufferPosition;
        int64 filePos = m_pD->File->Tell();
        check(filePos>0);
        size_t pos = size_t(filePos) - bufferOffset;
        return pos;
    }


    //---------------------------------------------------------------------------------------------
    void InputFileStream::Seek( uint64 NewPosition )
    {
		check(IsOpen());

        bool bSuccess = m_pD->File->Seek( int64(NewPosition) );
        check(bSuccess);

        m_pD->BytesInBuffer = 0;
        m_pD->BufferPosition = 0;
		m_pD->FilePosition = NewPosition;
    }


	//---------------------------------------------------------------------------------------------
    void InputFileStream::Read( void* pData, uint64 size )
	{
		check(IsOpen());

        uint8* pDest = (uint8*) pData;
        while (size && m_pD->File )
		{			
			int64 Available = FMath::Min( int64(m_pD->BytesInBuffer) - int64(m_pD->BufferPosition), int64(size) );

			// Copy from what is already loaded
			if (Available >0 )
			{
                FMemory::Memcpy( pDest, m_pD->Buffer.GetData()+m_pD->BufferPosition, Available);
				m_pD->BufferPosition += Available;
				size -= Available;
				pDest += Available;
			}

			// Load more data if necessary
			if ( m_pD->BufferPosition == m_pD->BytesInBuffer && size )
			{
				int64 BytesLeftInFile = m_pD->FileSize - m_pD->FilePosition;
				int64 BytesToRead = FMath::Min(MUTABLE_STREAM_BUFFER_SIZE, BytesLeftInFile);
                bool bSuccess = m_pD->File->Read( m_pD->Buffer.GetData(), BytesToRead );
                check( bSuccess );
				m_pD->BytesInBuffer = BytesToRead;
				m_pD->BufferPosition = 0;
                m_pD->FilePosition += BytesToRead;
            }
		}

	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	OutputFileStream::OutputFileStream( const FString& File )
	{
		m_pD = new Private();
		TSharedPtr<IFileHandle> FilePtr(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*File));
		m_pD->File = FilePtr;
		m_pD->BufferPosition = 0;
		m_pD->Buffer.SetNum(MUTABLE_STREAM_BUFFER_SIZE);

		check( m_pD->File );
	}


	//---------------------------------------------------------------------------------------------
    void OutputFileStream::Write( const void* pData, uint64 size )
	{
        const uint8_t* pSrc = (const uint8_t*) pData;

		while (size)
		{
            int64 Available = FMath::Min(int64(MUTABLE_STREAM_BUFFER_SIZE) - int64(m_pD->BufferPosition), int64(size) );

			// Copy from what fits in the current buffer
			if (Available >0 )
			{
                FMemory::Memcpy( m_pD->Buffer.GetData() + m_pD->BufferPosition, pSrc, Available);
				m_pD->BufferPosition += Available;
				size -= Available;
				pSrc += Available;
			}

			// Flush the data if necessary
            if ( m_pD->BufferPosition == MUTABLE_STREAM_BUFFER_SIZE )
			{
				Flush();
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void OutputFileStream::Flush()
	{
		if ( m_pD->BufferPosition)
		{
			bool bSuccess = m_pD->File->Write( m_pD->Buffer.GetData(), m_pD->BufferPosition);
			check(bSuccess);
        }

		m_pD->BufferPosition = 0;
	}


	//---------------------------------------------------------------------------------------------
	OutputFileStream::~OutputFileStream()
	{
		Flush();

        delete m_pD;
		m_pD = 0;
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    FileModelWriter::FileModelWriter(const FString& InModelLocation)
    {
		ModelLocation = InModelLocation;
    }


    //---------------------------------------------------------------------------------------------
    void FileModelWriter::GetFileName( uint64 key0, FString& outFileName )
    {
		outFileName = MakeDataFileName( ModelLocation, key0 );
    }


    //---------------------------------------------------------------------------------------------
    void FileModelWriter::OpenWriteFile(uint64 key0)
    {
        check( !WriteFile );

        IFileHandle* pFile = nullptr;
        TotalWritten = 0;

        if ( !key0 )
        {
            // It is the main model file
            pFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite( *ModelLocation );
			check(pFile);
			if (!pFile)
			{
				return;
			}

            // Write the file identifying tag.
            bool bSuccess = pFile->Write( (const uint8*)MUTABLE_COMPILED_MODEL_FILETAG, 4 );
            check(bSuccess);

            uint32 codeVersion = MUTABLE_COMPILED_MODEL_CODE_VERSION;
			bSuccess = pFile->Write( (const uint8*)&codeVersion, sizeof(uint32) );
            check(bSuccess);
        }
        else
        {
            // It is a data file from a split model
            FString name = MakeDataFileName(ModelLocation, key0 );
            pFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite( *name );
        }

        check( pFile );

        WriteFile = pFile;
    }


    //---------------------------------------------------------------------------------------------
    void FileModelWriter::Write( const void* pBuffer, uint64 size )
    {
        check( WriteFile );
        WriteFile->Write( ((const uint8*)pBuffer), size );
        TotalWritten += size;
    }


    //---------------------------------------------------------------------------------------------
    void FileModelWriter::CloseWriteFile()
    {
        check( WriteFile );
        delete WriteFile;
        WriteFile = nullptr;
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    class ProxyFactoryMutableSourceFile::Private
    {
    public:
        InputFileStream* Stream = nullptr;
        FString FileName;
    };


    //---------------------------------------------------------------------------------------------
    ProxyFactoryMutableSourceFile::ProxyFactoryMutableSourceFile( const FString& InFileName, 
                                                                  InputFileStream* pStream )
    {
        m_pD = new Private();

        m_pD->Stream = pStream;
        m_pD->FileName = InFileName;
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
        size_t position = m_pD->Stream->Tell();

        // we are unserialising just to skip the image. It would be much better if we had the size
        // and we could skip it.
        Ptr<Image> t = Image::StaticUnserialise( arch );

        return new ResourceProxyFile<Image>( m_pD->FileName, position );
    }


}

