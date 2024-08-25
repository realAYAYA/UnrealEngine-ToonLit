// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <string>

#include "HAL/Platform.h"
#include "MuR/MutableMemory.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"

#include "Math/Vector.h"
#include "Math/IntVector.h"
#include "Math/Vector4.h"
#include "MuR/MutableMath.h"


namespace mu
{    
    class Image;
	class Model;

#define MUTABLE_DEFINE_POD_SERIALISABLE(Type)					   \
	void DLLEXPORT operator<<(OutputArchive& Arch, const Type& T); \
	void DLLEXPORT operator>>(InputArchive& Arch, Type& T);		   \

#define MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(Type)					\
	template<typename Alloc>                                            \
	void operator<<(OutputArchive& Arch, const TArray<Type, Alloc>& V);	\
	                                                                    \
	template<typename Alloc>                                            \
	void operator>>(InputArchive& Arch, TArray<Type, Alloc>& V);        \

#define MUTABLE_DEFINE_ENUM_SERIALISABLE(Type)					   \
    void DLLEXPORT operator<<(OutputArchive& Arch, const Type& T); \
    void DLLEXPORT operator>>(InputArchive& Arch, Type& T);		   \
	
	
    /** This repesents a handle to Mutable resource. */
    template<class R>
    class ResourceProxy : public RefCounted
    {
    public:

        virtual Ptr<const R> Get() = 0;

    };


	/** Proxy implementation that always has the resource loaded in memory. */
	template<class R>
	class ResourceProxyMemory : public ResourceProxy<R>
	{
	private:

		Ptr<const R> m_resource;

	public:

		ResourceProxyMemory(const Ptr<const R>& i)
		{
			m_resource = i;
		}

		// ResourceProxy interface
		Ptr<const R> Get() override
		{
			return m_resource;
		}
	};


	/** */
	class MUTABLERUNTIME_API ModelReader
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        //! Ensure virtual destruction.
        virtual ~ModelReader() {}

        //-----------------------------------------------------------------------------------------
        // Reading interface
        //-----------------------------------------------------------------------------------------

        //! Identifier of reading data operations sent to this interface.
		//! Negative values indicate an error.
        typedef int32 OPERATION_ID;

		//! \brief Start a data request operation.
		//! \param Model.
        //! \param key0 key identifying the model data fragment that is requested.
        //!         This key interpretation depends on the implementation of the ModelStreamer,
		//! \param pBuffer is an already-allocated buffer big enough to receive the expected data.
		//! \param size is the size of the pBuffer buffer, which must match the size of the data
		//! requested with the key identifiers.
		//! \param CompletionCallback Optional callback. Copied inside the called function. Will always be called.
		//! \return a previously unused identifier, now used for this operation, that can be used in
		//! calls to the other methods of this interface. If the return value is negative it indicates
		//! an unrecoverable error.
		virtual OPERATION_ID BeginReadBlock(const mu::Model*, uint64 key0, void* pBuffer, uint64 size, TFunction<void(bool bSuccess)>* CompletionCallback = nullptr) = 0;

        //! Check if a data request operation has been completed.
        //! This is a weak check than *may* return true if the given operation has completed, but
        //! it is not mandatory. It is used as a hint by the System to optimise its opertaions.
        //! There is no guarantee that this method will ever be called, and it is safe to always
        //! return false.
        virtual bool IsReadCompleted( OPERATION_ID ) = 0;

