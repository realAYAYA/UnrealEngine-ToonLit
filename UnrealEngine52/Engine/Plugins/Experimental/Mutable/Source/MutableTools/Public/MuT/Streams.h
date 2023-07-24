// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/Serialisation.h"

namespace mu { class Image; }
namespace mu { class Model; }


//! This tag is used to identify files containing serialised Model objects. The tag is not added
//! or checked by the Model serialisation methods, but the involved tools should take care of it.
#define MUTABLE_COMPILED_MODEL_FILETAG			"amc2"


namespace mu
{


	//! Read a file into a serialisation stream.
	//! \warning This classes are for reference and they are not specially good at error handling,
	//! or working efficiently.
	class MUTABLETOOLS_API InputFileStream : public InputStream
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		//! Create the stream by reading a file.
		InputFileStream( const char* strFile );

		~InputFileStream();


		//-----------------------------------------------------------------------------------------
		// InputStream interface
		//-----------------------------------------------------------------------------------------
        void Read( void* pData, uint64 size ) override;

                
		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

        //! Return true if the file was found and it could be opened in the constructor.
        bool IsOpen() const;

        //! Return the number of bytes read from the stream. This should not be used as file 
        //! position since other bytes might have been read outside this stream, or even Seek might 
        //! have been called to repeat reads.
		uint64 GetReadBytes() const;

        //! Return the current position in bytes in the underlying file.
		uint64 Tell() const;

        //! Reposition the stream index
        void Seek( uint64 position );
        
	private:

		class Private;
		Private* m_pD;
	};


	//! Write a serialisation stream to a file.
	//! \warning This classes are for reference and they are not specially good at error handling,
	//! or working efficiently.
	class MUTABLETOOLS_API OutputFileStream : public OutputStream
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		//! Create a stream that will write to a file.
		//! The file will be created overwriting any other file with the same name.
		OutputFileStream( const char* strFile );

		~OutputFileStream();


		//-----------------------------------------------------------------------------------------
		// OutputStream interface
		//-----------------------------------------------------------------------------------------
        void Write( const void* pData, uint64 size ) override;

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Write the pending data to disk. After this call, the stream becomes invalid and cannot
		//! receive any more serialisation.
		void Flush();


	private:

		class Private;
		Private* m_pD;

	};


    //! Implementation of the streaming interface that actually loads the file synchronously when
    //! needed.
    //! \ingroup tools
    class MUTABLETOOLS_API FileModelStreamer : public ModelStreamer
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------
        FileModelStreamer();
        ~FileModelStreamer() override;

        //-----------------------------------------------------------------------------------------
        // ModelStreamer interface
        //-----------------------------------------------------------------------------------------
		OPERATION_ID BeginReadBlock(const mu::Model*, uint64 key0, void* pBuffer, uint64 size ) override;
        bool IsReadCompleted( OPERATION_ID ) override;
        void EndRead( OPERATION_ID ) override;

        void OpenWriteFile( const char* strModelName, uint64 key0 ) override;
        void Write( const void* pBuffer, uint64 size ) override;
        void CloseWriteFile() override;

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------

        //! Get the file name that this streamer would use for a specific model and data ID.
        static void GetFileName(const FString& strModelName, uint64 key0, FString& outFileName );

    private:

        class Private;
        Private* m_pD;
    };


    //! ProxyFactory that provides proxies for data stored in temporary files.
    //! \ingroup tools
    class MUTABLETOOLS_API ProxyFactoryFiles : public InputArchiveWithProxies::ProxyFactory
    {
    public:
        Ptr<ResourceProxy<mu::Image>> NewImageProxy(InputArchive& arch) override;
    };


    //! ProxyFactory that provides proxies for data stored in a mutable_source file.
    //! \ingroup tools
    class MUTABLETOOLS_API ProxyFactoryMutableSourceFile : public InputArchiveWithProxies::ProxyFactory
    {
    public:
        ProxyFactoryMutableSourceFile( const char* strFileName, InputFileStream* );
        ~ProxyFactoryMutableSourceFile() override;

        Ptr<ResourceProxy<mu::Image>> NewImageProxy(InputArchive& arch) override;

    private:

        class Private;
        Private* m_pD;
    };



}
