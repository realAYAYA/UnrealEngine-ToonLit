// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Serialisation.h"

#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Math/Transform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/MutableMemory.h"
#include "MuR/Types.h"
#include "MuR/MemoryPrivate.h"

#include <string>


namespace mu
{
	typedef std::string string;

    //!
    class MUTABLERUNTIME_API InputMemoryStream : public InputStream
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        //! Create the stream using an external buffer.
        //! The buffer will not be owned by this object, so it cannot be deallocated while this
        //! objects is in use.
        InputMemoryStream( const void* pBuffer, uint64 size );

        ~InputMemoryStream();


        //-----------------------------------------------------------------------------------------
        // InputStream interface
        //-----------------------------------------------------------------------------------------
        void Read( void* pData, uint64 size ) override;


    private:

        class Private;
        Private* m_pD;

    };


    //! This stream doesn-t store any data, it just counts de amonut of data serialised.
    class MUTABLERUNTIME_API OutputSizeStream : public OutputStream
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        //!
        OutputSizeStream();

        //-----------------------------------------------------------------------------------------
        // OutputStream interface
        //-----------------------------------------------------------------------------------------
        void Write( const void* pData, uint64 size ) override;

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------

        //! Get the amount of data serialised, in bytes.
        uint64 GetBufferSize() const;

    private:

        uint64 m_writtenBytes;

    };


    //---------------------------------------------------------------------------------------------
    class MUTABLERUNTIME_API InputMemoryStream::Private : public Base
    {
    public:

        Private()
        {
            m_pBuffer = 0;
            m_size = 0;
            m_pos = 0;
        }

        const void* m_pBuffer;

		uint64 m_size;

		uint64 m_pos;
    };


    //---------------------------------------------------------------------------------------------
    class MUTABLERUNTIME_API OutputMemoryStream::Private : public Base
    {
    public:
        TArray<uint8,TSizedHeapAllocator<64>> m_buffer;
    };


	//---------------------------------------------------------------------------------------------
	class MUTABLERUNTIME_API InputArchive::Private : public Base
	{
	public:

		//! Not owned
		InputStream* m_pStream;

		//! Already read pointers
		TArray< Ptr<RefCounted> > m_history;

	};

	//---------------------------------------------------------------------------------------------
	class MUTABLERUNTIME_API OutputArchive::Private : public Base
	{
	public:

		//! Not owned
		OutputStream* m_pStream;

		//! Already written pointers and their ids
        TMap< const void*, int32 > m_history;

	};


	//---------------------------------------------------------------------------------------------
	template< typename T >
	inline void operator<< ( OutputArchive& arch, const T& t )
	{
        t.Serialise( arch );
	}

	template< typename T >
	inline void operator>> ( InputArchive& arch, T& t )
	{
        t.Unserialise( arch );
	}


	//---------------------------------------------------------------------------------------------
#define MUTABLE_DEFINE_POD_SERIALISABLE(T)								\
		template<>														\
		inline void operator<< <T>( OutputArchive& arch, const T& t )	\
		{																\
			arch.GetPrivate()->m_pStream->Write( &t, sizeof(T) );		\
		}																\
																		\
		template<>														\
		inline void operator>> <T>( InputArchive& arch, T& t )			\
		{																\
			arch.GetPrivate()->m_pStream->Read( &t, sizeof(T) );		\
		}

	MUTABLE_DEFINE_POD_SERIALISABLE(float);
	MUTABLE_DEFINE_POD_SERIALISABLE(double);

    MUTABLE_DEFINE_POD_SERIALISABLE( int8 );
    MUTABLE_DEFINE_POD_SERIALISABLE( int16 );
    MUTABLE_DEFINE_POD_SERIALISABLE( int32 );
    MUTABLE_DEFINE_POD_SERIALISABLE( int64 );

    MUTABLE_DEFINE_POD_SERIALISABLE( uint8 );
    MUTABLE_DEFINE_POD_SERIALISABLE( uint16 );
    MUTABLE_DEFINE_POD_SERIALISABLE( uint32 );
    MUTABLE_DEFINE_POD_SERIALISABLE( uint64 );

	//---------------------------------------------------------------------------------------------
	// Bool size is not a standard
	//---------------------------------------------------------------------------------------------
	template<>														
	inline void operator<< <bool>( OutputArchive& arch, const bool& t )
	{
        uint8 s = t ? 1 : 0;
        arch.GetPrivate()->m_pStream->Write( &s, sizeof(uint8) );
	}

	template<>
	inline void operator>> <bool>( InputArchive& arch, bool& t )
	{
        uint8 s;
        arch.GetPrivate()->m_pStream->Read( &s, sizeof(uint8) );
		t = s!=0;
	}


	//---------------------------------------------------------------------------------------------