        //! Complete a data request operation. This method has to block until a data request issued
        //! with OpenFile has been completed. After returning from this call, the ID cannot be used
        //! any more to identify the same operation and becomes free.
        virtual void EndRead( OPERATION_ID ) = 0;

    };


	/** */
	class MUTABLERUNTIME_API ModelWriter
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		//! Ensure virtual destruction.
		virtual ~ModelWriter() {}

		//-----------------------------------------------------------------------------------------
		// Writing interface
		//-----------------------------------------------------------------------------------------

		//! \brief Open a file for writing data.
		//!
		//! Only one file can be open a time for writing. This method is only required when
		//! writing model data from tools.
		//! \param strModelName Name of the model where the file to open belongs to. This string's
		//!         meaning depends on the implementation of the ModelStreamer subclasses. It could
		//!         be a file path, or a resource identifier, etc. It will be the same for all
		//!         files in a model.
		//! \param key key identifying the model data fragment that is requested.
		//!         This key interpretation depends on the implementation of the ModelStreamer,
		virtual void OpenWriteFile(uint64 key0) = 0;

		//! \brief Write a piece of data to the currently open file.
		//!
		//! There must be a file open with OpenWriteFile before calling this.
		//! \param pBuffer pointer to the data that will be written to the file.
		//! \param size size of the data to write on the file, in bytes.
		virtual void Write(const void* pBuffer, uint64 size) = 0;

		//! \brief Close the file open for writing in a previous call to OpenWriteFile in this
		//! object.
		virtual void CloseWriteFile() = 0;


	};



    //! Interface for any input stream to be use with InputArchives.
    //! \ingroup tools
    class MUTABLERUNTIME_API InputStream
    {
    public:

        //! Ensure virtual destruction
        virtual ~InputStream() {}

        //! Read a byte buffer
        //! \param pData destination buffer, must have at least size bytes allocated.
        //! \param size amount of bytes to read from the stream.
        virtual void Read( void* pData, uint64 size ) = 0;
    };


    //! Interface for any output stream to be used with OutputArchives
    //! \ingroup tools
    class MUTABLERUNTIME_API OutputStream
    {
    public:

        //! Ensure virtual destruction
        virtual ~OutputStream() {}

        //! Write a byte buffer
        //! \param pData source buffer where data will be read from.
        //! \param size amount of data to write to the stream.
        virtual void Write( const void* pData, uint64 size ) = 0;

    };


    //! Archive containing data to be deserialised.
    //! \ingroup tools
    class MUTABLERUNTIME_API InputArchive
    {
    public:

        //! Construct form an input stream. The stream will not be owned by the archive and the
        //! caller must make sure it is not modified or destroyed while serialisation is happening.
        InputArchive( InputStream* );

        //!
        virtual ~InputArchive();

        //
        virtual Ptr<ResourceProxy<Image>> NewImageProxy();

        // Interface pattern
        class Private;

        Private* GetPrivate() const;

    private:

        Private* m_pD;

    };


    //! Archive where data can be serialised to.
    //! \ingroup tools
    class MUTABLERUNTIME_API OutputArchive
    {
    public:

        //! Construct form an output stream. The stream will not be owned by the archive and the
        //! caller must make sure it is not modified or destroyed while serialisation is happening.
        OutputArchive( OutputStream* );

        //!
        ~OutputArchive();

        // Interface pattern
        class Private;

        Private* GetPrivate() const;

    private:

        Private* m_pD;

    };

    //!
    //! \ingroup runtime
    class MUTABLERUNTIME_API InputArchiveWithProxies : public InputArchive
    {
    public:

        class ProxyFactory
        {
        public:
          virtual ~ProxyFactory() {}
          virtual Ptr<ResourceProxy<Image>> NewImageProxy(InputArchive& arch) = 0;
        };

    public:

        //! Construct form an input stream. The stream will not be owned by the archive and the
        //! caller must make sure it is not modified or destroyed while serialisation is happening.
        InputArchiveWithProxies( InputStream*, ProxyFactory*  );

        ~InputArchiveWithProxies() override;

        //
        Ptr<ResourceProxy<Image>> NewImageProxy() override;

    private:

        class Private;

        Private* m_pD;
    };



    //!
    class MUTABLERUNTIME_API OutputMemoryStream : public OutputStream
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        //! Create the stream with an optional buffer size in bytes.
        //! The internal buffer will be enlarged as much as necessary.
        OutputMemoryStream( uint64 reserve = 0 );

        ~OutputMemoryStream();


        //-----------------------------------------------------------------------------------------
        // OutputStream interface
        //-----------------------------------------------------------------------------------------
        void Write( const void* pData, uint64 size ) override;

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------

        //! Get the serialised data buffer pointer. This pointer invalidates after a Write
        //! operation has been done, and you need to get it again.
        const void* GetBuffer() const;

        //! Get the amount of data in the stream, in bytes.
        uint64 GetBufferSize() const;

    private:

        class Private;

        Private* m_pD;

    };


	template<typename Type>
	void operator<<(OutputArchive& Arch, const Type& Value)
	{
        Value.Serialise(Arch);
	}

	template<typename Type>
	void operator>>(InputArchive& Arch, Type& Value)
	{
        Value.Unserialise(Arch);
	}

	MUTABLE_DEFINE_POD_SERIALISABLE(float);
	MUTABLE_DEFINE_POD_SERIALISABLE(double);

    MUTABLE_DEFINE_POD_SERIALISABLE(int8);
    MUTABLE_DEFINE_POD_SERIALISABLE(int16);
    MUTABLE_DEFINE_POD_SERIALISABLE(int32);
    MUTABLE_DEFINE_POD_SERIALISABLE(int64);

    MUTABLE_DEFINE_POD_SERIALISABLE(uint8);
    MUTABLE_DEFINE_POD_SERIALISABLE(uint16);
    MUTABLE_DEFINE_POD_SERIALISABLE(uint32);
	MUTABLE_DEFINE_POD_SERIALISABLE(uint64);

	MUTABLE_DEFINE_POD_SERIALISABLE(FUintVector2);
	MUTABLE_DEFINE_POD_SERIALISABLE(FIntVector2);
	MUTABLE_DEFINE_POD_SERIALISABLE(UE::Math::TIntVector2<uint16>);
	MUTABLE_DEFINE_POD_SERIALISABLE(UE::Math::TIntVector2<int16>);
	MUTABLE_DEFINE_POD_SERIALISABLE(FVector2f);
	MUTABLE_DEFINE_POD_SERIALISABLE(FVector4f);
	MUTABLE_DEFINE_POD_SERIALISABLE(FMatrix44f);

	MUTABLE_DEFINE_POD_SERIALISABLE(FGuid);

	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(float);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(double);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint8);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint16);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint32);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint64);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int8);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int16);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int32);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int64);

	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FVector2f);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FMatrix44f);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FIntVector2);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(TCHAR);

	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FUintVector2);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(UE::Math::TIntVector2<uint16>);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(UE::Math::TIntVector2<int16>);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FVector4f);
	
	//---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API void operator<<(OutputArchive& arch, const FString& t);
	
	MUTABLERUNTIME_API void operator>>(InputArchive& arch, FString& t);


	//---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API void operator<<(OutputArchive& arch, const FName& v);

	MUTABLERUNTIME_API void operator>>(InputArchive& arch, FName& v);


	//---------------------------------------------------------------------------------------------
	template<typename T, typename Alloc> 
	void operator<<(OutputArchive& Arch, const TArray<T, Alloc>& V)
	{
		const uint32 Num = (uint32)V.Num();
		Arch << Num;
		
		for (SIZE_T Index = 0; Index < Num; ++Index)
		{
			Arch << V[Index];
		}
	}

	template<typename T, typename Alloc> 
	void operator>>(InputArchive& Arch, TArray<T, Alloc>& V)
	{
		uint32 Num;
		Arch >> Num;
		V.SetNum(Num);

		for (SIZE_T Index = 0; Index < Num; ++Index)
		{
			Arch >> V[Index];
		}
	}

	
	MUTABLERUNTIME_API void operator>> ( InputArchive& arch, std::string& v );

	// Bool size is not a standard
	MUTABLERUNTIME_API void operator<<(OutputArchive& Arch, const bool& T);

	MUTABLERUNTIME_API void operator>>(InputArchive& Arch, bool& T);
}

