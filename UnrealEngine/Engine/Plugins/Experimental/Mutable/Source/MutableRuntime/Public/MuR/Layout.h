// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MutableMath.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "Containers/Array.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"

namespace mu
{

	// Forward references
	class Layout;

	typedef Ptr<Layout> LayoutPtr;
	typedef Ptr<const Layout> LayoutPtrConst;

	//! Types of layout packing strategies 
	enum class EPackStrategy : uint32
	{
		RESIZABLE_LAYOUT,
		FIXED_LAYOUT
	};


    //! \brief Image block layout class.
    //!
    //! It contains the information about what blocks are defined in a texture layout (texture
    //! coordinates set from a mesh).
    //! It is usually not necessary to use this objects, except for some advanced cases.
	//! \ingroup runtime
    class MUTABLERUNTIME_API Layout : public RefCounted
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		Layout();

		//! Deep clone this layout.
		LayoutPtr Clone() const;

		//! Serialisation
		static void Serialise( const Layout* p, OutputArchive& arch );
		static LayoutPtr StaticUnserialise( InputArchive& arch );

		//! Full compare
		bool operator==( const Layout& other ) const;

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Get the resolution of the grid where the blocks are defined.
		FIntPoint GetGridSize() const;

		//! Get the resolution of the grid where the blocks are defined. It must be bigger than 0
		//! on each axis.
		//! \param sizeX width of the grid.
		//! \param sizeY height of the grid.
		void SetGridSize( int sizeX, int sizeY );

		//! Get the maximum resolution of the grid where the blocks are defined.
		//! \param[out] pSizeX The integer pointed by this will be set to the width of the grid.
		//! \param[out] pSizeY The integer pointed by this will be set to the height of the grid.
		void GetMaxGridSize(int* pSizeX, int* pSizeY) const;

		//! Get the maximum resolution of the grid where the blocks are defined. It must be bigger than 0
		//! on each axis.
		//! \param sizeX width of the grid.
		//! \param sizeY height of the grid.
		void SetMaxGridSize(int sizeX, int sizeY);

		//! Return the number of blocks in this layout.
		int32 GetBlockCount() const;

		//! Set the number of blocks in this layout.
		//! The existing blocks will be kept as much as possible. The new blocks will be undefined.
		void SetBlockCount( int32 );

		//! Return a block of the layout.
		//! "Position" here means the lower-left corner of the block.
		//! \param index Block to get.
		//! \param[out] pMinX will be set to the x position of the block
		//! (from 0 to the X size of the grid minus one )
		//! \param[out] pMinY will be set to the y position of the block
		//! (from 0 to the Y size of the grid minus one )
		//! \param[out] pSizeX will be set to the x size of the block
		//! \param[out] pSizeY will be set to the y size of the block
		void GetBlock( int index, int* pMinX, int* pMinY, int* pSizeX, int* pSizeY ) const;

		//! Returns the reduction priority of a block.
		//! \param index Block to get priority.
		//! \param[out] priority will be set to the reduction priority of the block
		void GetBlockPriority(int index, int* priority) const;

		//! Set a block of the layout.
		//! "Position" here means the lower-left corner of the block.
		//! \param index Block to set.
		//! \param minX will be set to the x position of the block
		//! (from 0 to the X size of the grid minus one )
		//! \param minY will be set to the y position of the block
		//! (from 0 to the Y size of the grid minus one )
		//! \param sizeX will be set to the x size of the block
		//! \param sizeY will be set to the y size of the block
        void SetBlock( int index, int minX, int minY, int sizeX, int sizeY );

		//! Set the reduction priority of a block
		//! The blocks with the highest values will be the last to be reduced
		//! \param index Block to set priority
		//! \param priority will be set to the reduction priority of the block
		void SetBlockPriority(int index, int priority);

		//! Set the texture layout packing strategy
		//! By default the texture layout packing strategy is set to resizable layout
		void SetLayoutPackingStrategy(EPackStrategy _strategy);

		//! Set the texture layout packing strategy
		EPackStrategy GetLayoutPackingStrategy() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~Layout() {}

	public:

		// This used to be the data in the private implementation of the image interface
		//-----------------------------------------------------------------------------------------
		struct BLOCK
		{
			BLOCK(vec2<uint16> min = vec2<uint16>(), vec2<uint16> size = vec2<uint16>())
			{
				m_min = min;
				m_size = size;
				m_id = -1;
				m_priority = 0;
			}

			vec2<uint16> m_min;
			vec2<uint16> m_size;

			//! Absolute id used to control merging of various layouts
			int32 m_id;

			//! Priority value to control the shrink texture layout strategy
			int32 m_priority;


			//!
			void Serialise(OutputArchive& arch) const
			{
				arch << m_min;
				arch << m_size;
				arch << m_id;
				arch << m_priority;
			}

			//!
			void Unserialise(InputArchive& arch)
			{
				arch >> m_min;
				arch >> m_size;
				arch >> m_id;
				arch >> m_priority;
			}

			//!
			inline bool operator==(const BLOCK& o) const
			{
				return (m_min == o.m_min) &&
					(m_size == o.m_size) &&
					(m_id == o.m_id) &&
					(m_priority == o.m_priority);
			}

			inline bool IsSimilar(const BLOCK& o) const
			{
				// All but ids
				return (m_min == o.m_min) &&
					(m_size == o.m_size) &&
					(m_priority == o.m_priority);
			}
		};


		//!
		vec2<uint16> m_size;

		vec2<uint16> m_maxsize;

		//!
		TArray<BLOCK> m_blocks;

		//! Packing strategy
		EPackStrategy m_strategy = EPackStrategy::RESIZABLE_LAYOUT;

		//!
		void Serialise(OutputArchive& arch) const
		{
			uint32 ver = 3;
			arch << ver;

			arch << m_size;
			arch << m_blocks;

			arch << m_maxsize;
			arch << uint32(m_strategy);
		}

		//!
		void Unserialise(InputArchive& arch)
		{
			uint32 ver;
			arch >> ver;
			check(ver == 3);

			arch >> m_size;

			arch >> m_blocks;
			arch >> m_maxsize;

			uint32 temp;
			arch >> temp;
			m_strategy = EPackStrategy(temp);
		}

		//!
		bool IsSimilar(const Layout& o) const;

		//! Find a block by id. This converts the "absolute" id to a relative index to the layout
		//! blocks. Return -1 if not found.
		int32 FindBlock(int32 id) const;

		//! Return true if the layout is a single block filling all area.
		bool IsSingleBlockAndFull() const;
	};

}

