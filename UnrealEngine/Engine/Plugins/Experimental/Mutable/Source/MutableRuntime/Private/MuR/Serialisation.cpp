// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Serialisation.h"

#include "Containers/Array.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/SerialisationPrivate.h"


namespace mu
{
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(float);    
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(double);   
                                                  
    MUTABLE_IMPLEMENT_POD_SERIALISABLE( int8 );   
    MUTABLE_IMPLEMENT_POD_SERIALISABLE( int16 );  
    MUTABLE_IMPLEMENT_POD_SERIALISABLE( int32 );  
    MUTABLE_IMPLEMENT_POD_SERIALISABLE( int64 );  
                                                  
    MUTABLE_IMPLEMENT_POD_SERIALISABLE( uint8 );  
    MUTABLE_IMPLEMENT_POD_SERIALISABLE( uint16 ); 
    MUTABLE_IMPLEMENT_POD_SERIALISABLE( uint32 ); 
    MUTABLE_IMPLEMENT_POD_SERIALISABLE( uint64 )
	
	// Unreal POD Serializables                                                       
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FGuid);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FUintVector2);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FIntVector2);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(UE::Math::TIntVector2<uint16>);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(UE::Math::TIntVector2<int16>);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FVector2f);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FVector4f);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FMatrix44f);

	
    void operator<<(OutputArchive& arch, const FString& t)
    {
	    const TArray<TCHAR>& Data = t.GetCharArray();
    	arch << Data;
    }


    void operator>>(InputArchive& arch, FString& t)
    {
		TArray<TCHAR> Data;
    	arch >> Data;

    	t = FString(Data.GetData()); // Construct from raw pointer to avoid double zero terminating character
    }


    void operator<<(OutputArchive& arch, const FName& v)
    {
	    arch << v.ToString();
    }


    void operator>>(InputArchive& arch, FName& v)
    {
	    FString Temp;
	    arch >> Temp;
	    v = FName(Temp);
    }
	

	void operator>> ( InputArchive& arch, std::string& v )
    {
    	uint32 size;
    	arch >> size;
    	v.resize( size );
    	if (size)
    	{
    		arch.GetPrivate()->m_pStream->Read( &v[0], (unsigned)size*sizeof(char) );
    	}
    }

	
	void operator<<(OutputArchive& Arch, const bool& T)
    {
    	uint8 S = T ? 1 : 0;
    	Arch.GetPrivate()->m_pStream->Write(&S, sizeof(uint8));
    }


	void operator>>(InputArchive& Arch, bool& T)
    {
    	uint8 S;
    	Arch.GetPrivate()->m_pStream->Read(&S, sizeof(uint8));
    	T = S != 0;
    }

	
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    InputMemoryStream::InputMemoryStream( const void* pBuffer, uint64 size )
    {
        m_pD = new Private();
        m_pD->m_pBuffer = pBuffer;
        m_pD->m_size = size;
    }


    //---------------------------------------------------------------------------------------------
    InputMemoryStream::~InputMemoryStream()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    void InputMemoryStream::Read( void* pData, uint64 size )
    {
        if (size)
        {
            check( m_pD->m_pos + size <= m_pD->m_size );

            const uint8* pSource = ((const uint8*)(m_pD->m_pBuffer))+m_pD->m_pos;
            FMemory::Memcpy( pData, pSource, (SIZE_T)size );
            m_pD->m_pos += size;
        }
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    OutputMemoryStream::OutputMemoryStream(uint64 reserve )
    {
        m_pD = new Private();
        if (reserve)
        {
             m_pD->m_buffer.Reserve( reserve );
        }
    }


    //---------------------------------------------------------------------------------------------
    OutputMemoryStream::~OutputMemoryStream()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    void OutputMemoryStream::Write( const void* pData, uint64 size )
    {
        if (size)
        {
            uint64 pos = m_pD->m_buffer.Num();
            m_pD->m_buffer.SetNum( pos + size, EAllowShrinking::No );
			FMemory::Memcpy( &m_pD->m_buffer[pos], pData, size );
        }
    }


    //---------------------------------------------------------------------------------------------
    const void* OutputMemoryStream::GetBuffer() const
    {
        const void* pResult = 0;

        if ( m_pD->m_buffer.Num() )
        {
            pResult = &m_pD->m_buffer[0];
        }

        return pResult;
    }


    //---------------------------------------------------------------------------------------------
	uint64 OutputMemoryStream::GetBufferSize() const
    {
        return m_pD->m_buffer.Num();
    }


    //-------------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------------
    //-------------------------------------------------------------------------------------------------
    OutputSizeStream::OutputSizeStream()
    {
        m_writtenBytes = 0;
    }


    //-------------------------------------------------------------------------------------------------
    void OutputSizeStream::Write( const void*, uint64 size )
    {
        m_writtenBytes += size;
    }


    //-------------------------------------------------------------------------------------------------
	uint64 OutputSizeStream::GetBufferSize() const
    {
        return m_writtenBytes;
    }


    //---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	InputArchive::InputArchive( InputStream* pStream )
	{
		m_pD = new Private();
		m_pD->m_pStream = pStream;
	}


	//---------------------------------------------------------------------------------------------
	InputArchive::~InputArchive()
	{
        check( m_pD );
		delete m_pD;
		m_pD = 0;
	}


	//---------------------------------------------------------------------------------------------
	InputArchive::Private* InputArchive::GetPrivate() const
	{
		return m_pD;
	}


    //---------------------------------------------------------------------------------------------
    Ptr<ResourceProxy<Image>> InputArchive::NewImageProxy()
    {
        return nullptr;
    }


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	OutputArchive::OutputArchive( OutputStream* pStream )
	{
		m_pD = new Private();
		m_pD->m_pStream = pStream;
	}


	//---------------------------------------------------------------------------------------------
	OutputArchive::~OutputArchive()
	{
        check( m_pD );
		delete m_pD;
		m_pD = 0;
	}


	//---------------------------------------------------------------------------------------------
	OutputArchive::Private* OutputArchive::GetPrivate() const
	{
		return m_pD;
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    class InputArchiveWithProxies::Private
    {
    public:
        TArray< Ptr<ResourceProxy<Image>> > m_proxyHistory;
        ProxyFactory* m_pFactory = nullptr;
    };


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    InputArchiveWithProxies::InputArchiveWithProxies( InputStream* s, ProxyFactory* f )
        : InputArchive( s )
    {
        m_pD = new Private();
        m_pD->m_pFactory = f;
    }


    //---------------------------------------------------------------------------------------------
    InputArchiveWithProxies::~InputArchiveWithProxies()
    {
        delete m_pD;
    }


    //---------------------------------------------------------------------------------------------
    Ptr<ResourceProxy<Image>> InputArchiveWithProxies::NewImageProxy()
    {
        // Similar to Ptr serisalisation in SerialisationPrivate
        Ptr<ResourceProxy<Image>> p;
        {
            int32 id;
            (*this) >> id;

            if ( id == -1 )
            {
                // We consumed the serialisation, so we need to return something.
                class ImageProxyNull : public ResourceProxy<Image>
                {
                public:
                    ImagePtrConst Get() override
                    {
                        return nullptr;
                    }
                };
                p = new ImageProxyNull();
            }
            else
            {
                if ( id < m_pD->m_proxyHistory.Num() )
                {
                    p = m_pD->m_proxyHistory[id];

                    // If the pointer was nullptr it means the position in history is used, but not set
                    // yet: we have a smart pointer loop which is very bad.
                    check( p );
                }
                else
                {
                    // Ids come in order.
                    m_pD->m_proxyHistory.SetNum(id+1);

                    p = m_pD->m_pFactory->NewImageProxy(*this);
                    m_pD->m_proxyHistory[id] = p;
                }
            }
        }

        return p;
    }

}