#define MUTABLE_DEFINE_ENUM_SERIALISABLE(T)								\
		template<>														\
        inline void operator<< <T>( OutputArchive& arch, const T& t )	\
		{																\
            auto v = (uint32)t;                                       \
            arch.GetPrivate()->m_pStream->Write( &v, sizeof(uint32) );\
		}																\
																		\
		template<>														\
        inline void operator>> <T>( InputArchive& arch, T& t )          \
		{																\
            uint32 v;													\
            arch.GetPrivate()->m_pStream->Read( &v, sizeof(uint32) );	\
			t = (T)v;													\
		}


    //---------------------------------------------------------------------------------------------
    template< typename T0, typename T1 >
    inline void operator<< ( OutputArchive& arch, const std::pair<T0,T1>& v )
    {
        arch << v.first;
        arch << v.second;
    }

    template< typename T0, typename T1 >
    inline void operator>> ( InputArchive& arch, std::pair<T0,T1>& v )
    {
        arch >> v.first;
        arch >> v.second;
    }


	
	//---------------------------------------------------------------------------------------------
	template<typename T> inline void operator<<(OutputArchive& arch, const TArray<T>& v)
	{
		// TODO: Optimise for vectors of PODs
		arch << (uint32)v.Num();
		for (std::size_t i = 0; i < v.Num(); ++i)
		{
			arch << v[i];
		}
	}

	template<typename T> inline void operator>>(InputArchive& arch, TArray<T>& v)
	{
		// TODO: Optimise for vectors of PODs
		uint32 size;
		arch >> size;
		v.SetNum(size);
		for (std::size_t i = 0; i < size; ++i)
		{
			arch >> v[i];
		}
	}

	//---------------------------------------------------------------------------------------------
	template< typename K, typename T >
	inline void operator<< (OutputArchive& arch, const TMap<K, T>& v)
	{
		arch << (uint32)v.Num();
		for (const auto& p : v)
		{
			arch << p.Key;
			arch << p.Value;
		}
	}

	template< typename K, typename T >
	inline void operator>> (InputArchive& arch, TMap<K, T>& v)
	{
		uint32 size;
		arch >> size;

		for (uint32 i = 0; i < size; ++i)
		{
			K k;
			T t;
			arch >> k;
			arch >> t;
			v.Add(k, t);
		}
	}


	//---------------------------------------------------------------------------------------------
