// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StaticArray.h"
#include "MuR/Serialisation.h"

#include "Misc/TVariant.h"
#include "Math/Color.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Math/IntVector.h"
#include "Math/Vector4.h"
#include "Math/Transform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/MutableMemory.h"
#include "MuR/MutableMath.h"
#include "MuR/Types.h"

#include <string>


namespace mu
{	

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
    class MUTABLERUNTIME_API InputMemoryStream::Private
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
    class MUTABLERUNTIME_API OutputMemoryStream::Private
    {
    public:
        TArray<uint8,TSizedHeapAllocator<64>> m_buffer;
    };


	//---------------------------------------------------------------------------------------------
	class MUTABLERUNTIME_API InputArchive::Private
	{
	public:

		//! Not owned
		InputStream* m_pStream;

		//! Already read pointers
		TArray< Ptr<RefCounted> > m_history;

	};

	//---------------------------------------------------------------------------------------------
	class MUTABLERUNTIME_API OutputArchive::Private
	{
	public:

		//! Not owned
		OutputStream* m_pStream;

		//! Already written pointers and their ids
        TMap< const void*, int32 > m_history;

	};


#define MUTABLE_IMPLEMENT_POD_SERIALISABLE(Type)				     \
    void DLLEXPORT operator<<(OutputArchive& Arch, const Type& T)    \
    {																 \
        Arch.GetPrivate()->m_pStream->Write(&T, sizeof(Type));		 \
    }																 \
                                                                     \
    void DLLEXPORT operator>>(InputArchive& Arch, Type& T)		     \
    {																 \
        Arch.GetPrivate()->m_pStream->Read(&T, sizeof(Type));		 \
    }																 \
		

#define MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(Type)                        \
    template<class Alloc>                                                      \
	void operator<<(OutputArchive& Arch, const TArray<Type, Alloc>& V)         \
	{                                                                          \
		uint32 Num = uint32(V.Num());                                          \
		Arch << Num;                                                           \
		if (Num)                                                               \
		{                                                                      \
			Arch.GetPrivate()->m_pStream->Write(&V[0], Num * sizeof(Type));    \
		}                                                                      \
	}                                                                          \
                                                                               \
    template<class Alloc>                                                      \
	void operator>>(InputArchive& Arch, TArray<Type, Alloc>& V)                \
	{                                                                          \
		uint32 Num;                                                            \
		Arch >> Num;                                                           \
		V.SetNum(Num);                                                         \
		if (Num)                                                               \
		{                                                                      \
			Arch.GetPrivate()->m_pStream->Read(&V[0], Num * sizeof(Type));     \
		}                                                                      \
	}                                                                          \

	/** TVariant custom serialize. Based on the default serialization. */
	template <typename... Ts>
	void operator<<(OutputArchive& Ar, const TVariant<Ts...>& Variant)
	{
		const uint8 Index = static_cast<uint8>(Variant.GetIndex());
		Ar << Index;
		
		Visit([&Ar](auto& StoredValue)
		{
			Ar << StoredValue;
		}, Variant);
	}


	template <typename T, typename VariantType>
	struct TVariantLoadFromInputArchiveCaller
	{
		/** Default construct the type and load it from the FArchive */
		static void Load(InputArchive& Ar, VariantType& OutVariant)
		{
			OutVariant.template Emplace<T>();
			Ar >> OutVariant.template Get<T>();
		}
	};

	
	template <typename... Ts>
	struct TVariantLoadFromInputArchiveLookup
	{
		using VariantType = TVariant<Ts...>;
		static_assert((std::is_default_constructible<Ts>::value && ...), "Each type in TVariant template parameter pack must be default constructible in order to use FArchive serialization");

		/** Load the type at the specified index from the FArchive and emplace it into the TVariant */
		static void Load(SIZE_T TypeIndex, InputArchive& Ar, VariantType& OutVariant)
		{
			static constexpr void(*Loaders[])(InputArchive&, VariantType&) = { &TVariantLoadFromInputArchiveCaller<Ts, VariantType>::Load... };
			check(TypeIndex < UE_ARRAY_COUNT(Loaders));
			Loaders[TypeIndex](Ar, OutVariant);
		}
	};

	
	template <typename... Ts>
	void operator>>(InputArchive& Ar, TVariant<Ts...>& Variant)
	{
		uint8 Index;
		Ar >> Index;
		check(Index < sizeof...(Ts));

		TVariantLoadFromInputArchiveLookup<Ts...>::Load(static_cast<SIZE_T>(Index), Ar, Variant);
	}

#define MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(Type)						\
        void DLLEXPORT operator<<(OutputArchive& Arch, const Type& T)   \
		{																\
            uint32 V = (uint32)T;                                       \
            Arch.GetPrivate()->m_pStream->Write(&V, sizeof(uint32));    \
		}																\
																		\
        void DLLEXPORT operator>>(InputArchive& Arch, Type& T)   		\
		{																\
            uint32 V;													\
            Arch.GetPrivate()->m_pStream->Read(&V, sizeof(uint32));     \
			T = (Type)V;												\
		}																\

    template<typename T0, typename T1>
    inline void operator<<(OutputArchive& Arch, const std::pair<T0, T1>& V)
    {
        Arch << V.first;
        Arch << V.second;
    }

    template<typename T0, typename T1>
    inline void operator>>(InputArchive& Arch, std::pair<T0, T1>& V)
    {
        Arch >> V.first;
        Arch >> V.second;
    }
	
	template<typename T, uint32 Size, uint32 Align> 
    void operator<<(OutputArchive& Arch, const TStaticArray<T, Size, Align>& V)
	{
		for (int32 I = 0; I < Size; ++I)
		{
			Arch << V[I];
		}
	}

	template<typename T, uint32 Size, uint32 Align> 
    void operator>>(InputArchive& Arch, TStaticArray<T, Size, Align>& V)
	{
		for (uint32 I = 0; I < Size; ++I)
		{
			Arch >> V[I];
		}
	}
	

	//---------------------------------------------------------------------------------------------
	template< typename K, typename T >
	inline void operator<< (OutputArchive& Arch, const TMap<K, T>& V)
	{
		Arch << (uint32)V.Num();
		for (const TPair<K, T>& Element : V)
		{
			Arch << Element.Key;
			Arch << Element.Value;
		}
	}

	template< typename K, typename T >
	inline void operator>> (InputArchive& Arch, TMap<K, T>& V)
	{
		uint32 Num;
		Arch >> Num;

		for (uint32 Index = 0; Index < Num; ++Index)
		{
			K Key;
			T Element;
			Arch >> Key;
			Arch >> Element;

			V.Emplace(MoveTemp(Key), MoveTemp(Element));
		}
	}

	// Unreal POD Serializables
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(float);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(double);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(uint8);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(uint16);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(uint32);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(uint64);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(int8);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(int16);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(int32);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(int64);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(TCHAR);

	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(FIntVector2);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(FUintVector2);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(UE::Math::TIntVector2<uint16>);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(UE::Math::TIntVector2<int16>);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(FVector2f);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(FVector4f);
	MUTABLE_IMPLEMENT_POD_VECTOR_SERIALISABLE(FMatrix44f);

	
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
