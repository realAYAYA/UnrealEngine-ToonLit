// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/SerialisationPrivate.h"
#include "Containers/Array.h"
#include "HAL/PlatformMath.h"
#include "MuR/MutableMemory.h"
#include "MuR/Serialisation.h"


namespace mu
{

	//! Supported formats for the elements in mesh buffers.
	//! \ingroup runtime
	typedef enum
	{

		MBF_NONE,
		MBF_FLOAT16,
		MBF_FLOAT32,

		MBF_UINT8,
		MBF_UINT16,
		MBF_UINT32,
		MBF_INT8,
		MBF_INT16,
		MBF_INT32,

		//! Integers interpreted as being in the range 0.0f to 1.0f
		MBF_NUINT8,
		MBF_NUINT16,
		MBF_NUINT32,

		//! Integers interpreted as being in the range -1.0f to 1.0f
		MBF_NINT8,
		MBF_NINT16,
		MBF_NINT32,

        //! Packed 1 to -1 value using multiply+add (128 is almost zero). Use 8-bit unsigned ints.
        MBF_PACKEDDIR8,

        //! Same as MBF_PACKEDDIR8, with the w component replaced with the sign of the determinant
        //! of the vertex basis to define the orientation of the tangent space in UE4 format.
        //! Use 8-bit unsigned ints.
        MBF_PACKEDDIR8_W_TANGENTSIGN,

        //! Packed 1 to -1 value using multiply+add (128 is almost zero). Use 8-bit signed ints.
        MBF_PACKEDDIRS8,

        //! Same as MBF_PACKEDDIRS8, with the w component replaced with the sign of the determinant
        //! of the vertex basis to define the orientation of the tangent space in UE4 format.
        //! Use 8-bit signed ints.
        MBF_PACKEDDIRS8_W_TANGENTSIGN,

		MBF_FLOAT64,

		MBF_COUNT,

		_MBF_FORCE32BITS = 0xFFFFFFFF

	} MESH_BUFFER_FORMAT;

	//!
	struct FMeshBufferFormatData
	{
		//! Size per component
		unsigned m_size;

		//! log 2 of the max value if integer
		uint8_t m_maxValueBits;
	};

	MUTABLERUNTIME_API const FMeshBufferFormatData& GetMeshFormatData(MESH_BUFFER_FORMAT format);


	//! Semantics of the mesh buffers
	//! \ingroup runtime
	typedef enum
	{

		MBS_NONE,

		//! For index buffers, and mesh morphs
		MBS_VERTEXINDEX,

		//! Standard vertex semantics
		MBS_POSITION,
		MBS_NORMAL,
		MBS_TANGENT,
		MBS_BINORMAL,
		MBS_TEXCOORDS,
		MBS_COLOUR,
		MBS_BONEWEIGHTS,
		MBS_BONEINDICES,

		//! Internal semantic indicating what layout block each vertex belongs to.
		//! It can be safely ignored if present in meshes returned by the system.
		//! It will never be in the same buffer that other vertex semantics.
		MBS_LAYOUTBLOCK,

		//! Internal semantic indicating what chart does the given vertex or face belong to.
		//! It can be safely ignored if present in meshes returned by the system.
		MBS_CHART,

		//! To let users define channels with semantics unknown to the system.
		//! These channels will never be transformed, and the per-vertex or per-index data will be
		//! simply copied.
		MBS_OTHER,

        //! Sign to define the orientation of the tangent space.
        MBS_TANGENTSIGN,

		//! Semantics usefule for mesh binding.
		MBS_TRIANGLEINDEX,
		MBS_BARYCENTRICCOORDS,
		MBS_DISTANCE,

		//! Utility
		MBS_COUNT,

        _MBS_FORCE32BITS = 0xFFFFFFFF
	} MESH_BUFFER_SEMANTIC;


	//!
	struct MESH_BUFFER_CHANNEL
	{
		MESH_BUFFER_CHANNEL()
		{
			m_semantic = MBS_NONE;
			m_format = MBF_NONE;
			m_componentCount = 0;
			m_semanticIndex = 0;
			m_offset = 0;
		}

		//!
		MESH_BUFFER_SEMANTIC m_semantic;

		//!
		MESH_BUFFER_FORMAT m_format;

		//! Index of the semantic, in case there are more than one of this type.
		uint8 m_semanticIndex;

		//! Offset in bytes from the begining of a buffer element
		uint8 m_offset;

		//! Number of components of the type in m_format for every value in the channel
		uint16 m_componentCount;

