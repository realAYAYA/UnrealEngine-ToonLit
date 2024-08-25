// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "Containers/Array.h"
#include "Misc/AssertionMacros.h"
#include "Math/IntVector.h"

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
		FIXED_LAYOUT,
		OVERLAY_LAYOUT
	};

	//! Types of layout reduction methods 
	enum class EReductionMethod : uint32
	{
		HALVE_REDUCTION,	// Divide axis by 2
		UNITARY_REDUCTION	// Reduces 1 block the axis 
	};


    //! \brief Image block layout class.
    //!
    //! It contains the information about what blocks are defined in a texture layout (texture
    //! coordinates set from a mesh).
    //! It is usually not necessary to use this objects, except for some advanced cases.
	//! \ingroup runtime
    class MUTABLERUNTIME_API Layout : public Resource
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

		// Resource interface
		int32 GetDataSize() const override;

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
		void GetBlock( int index, uint16* pMinX, uint16* pMinY, uint16* pSizeX, uint16* pSizeY ) const;

		//! Returns the reduction priority of a block.
		//! \param index Block to get priority.
		//! \param[out] priority reduction priority of the block
		//! \param[out] bReduceBothAxes reduction method of the block
		//! \param[out] bReduceByTwo reduction method of the block
		void GetBlockOptions(int index, int& priority, bool& bReduceBothAxes, bool& bReduceByTwo) const;

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

		//! Set the reduction options of a block
		//! \param index Block to set the options
		//! \param priority will be set to the reduction priority of the block. The blocks with the highest values will be the last to be reduced
		//! \param bReduceBothAxes will be set to reduce the block in both axis at the same time.
		//! \param bReduceByTwo will reduce by two blocks on a unitary reduction.
		void SetBlockOptions(int index, int priority, bool bReduceBothAxes, bool bReduceByTwo);

		//! Set the texture layout packing strategy
		//! By default the texture layout packing strategy is set to resizable layout
		void SetLayoutPackingStrategy(EPackStrategy _strategy);

		//! Set the texture layout packing strategy
		EPackStrategy GetLayoutPackingStrategy() const;

		//! Set at which LOD the unassigned vertices warnings will star to be ignored
		void SetIgnoreLODWarnings(int32 LOD);

		//! Get the LOD where the unassigned vertices warnings starts to be ignored
		int32 GetIgnoreLODWarnings();

		//! Set the block reduction method a the Fixed_Layout strategy
		void SetBlockReductionMethod(EReductionMethod Method);

		//! Returns the block reduction method
		EReductionMethod GetBlockReductionMethod() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~Layout() {}

	public:

		// This used to be the data in the private implementation of the image interface
		//-----------------------------------------------------------------------------------------
		struct FBlock
		{
			FBlock(UE::Math::TIntVector2<uint16> min = UE::Math::TIntVector2<uint16>(), UE::Math::TIntVector2<uint16> size = UE::Math::TIntVector2<uint16>())
			{
				m_min = min;
				m_size = size;
				m_id = -1;
				m_priority = 0;
				bReduceBothAxes = false;
				bReduceByTwo = false;
			}

			UE::Math::TIntVector2<uint16> m_min = UE::Math::TIntVector2<uint16>(0,0);
			UE::Math::TIntVector2<uint16> m_size = UE::Math::TIntVector2<uint16>(0, 0);

			//! Absolute id used to control merging of various layouts
			int32 m_id;

			//! Priority value to control the shrink texture layout strategy
			int32 m_priority;

			//! Value to control the method to reduce the block
			bool bReduceBothAxes;

			//! Value to control if a block has to be reduced by two in an unitary reduction strategy
			bool bReduceByTwo;


			//!
			void Serialise(OutputArchive& arch) const;

			//!
			void Unserialise(InputArchive& arch);

			//!
			inline bool operator==(const FBlock& o) const
			{
				return (m_min == o.m_min) &&
					(m_size == o.m_size) &&
					(m_id == o.m_id) &&
					(m_priority == o.m_priority) &&
					(bReduceBothAxes == o.bReduceBothAxes) &&
					(bReduceByTwo == o.bReduceByTwo);
			}

			inline bool IsSimilar(const FBlock& o) const
			{
				// All but ids
				return (m_min == o.m_min) &&
					(m_size == o.m_size) &&
					(m_priority == o.m_priority) &&
					(bReduceBothAxes == o.bReduceBothAxes) &&
					(bReduceByTwo == o.bReduceByTwo);
			}

			//!
			void UnserialiseOldVersion(InputArchive& Archive, const int32 Version);
		};


		//!
		UE::Math::TIntVector2<uint16> m_size = UE::Math::TIntVector2<uint16>(0, 0);

		UE::Math::TIntVector2<uint16> m_maxsize = UE::Math::TIntVector2<uint16>(0, 0);

		//!
		TArray<FBlock> m_blocks;

		//! Packing strategy
		EPackStrategy m_strategy = EPackStrategy::RESIZABLE_LAYOUT;
		 
		int32 FirstLODToIgnoreWarnings = 0;

		EReductionMethod ReductionMethod = EReductionMethod::HALVE_REDUCTION;


		//!
		void Serialise(OutputArchive& arch) const;

		//!
		void Unserialise(InputArchive& arch);

		//!
		bool IsSimilar(const Layout& o) const;

		//! Find a block by id. This converts the "absolute" id to a relative index to the layout
		//! blocks. Return -1 if not found.
		int32 FindBlock(int32 id) const;

		//! Return true if the layout is a single block filling all area.
		bool IsSingleBlockAndFull() const;
	};

}

