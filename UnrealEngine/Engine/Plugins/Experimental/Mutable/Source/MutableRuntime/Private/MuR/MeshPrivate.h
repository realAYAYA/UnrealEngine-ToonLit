// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Mesh.h"

#include "MuR/Layout.h"
#include "MuR/Skeleton.h"
#include "MuR/SerialisationPrivate.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ConvertData.h"

#include "CoreFwd.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	inline void GetMeshBuf
		(
			Mesh* pMesh,
			MESH_BUFFER_SEMANTIC semantic,
			MESH_BUFFER_FORMAT expectedFormat,
			int expectedComponents,
            uint8_t*& pBuf,
			int& elemSize
		)
	{
		// Avoid unreferenced parameter warnings
		(void)expectedFormat;
		(void)expectedComponents;

		int buffer = -1;
		int channel = -1;
		pMesh->GetVertexBuffers().FindChannel( semantic, 0, &buffer, &channel );
		check( buffer>=0 && channel>=0 );

		MESH_BUFFER_SEMANTIC realSemantic = MBS_NONE;
		int realSemanticIndex = 0;
		MESH_BUFFER_FORMAT format = MBF_NONE;
		int components = 0;
		int offset = 0;
		pMesh->GetVertexBuffers().GetChannel( buffer, channel, &realSemantic, &realSemanticIndex, &format, &components, &offset );
		check( realSemantic == semantic );
		check( format == expectedFormat );
		check( components == expectedComponents );
		elemSize = pMesh->GetVertexBuffers().GetElementSize( buffer );
		pBuf = pMesh->GetVertexBuffers().GetBufferData( buffer );
		pBuf += offset;
	}


	//---------------------------------------------------------------------------------------------
	inline void GetMeshBuf
		(
			const Mesh* pMesh,
			MESH_BUFFER_SEMANTIC semantic,
			MESH_BUFFER_FORMAT expectedFormat,
			int expectedComponents,
            const uint8_t*& pBuf,
			int& elemSize
		)
	{
		// Avoid unreferenced parameter warnings
		(void)expectedFormat;
		(void)expectedComponents;

		int buffer = -1;
		int channel = -1;
		pMesh->GetVertexBuffers().FindChannel( semantic, 0, &buffer, &channel );
		check( buffer>=0 && channel>=0 );

		MESH_BUFFER_SEMANTIC realSemantic = MBS_NONE;
		int realSemanticIndex = 0;
		MESH_BUFFER_FORMAT format = MBF_NONE;
		int components = 0;
		int offset = 0;
		pMesh->GetVertexBuffers().GetChannel
				( buffer, channel, &realSemantic, &realSemanticIndex, &format, &components, &offset );
		check( realSemantic == semantic );
		check( format == expectedFormat );
		check( components == expectedComponents );
		elemSize = pMesh->GetVertexBuffers().GetElementSize( buffer );
		pBuf = pMesh->GetVertexBuffers().GetBufferData( buffer );
		pBuf += offset;
	}


	//---------------------------------------------------------------------------------------------
	//! Class to iterate a specific buffer channel of unknown type
	//---------------------------------------------------------------------------------------------
	class UntypedMeshBufferIteratorConst;

	class MUTABLERUNTIME_API UntypedMeshBufferIterator 
	{
	public:
		inline UntypedMeshBufferIterator()
		{
			m_format = MBF_NONE;
			m_components = 0;
			m_elementSize = 0;
			m_pBuf = 0;
		}


		inline UntypedMeshBufferIterator
			(
				FMeshBufferSet& bufferSet,
				MESH_BUFFER_SEMANTIC semantic,
				int semanticIndex = 0
			)
		{
			int buffer = -1;
			int channel = -1;
			bufferSet.FindChannel( semantic, semanticIndex, &buffer, &channel );

			if ( buffer>=0 && channel>=0 )
			{
				MESH_BUFFER_SEMANTIC realSemantic = MBS_NONE;
				int realSemanticIndex = 0;
				m_format = MBF_NONE;
				m_components = 0;
				int offset = 0;
				bufferSet.GetChannel
						(
							buffer, channel,
							&realSemantic, &realSemanticIndex,
							&m_format, &m_components,
							&offset
						);
				check( realSemantic == semantic );
				check( realSemanticIndex == semanticIndex );

				m_elementSize = bufferSet.GetElementSize( buffer );

				m_pBuf = bufferSet.GetBufferData( buffer );
				m_pBuf += offset;
			}
			else
			{
				m_format = MBF_NONE;
				m_components = 0;
				m_elementSize = 0;
				m_pBuf = 0;
			}
		}

        inline uint8_t* ptr() const
		{
			return m_pBuf;
		}

		inline void operator++()
		{
			m_pBuf += m_elementSize;
		}

		inline void operator++(int)
		{
			m_pBuf += m_elementSize;
		}

		inline void operator+=( int c )
		{
			m_pBuf += c*m_elementSize;
		}

		inline UntypedMeshBufferIterator operator+(int c) const
		{
			UntypedMeshBufferIterator res = *this;
			res += c;

			return res;
		}

		inline std::size_t operator-( const UntypedMeshBufferIterator& other ) const
		{
			// Special degenerate case.
			if (m_elementSize == 0)
			{
				return 0;
			}
			check( other.m_elementSize == m_elementSize );
			check( (ptr()-other.ptr()) % m_elementSize == 0 );
			return (ptr()-other.ptr()) / m_elementSize;
		}

		inline int GetElementSize() const
		{
			return m_elementSize;
		}

		inline MESH_BUFFER_FORMAT GetFormat() const
		{
			return m_format;
		}

		inline int GetComponents() const
		{
			return m_components;
		}

        vec4<float> GetAsVec4f() const
        {
            vec4<float> res;

            for (int c=0; c< FMath::Min(m_components,4); ++c)
            {
                ConvertData( c, &res[0], MBF_FLOAT32, ptr(), m_format );
            }

            return res;
        }

		FVector3f GetAsVec3f() const
		{
			FVector3f res = { 0,0,0 };
			for (int c = 0; c < FMath::Min(m_components, 3); ++c)
			{
				ConvertData(c, &res[0], MBF_FLOAT32, ptr(), m_format);
			}
			return res;
		}

		FVector3d GetAsVec3d() const
		{
			FVector3d res = { 0,0,0 };
			for (int c = 0; c < FMath::Min(m_components, 3); ++c)
			{
				ConvertData(c, &res[0], MBF_FLOAT64, ptr(), m_format);
			}
			return res;
		}
		
		FVector2f GetAsVec2f() const
		{
			FVector2f res = {0.0f, 0.0f};
			for (int32 c = 0; c < FMath::Min(m_components, 2); ++c)
			{
				ConvertData(c, &res[0], MBF_FLOAT32, ptr(), m_format);
			}

			return res;
		}
		
        uint32_t GetAsUINT32() const
        {
            uint32_t res;
            ConvertData( 0, &res, MBF_UINT32, ptr(), m_format );
            return res;
        }

		void SetFromUINT32(uint32_t v)
		{
			ConvertData(0, ptr(), m_format, &v, MBF_UINT32);
		}

		void SetFromVec3f(const FVector3f& v)
		{
			for (int c = 0; c < FMath::Min(m_components, 3); ++c)
			{
				ConvertData(c, ptr(), m_format, &v, MBF_FLOAT32);
			}
		}

		void SetFromVec3d(const FVector3d& v)
		{
			for (int c = 0; c < FMath::Min(m_components, 3); ++c)
			{
				ConvertData(c, ptr(), m_format, &v, MBF_FLOAT64);
			}
		}

	protected:

		int m_elementSize;
        uint8_t* m_pBuf;
		MESH_BUFFER_FORMAT m_format;
		int m_components;
	};


	//---------------------------------------------------------------------------------------------
	//! Class to iterate a specific buffer channel with known type
	//---------------------------------------------------------------------------------------------
	template<MESH_BUFFER_FORMAT FORMAT, class CTYPE, int COMPONENTS>
	class MeshBufferIteratorConst;

	template<MESH_BUFFER_FORMAT FORMAT, class CTYPE, int COMPONENTS>
	class MUTABLERUNTIME_API MeshBufferIterator : public UntypedMeshBufferIterator
	{
	public:
		inline MeshBufferIterator
			(
				FMeshBufferSet& bufferSet,
				MESH_BUFFER_SEMANTIC semantic,
				int semanticIndex = 0
			)
			: UntypedMeshBufferIterator( bufferSet, semantic, semanticIndex )
		{
			if (m_pBuf)
			{
				// Extra checks
				int buffer = -1;
				int channel = -1;
				bufferSet.FindChannel(semantic, semanticIndex, &buffer, &channel);
				
				if (buffer >= 0 && channel >= 0)
				{
					MESH_BUFFER_SEMANTIC realSemantic = MBS_NONE;
					int realSemanticIndex = 0;
					MESH_BUFFER_FORMAT format = MBF_NONE;
					int components = 0;
					int offset = 0;
					bufferSet.GetChannel
					(
						buffer, channel,
						&realSemantic, &realSemanticIndex,
						&format, &components,
						&offset
					);

					if (format!=FORMAT || components!=COMPONENTS)
					{
						// Invalidate
						m_format = MBF_NONE;
						m_components = 0;
						m_elementSize = 0;
						m_pBuf = nullptr;
					}
				}
				else
				{
					// Invalidate
					m_format = MBF_NONE;
					m_components = 0;
					m_elementSize = 0;
					m_pBuf = nullptr;
				}
			}
		}

		inline vec<CTYPE,COMPONENTS>& operator*()
		{
			return *reinterpret_cast< vec<CTYPE,COMPONENTS>* >( m_pBuf );
		}

		inline vec<CTYPE,COMPONENTS>& operator[]( size_t i )
		{
			return *((*this)+int(i));
		}

		inline MeshBufferIterator<FORMAT,CTYPE,COMPONENTS> operator+(int c)
		{
			MeshBufferIterator<FORMAT,CTYPE,COMPONENTS> res = *this;
			res += c;
			return res;
		}

	};


	//---------------------------------------------------------------------------------------------
	//! Class to iterate a specific buffer channel of unknown type
	//---------------------------------------------------------------------------------------------
	class MUTABLERUNTIME_API UntypedMeshBufferIteratorConst
	{
	public:

		inline UntypedMeshBufferIteratorConst()
		{
			m_format = MBF_NONE;
			m_components = 0;
			m_elementSize = 0;
			m_pBuf = nullptr;
		}

		inline UntypedMeshBufferIteratorConst
			(
				const FMeshBufferSet& bufferSet,
				MESH_BUFFER_SEMANTIC semantic,
				int semanticIndex = 0
			)
		{
			int buffer = -1;
			int channel = -1;
			bufferSet.FindChannel( semantic, semanticIndex, &buffer, &channel );

			if ( buffer>=0 && channel>=0 )
			{
				MESH_BUFFER_SEMANTIC realSemantic = MBS_NONE;
				int realSemanticIndex = 0;
				m_format = MBF_NONE;
				m_components = 0;
				int offset = 0;
				bufferSet.GetChannel
						(
							buffer, channel,
							&realSemantic, &realSemanticIndex,
							&m_format, &m_components,
							&offset
						);
				check( realSemantic == semantic );
				check( realSemanticIndex == semanticIndex );

				m_elementSize = bufferSet.GetElementSize( buffer );

				m_pBuf = bufferSet.GetBufferData( buffer );
				m_pBuf += offset;
			}
			else
			{
				m_format = MBF_NONE;
				m_components = 0;
				m_elementSize = 0;
				m_pBuf = nullptr;
			}
		}

		inline UntypedMeshBufferIteratorConst( const UntypedMeshBufferIteratorConst& other )
		{
			m_elementSize = other.m_elementSize;
			m_pBuf = other.m_pBuf;
			m_format = other.m_format;
			m_components = other.m_components;
		}

        inline const uint8_t* ptr() const
		{
			return m_pBuf;
		}

		inline void operator++()
		{
			m_pBuf += m_elementSize;
		}

		inline void operator++(int)
		{
			m_pBuf += m_elementSize;
		}

		inline void operator+=( int c )
		{
			m_pBuf += c*m_elementSize;
		}

		inline std::size_t operator-( const UntypedMeshBufferIterator& other ) const
		{
			// Special degenerate case.
			if (m_elementSize == 0)
			{
				return 0;
			}
			check( other.GetElementSize() == m_elementSize );
			check( (ptr()-other.ptr()) % m_elementSize == 0 );
			return (ptr()-other.ptr()) / m_elementSize;
		}

		inline std::size_t operator-( const UntypedMeshBufferIteratorConst& other ) const
		{
			// Special degenerate case.
			if (m_elementSize == 0)
			{
				return 0;
			}
			check( other.GetElementSize() == m_elementSize );
			check( (ptr()-other.ptr()) % m_elementSize == 0 );
			return (ptr()-other.ptr()) / m_elementSize;
		}

		inline int GetElementSize() const
		{
			return m_elementSize;
		}

        inline MESH_BUFFER_FORMAT GetFormat() const
        {
            return m_format;
        }

        inline int GetComponents() const
        {
            return m_components;
        }

		vec4<float> GetAsVec4f() const
		{
			vec4<float> res = { 0.0f,0.0f,0.0f,0.0f };
			for (int c = 0; c < FMath::Min(m_components, 4); ++c)
			{
				ConvertData(c, &res[0], MBF_FLOAT32, ptr(), m_format);
			}
			return res;
		}

		FVector3f GetAsVec3f() const
		{
			FVector3f res = { 0,0,0 };
			for (int c = 0; c < FMath::Min(m_components, 3); ++c)
			{
				ConvertData(c, &res[0], MBF_FLOAT32, ptr(), m_format);
			}
			return res;
		}

		FVector3d GetAsVec3d() const
		{
			FVector3d res = { 0,0,0 };
			for (int c = 0; c < FMath::Min(m_components, 3); ++c)
			{
				ConvertData(c, &res[0], MBF_FLOAT64, ptr(), m_format);
			}
			return res;
		}

		FVector2f GetAsVec2f() const
		{
			FVector2f res = {0.0f, 0.0f};
			for (int32 c = 0; c < FMath::Min(m_components, 2); ++c)
			{
				ConvertData(c, &res[0], MBF_FLOAT32, ptr(), m_format);
			}

			return res;
		}
		
        uint32_t GetAsUINT32() const
        {
            uint32_t res = 0;
            ConvertData( 0, &res, MBF_UINT32, ptr(), m_format );
            return res;
        }
        
        vec<int32_t,8> GetAsVec8i() const
        {
            vec<int32_t, 8> res;
            for ( int c = 0; c < FMath::Min( m_components, 8 ); ++c )
            {
                ConvertData( c, &res[0], MBF_INT32, ptr(), m_format );
            }
            return res;
        }

        inline UntypedMeshBufferIteratorConst operator+(int c) const
        {
            UntypedMeshBufferIteratorConst res = *this;
            res += c;
            return res;
        }

	protected:

		int m_elementSize;
        const uint8_t* m_pBuf;
		MESH_BUFFER_FORMAT m_format;
		int m_components;
	};




	//---------------------------------------------------------------------------------------------
	//! Class to iterate a specific buffer channel of a constant buffer set
	//---------------------------------------------------------------------------------------------
	template<MESH_BUFFER_FORMAT FORMAT, class CTYPE, int COMPONENTS>
	class MUTABLERUNTIME_API MeshBufferIteratorConst : public UntypedMeshBufferIteratorConst
	{
	public:

		inline MeshBufferIteratorConst()
		{
		}


		inline MeshBufferIteratorConst
			(
				const FMeshBufferSet& bufferSet,
				MESH_BUFFER_SEMANTIC semantic,
				int semanticIndex = 0
			)
			: UntypedMeshBufferIteratorConst( bufferSet, semantic, semanticIndex )
		{
			if (m_pBuf)
			{
				// Extra checks
				int buffer = -1;
				int channel = -1;
				bufferSet.FindChannel(semantic, semanticIndex, &buffer, &channel);

				if (buffer >= 0 && channel >= 0)
				{
					MESH_BUFFER_SEMANTIC realSemantic = MBS_NONE;
					int realSemanticIndex = 0;
					MESH_BUFFER_FORMAT format = MBF_NONE;
					int components = 0;
					int offset = 0;
					bufferSet.GetChannel
					(
						buffer, channel,
						&realSemantic, &realSemanticIndex,
						&format, &components,
						&offset
					);
					
					if (format!=FORMAT || components!=COMPONENTS)
					{
						// Invalidate
						m_format = MBF_NONE;
						m_components = 0;
						m_elementSize = 0;
						m_pBuf = nullptr;
					}
				}
				else
				{
					// Invalidate
					m_format = MBF_NONE;
					m_components = 0;
					m_elementSize = 0;
					m_pBuf = nullptr;
				}
			}
		}

		inline MeshBufferIteratorConst( const MeshBufferIteratorConst& other )
			: UntypedMeshBufferIteratorConst( other )
		{
		}

		inline const vec<CTYPE,COMPONENTS>& operator*() const
		{
			return *reinterpret_cast< const vec<CTYPE,COMPONENTS>* >( m_pBuf );
		}

		inline const vec<CTYPE,COMPONENTS>& operator[]( size_t i ) const
		{
			return *((*this)+int(i));
		}

		inline MeshBufferIteratorConst<FORMAT,CTYPE,COMPONENTS> operator+(int c) const
		{
			MeshBufferIteratorConst<FORMAT,CTYPE,COMPONENTS> res = *this;
			res += c;
			return res;
		}

	};


	//---------------------------------------------------------------------------------------------
	// Optimised mesh formats that are identified in some operations to chose a faster version
	//---------------------------------------------------------------------------------------------
	typedef enum
	{
		SMF_NONE,

        SMF_PROJECT,

        SMF_PROJECTWRAPPING,

		SMF_COUNT
	} STATIC_MESH_FORMATS;

	//! Array of functions that can identify each format
	typedef bool (*STATIC_MESH_FORMAT_ID_FUNC)( const Mesh* );
	extern STATIC_MESH_FORMAT_ID_FUNC s_staticMeshFormatIdentify[SMF_COUNT];


}
