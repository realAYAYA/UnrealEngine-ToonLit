// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Serialisation.h"

#include "Containers/Array.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/SerialisationPrivate.h"

#include <limits>


namespace mu
{


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

            // This could happen in 32-bit platforms
            check( size<std::numeric_limits<size_t>::max() );

            const uint8_t* pSource = ((const uint8_t*)(m_pD->m_pBuffer))+m_pD->m_pos;
            FMemory::Memcpy( pData, pSource, (size_t)size );
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
            // This could happen in 32-bit platforms
            check( reserve<std::numeric_limits<size_t>::max() );
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
            // This could happen in 32-bit platforms
            check( size<std::numeric_limits<size_t>::max() );

            uint64 pos = m_pD->m_buffer.Num();
            m_pD->m_buffer.SetNum( pos + size, false );
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
    class InputArchiveWithProxies::Private : public Base
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
                if ( (std::size_t)id < m_pD->m_proxyHistory.Num() )
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

