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
		InputFileStream( const FString& File );

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
		OutputFileStream(const FString& File);

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
    class MUTABLETOOLS_API FileModelWriter : public ModelWriter
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------
		FileModelWriter( const FString& ModelLocation );

        //-----------------------------------------------------------------------------------------
        // ModelStreamer interface
        //-----------------------------------------------------------------------------------------
        void OpenWriteFile( uint64 key0 ) override;
        void Write( const void* pBuffer, uint64 size ) override;
        void CloseWriteFile() override;

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------

        //! Get the file name that this streamer would use for a specific model and data ID.
        void GetFileName(uint64 key0, FString& outFileName );

		/** */
		static inline FString MakeDataFileName(const FString& ModelName, uint64 key0)
		{
			FString Name = ModelName;
			int32 ExtensionPos = 0;
			if (Name.FindLastChar('.', ExtensionPos))
			{
				Name = Name.Left(ExtensionPos);
			}

			Name += FString::Printf(TEXT(".%08x-%08x.mutable_data"), uint32_t((key0 >> 32) & 0xffffffff), uint32(key0 & 0xffffffff));

			return Name;
		}

    private:

		IFileHandle* WriteFile = nullptr;

		FString ModelLocation;

		// For debug
		uint64 TotalWritten = 0;

	};


    //! ProxyFactory that provides proxies for data stored in a mutable_source file.
    //! \ingroup tools
    class MUTABLETOOLS_API ProxyFactoryMutableSourceFile : public InputArchiveWithProxies::ProxyFactory
    {
    public:
        ProxyFactoryMutableSourceFile( const FString& FileName, InputFileStream* );
        ~ProxyFactoryMutableSourceFile() override;

        Ptr<ResourceProxy<mu::Image>> NewImageProxy(InputArchive& arch) override;

    private:

        class Private;
        Private* m_pD;
    };



}