#define MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(T)										\
	template<> inline void operator<<<T>(OutputArchive& arch, const TArray<T>& v)		\
	{                                                                                   \
		uint32 Num = uint32(v.Num());													\
		arch << Num;																	\
		if (Num)																		\
		{                                                                               \
			arch.GetPrivate()->m_pStream->Write(&v[0], Num * sizeof(T));				\
		}                                                                               \
	}                                                                                   \
																						\
	template<> inline void operator>><T>(InputArchive& arch, TArray<T>& v)				\
	{                                                                                   \
		uint32 Num;																		\
		arch >> Num;																	\
		v.SetNum(Num);																	\
		if (Num)																		\
		{                                                                               \
			arch.GetPrivate()->m_pStream->Read(&v[0], Num * sizeof(T));					\
		}                                                                               \
	}

	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(float)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(double)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint8)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint16)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint32)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint64)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int8)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int16)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int32)
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int64)


	//---------------------------------------------------------------------------------------------
	template<>
	inline void operator<< <std::string>( OutputArchive& arch, const std::string& v )
	{
        arch << (uint32)v.size();
		if ( v.size() )
		{
			arch.GetPrivate()->m_pStream->Write( &v[0], (unsigned)v.size()*sizeof(char) );
		}
	}

	template<>
	inline void operator>> <std::string>( InputArchive& arch, std::string& v )
	{
        uint32 size;
		arch >> size;
		v.resize( size );
		if (size)
		{
			arch.GetPrivate()->m_pStream->Read( &v[0], (unsigned)size*sizeof(char) );
		}
	}


	//---------------------------------------------------------------------------------------------
	template< typename T >
	inline void operator<< ( OutputArchive& arch, const Ptr<const T>& p )
	{
		if (!p.get())
		{
            arch << (int32)-1;
		}
		else
		{
            int32* it = arch.GetPrivate()->m_history.Find(p.get());

			if ( !it )
			{
                int32 id = arch.GetPrivate()->m_history.Num();
                arch.GetPrivate()->m_history.Add( p.get(), id );
				arch << id;
				T::Serialise( p.get(), arch );
			}
			else
			{
				arch << *it;
			}
		}
	}

	template< typename T >
	inline void operator>> ( InputArchive& arch, Ptr<const T>& p )
	{
        int32 id;
		arch >> id;

		if ( id == -1 )
		{
			p = 0;
		}
		else
		{
			if ( id < arch.GetPrivate()->m_history.Num() )
			{
				p = static_cast<T*>( arch.GetPrivate()->m_history[id].get() );

				// If the pointer was 0 it means the position in history is used, but not set yet
				// option 1: we have a smart pointer loop which is very bad.
				// option 2: the resource in this Ptr is also pointed by a Proxy that has absorbed it
				//			 and this reference should also be a proxy instead of a ptr.
                check( p );
			}
			else
			{
                // Ids come in order, but they may have been absorbed outside in some serialisations
                // like proxies.
                //check( id == (int)arch.GetPrivate()->m_history.size() );
                arch.GetPrivate()->m_history.SetNum( id+1 );

				Ptr<T> t = T::StaticUnserialise( arch );
				p = t;
				arch.GetPrivate()->m_history[id] = t;
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	template< typename T >
	inline void operator<< ( OutputArchive& arch, const Ptr<T>& p )
	{
		operator<<( arch, (const Ptr<const T>&) p );
	}

	template< typename T >
	inline void operator>> ( InputArchive& arch, Ptr<T>& p )
	{
		operator>>( arch, (Ptr<const T>&) p );
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	template<typename T0, typename T1> inline void operator<<(OutputArchive& arch, const TPair<T0,T1>& v)
	{
		arch << v.Key;
		arch << v.Value;
	}

	template<typename T0, typename T1> inline void operator>>(InputArchive& arch, TPair<T0, T1>& v)
	{
		arch >> v.Key;
		arch >> v.Value;
	}


	//---------------------------------------------------------------------------------------------
	// TODO: As POD?
	template< typename T >
	inline void operator<< (OutputArchive& arch, const UE::Math::TQuat<T>& v)
	{		
		arch << v.X;
		arch << v.Y;
		arch << v.Z;
		arch << v.W;
	}

	template< typename T >
	inline void operator>> (InputArchive& arch, UE::Math::TQuat<T>& v)
	{
		arch >> v.X;
		arch >> v.Y;
		arch >> v.Z;
		arch >> v.W;
	}


	//---------------------------------------------------------------------------------------------
	// TODO: As POD?
	template< typename T >
	inline void operator<< (OutputArchive& arch, const UE::Math::TVector<T>& v)
	{
		arch << v.X;
		arch << v.Y;
		arch << v.Z;
	}

	template< typename T >
	inline void operator>> (InputArchive& arch, UE::Math::TVector<T>& v)
	{
		arch >> v.X;
		arch >> v.Y;
		arch >> v.Z;
	}


	//---------------------------------------------------------------------------------------------
	template< typename T >
	inline void operator<< (OutputArchive& arch, const UE::Math::TTransform<T>& v)
	{
		arch << v.GetRotation();
		arch << v.GetTranslation();
		arch << v.GetScale3D();
	}

	template< typename T >
	inline void operator>> (InputArchive& arch, UE::Math::TTransform<T>& v)
	{
		UE::Math::TQuat<T> Rot;
		UE::Math::TVector<T> Trans;
		UE::Math::TVector<T> Scale;

		arch >> Rot;
		arch >> Trans;
		arch >> Scale;

		v.SetComponents(Rot, Trans, Scale);
	}

}