		//!
		inline bool operator==(const MESH_BUFFER_CHANNEL& o) const
		{
			return (m_semantic == o.m_semantic) &&
				(m_format == o.m_format) &&
				(m_semanticIndex == o.m_semanticIndex) &&
				(m_offset == o.m_offset) &&
				(m_componentCount == o.m_componentCount);
		}

	};

	MUTABLE_DEFINE_POD_SERIALISABLE(MESH_BUFFER_CHANNEL);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(MESH_BUFFER_CHANNEL);
	MUTABLE_DEFINE_ENUM_SERIALISABLE(MESH_BUFFER_FORMAT);
	MUTABLE_DEFINE_ENUM_SERIALISABLE(MESH_BUFFER_SEMANTIC);


	struct MESH_BUFFER
	{
		//!
		MESH_BUFFER()
		{
			m_elementSize = 0;
		}

		//!
		TArray<MESH_BUFFER_CHANNEL> m_channels;

		//!
		TArray<uint8> m_data;

		//!
		uint32 m_elementSize;

		//!
		inline void Serialise(OutputArchive& arch) const
		{
			arch << m_channels;
			arch << m_data;
			arch << m_elementSize;
		}

		//!
		inline void Unserialise(InputArchive& arch)
		{
			arch >> m_channels;
			arch >> m_data;
			arch >> m_elementSize;
		}

		//!
		inline bool operator==(const MESH_BUFFER& o) const
		{
			bool equal = (m_channels == o.m_channels);
			if (equal) equal = (m_elementSize == o.m_elementSize);
			if (equal) equal = (m_data == o.m_data);
			return equal;
		}

	};


	//! Set of buffers storing mesh element data. Elements can be vertices, indices or faces.
	class MUTABLERUNTIME_API FMeshBufferSet : public Base
	{
	public:

		//!
		uint32 m_elementCount = 0;

		//!
		TArray<MESH_BUFFER> m_buffers;

		//!
		inline void Serialise(OutputArchive& arch) const
		{
			arch << m_elementCount;
			arch << m_buffers;
		}

		//!
		inline void Unserialise(InputArchive& arch)
		{
			arch >> m_elementCount;
			arch >> m_buffers;
		}

		//!
		inline bool operator==(const FMeshBufferSet& o) const
		{
			return (m_elementCount == o.m_elementCount) &&
				(m_buffers == o.m_buffers);
		}

	public:

		//! Get the number of elements in the buffers.
		int32 GetElementCount() const;

		//! Set the number of vertices in the mesh. This will resize the vertex buffers keeping the
		//! previous data when possible. New data is undefined.
		void SetElementCount( int32 );

		//! Get the size in bytes of a buffer element.
		//! \param buffer index of the buffer from 0 to GetBufferCount()-1
		int32 GetElementSize( int32 buffer ) const;

		//! Get the number of vertex buffers in the mesh
		int32 GetBufferCount() const;

		//! Set the number of vertex buffers in the mesh.
		void SetBufferCount( int32 );

		//! Get the number of channels in a vertex buffer.
		//! \param buffer index of the vertex buffer from 0 to GetBufferCount()-1
		int32 GetBufferChannelCount( int32 buffer ) const;

		//! Get a channel of a buffer by index
		//! \param buffer index of the vertex buffer from 0 to GetBufferCount()-1
		//! \param channel index of the channel from 0 to GetBufferChannelCount( buffer )-1
		//! \param[out] pSemantic semantic of the channel
		//! \param[out] pSemanticIndex index of the semantic in case of having more than one of the
		//!				same type.
		//! \param[out] pFormat data format of the channel
		//! \param[out] pComponentCount components of an element of the channel
		//! \param[out] pOffset offset in bytes from the beginning of an element of the buffer
		void GetChannel
			(
				int32 buffer,
				int32 channel,
				MESH_BUFFER_SEMANTIC* pSemantic,
				int32* pSemanticIndex,
				MESH_BUFFER_FORMAT* pFormat,
				int32* pComponentCount,
				int32* pOffset
			) const;

		//! Set all the channels of a buffer
		//! \param buffer index of the buffer from 0 to GetBufferCount()-1
        //! \param elementSize sizei n bytes of a vertex element in this buffer
        //! \param channelCount number of channels to set in the buffer
        //! \param pSemantics buffer of channelCount semantics
        //! \param pSemanticIndices buffer of indices for the semantic of every channel
        //! \param pFormats buffer of channelCount formats
        //! \param pComponentCounts buffer of channelCount component counts
        //! \param pOffsets offsets in bytes of every particular channel inside the buffer element
        void SetBuffer
			(
				int32 buffer,
				int32 elementSize,
				int32 channelCount,
				const MESH_BUFFER_SEMANTIC* pSemantics=nullptr,
				const int32* pSemanticIndices=nullptr,
				const MESH_BUFFER_FORMAT* pFormats=nullptr,
				const int32* pComponentCounts=nullptr,
				const int32* pOffsets=nullptr
			);

		//! Set one  channels of a buffer
		//! \param buffer index of the buffer from 0 to GetBufferCount()-1
        //! \param elementSize sizei n bytes of a vertex element in this buffer
        //! \param channelIndex number of channels to set in the buffer
        void SetBufferChannel
			(
				int32 buffer,
				int32 channelIndex,
				MESH_BUFFER_SEMANTIC semantic,
				int32 semanticIndice,
				MESH_BUFFER_FORMAT format,
				int32 componentCount,
				int32 offset
			);

		//! Get a pointer to the object-owned data of a buffer.
		//! Channel data is interleaved for every element and packed in the order it was set
		//! without any padding.
		//! \param buffer index of the buffer from 0 to GetBufferCount()-1
		//! \todo Add padding support for better alignment of buffer elements.
        uint8* GetBufferData( int32 buffer );
        const uint8* GetBufferData( int32 buffer ) const;

		//-----------------------------------------------------------------------------------------
		// Utility methods
		//-----------------------------------------------------------------------------------------

		//! Find the index of a buffer channel by semantic and relative index inside the semantic.
        //! \param semantic Semantic of the channel we are searching.
        //! \param semanticIndex Index of the semantic of the channel we are searching. e.g. if we
        //!         want the second set of texture coordinates, it should be 1.
        //! \param[out] pBuffer -1 if the channel is not found, otherwise it will contain the index
		//! of the buffer where the channel was found.
		//! \param[out] pChannel -1 if the channel is not found, otherwise it will contain the
		//! channel index of the channel inside the buffer returned at [buffer]
		void FindChannel
			(
				MESH_BUFFER_SEMANTIC semantic,
				int semanticIndex,
				int* pBuffer, int* pChannel
			) const;

		//! Get the offset in bytes of the data of this channel inside an element data.
		//! \param buffer index of the buffer from 0 to GetBufferCount()-1
		//! \param channel index of the channel from 0 to GetBufferChannelCount( buffer )-1
		int32 GetChannelOffset( int32 buffer, int32 channel ) const;

		//! Add a new buffer by cloning a buffer from another set.
		//! The buffers must have the same number of elements.
		void AddBuffer( const FMeshBufferSet& Other, int32 buffer );

		//! Return true if the formats of the two vertex buffers set match.
		bool HasSameFormat( const FMeshBufferSet& Other ) const;

		//! Remove the buffer at the specified position. This "invalidates" any buffer index that
		//! was referencing buffers after the removed one.
		void RemoveBuffer( int32 BufferIndex);

	public:

		//! Copy an element from one position to another, overwriting the other element.
		//! Both positions must be valid, buffer size won't change.
		void CopyElement(uint32 fromIndex, uint32 toIndex);

		//! Compare the format of the two buffers at index buffer and return true if they match.
		bool HasSameFormat(int32 buffer, const FMeshBufferSet& pOther) const;

		//! Get the total memory size of the buffers
		size_t GetDataSize() const;

		//! Compare the mesh buffer with another one, but ignore internal data like generated
		//! vertex indices.
		bool IsSpecialBufferToIgnoreInSimilar(const MESH_BUFFER& b) const;

		//! Compare the mesh buffer with another one, but ignore internal data like generated
		//! vertex indices. Be aware this method compares the data byte by byte without checking
		//! if the data belong to the buffer components and could give false negatives if unset 
		//! padding data is present.
		bool IsSimilar(const FMeshBufferSet& o) const;

		//! Compare the mesh buffer with another one, but ignore internal data like generated
		//! vertex indices. This version compares the data component-wise, skipping any memory
		//! not specified in the buffer description.	
		bool IsSimilarRobust(const FMeshBufferSet& Other,bool bCompareUVs) const;

		//! Change the buffer descriptions so that all buffer indices start at 0 and are in the
		//! same order than memory.
		void ResetBufferIndices();

		//!
		void UpdateOffsets(int32 b);
	};

}

