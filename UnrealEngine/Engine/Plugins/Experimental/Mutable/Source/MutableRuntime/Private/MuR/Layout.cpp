// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Layout.h"

#include "HAL/LowLevelMemTracker.h"
#include "Math/IntPoint.h"
#include "MuR/MutableMath.h"
#include "MuR/SerialisationPrivate.h"


namespace mu {

	//---------------------------------------------------------------------------------------------
	Layout::Layout()
	{
	}


	//---------------------------------------------------------------------------------------------
	void Layout::Serialise( const Layout* p, OutputArchive& arch )
	{
		arch << *p;
	}


	//---------------------------------------------------------------------------------------------
	LayoutPtr Layout::StaticUnserialise( InputArchive& arch )
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		LayoutPtr pResult = new Layout();
		arch >> *pResult;
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	LayoutPtr Layout::Clone() const
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		LayoutPtr pResult = new Layout();		
		pResult->m_size = m_size;
		pResult->m_maxsize = m_maxsize;
		pResult->m_blocks = m_blocks;
		pResult->m_strategy = m_strategy;
		pResult->FirstLODToIgnoreWarnings = FirstLODToIgnoreWarnings;
		pResult->ReductionMethod = ReductionMethod;
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	bool Layout::operator==( const Layout& o ) const
	{
		return  (m_size == o.m_size) &&
			(m_maxsize == o.m_maxsize) &&
			(m_blocks == o.m_blocks) &&
			(m_strategy == o.m_strategy) &&
			// maybe this is not needed
			(FirstLODToIgnoreWarnings == o.FirstLODToIgnoreWarnings) &&
			(ReductionMethod==o.ReductionMethod);
	}


	//---------------------------------------------------------------------------------------------
	int32 Layout::GetDataSize() const
	{
		return sizeof(Layout) + m_blocks.GetAllocatedSize();
	}


	//---------------------------------------------------------------------------------------------
	FIntPoint Layout::GetGridSize() const
	{
		return FIntPoint(m_size[0], m_size[1]);
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetGridSize( int sizeX, int sizeY )
	{
		check( sizeX>=0 && sizeY>=0 );
		m_size[0] = (uint16)sizeX;
		m_size[1] = (uint16)sizeY;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::GetMaxGridSize(int* pSizeX, int* pSizeY) const
	{
		check(pSizeX && pSizeY);

		if (pSizeX && pSizeY)
		{
			*pSizeX = m_maxsize[0];
			*pSizeY = m_maxsize[1];
		}
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetMaxGridSize(int sizeX, int sizeY)
	{
		check(sizeX >= 0 && sizeY >= 0);
		m_maxsize[0] = (uint16)sizeX;
		m_maxsize[1] = (uint16)sizeY;
	}


	//---------------------------------------------------------------------------------------------
	int32 Layout::GetBlockCount() const
	{
		return m_blocks.Num();
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetBlockCount( int32 n )
	{
		check( n>=0 );
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		m_blocks.SetNum( n );
	}


	//---------------------------------------------------------------------------------------------
	void Layout::GetBlock( int index, uint16* pMinX, uint16* pMinY, uint16* pSizeX, uint16* pSizeY ) const
	{
		check( index >=0 && index < m_blocks.Num() );
		check( pMinX && pMinY && pSizeX && pSizeY );

		if (pMinX && pMinY && pSizeX && pSizeY)
		{
			*pMinX = m_blocks[index].m_min[0];
			*pMinY = m_blocks[index].m_min[1];
			*pSizeX = m_blocks[index].m_size[0];
			*pSizeY = m_blocks[index].m_size[1];
		}
	}


	//---------------------------------------------------------------------------------------------
	void Layout::GetBlockOptions(int index, int& pPriority, bool& bReduceBothAxes, bool& bReduceByTwo) const
	{
		check(index >= 0 && index < m_blocks.Num());

		pPriority = m_blocks[index].m_priority;
		bReduceBothAxes = m_blocks[index].bReduceBothAxes;
		bReduceByTwo = m_blocks[index].bReduceByTwo;
	}


	//---------------------------------------------------------------------------------------------
    void Layout::SetBlock( int index, int minx, int miny, int sizex, int sizey )
	{
		check( index >=0 && index < m_blocks.Num() );

		// Keeps the id
		m_blocks[index].m_min = UE::Math::TIntVector2<uint16>((uint16)minx, (uint16)miny);
		m_blocks[index].m_size = UE::Math::TIntVector2<uint16>((uint16)sizex, (uint16)sizey);
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetBlockOptions(int index, int priority, bool bReduceBothAxes, bool bReduceByTwo)
	{
		check(index >= 0 && index < m_blocks.Num());

		m_blocks[index].m_priority = priority;
		m_blocks[index].bReduceBothAxes = bReduceBothAxes;
		m_blocks[index].bReduceByTwo = bReduceByTwo;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetLayoutPackingStrategy(EPackStrategy _strategy)
	{
		m_strategy = _strategy;
	}


	//---------------------------------------------------------------------------------------------
	EPackStrategy Layout::GetLayoutPackingStrategy() const
	{
		return m_strategy;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::Serialise(OutputArchive& arch) const
	{
		uint32 ver = 6;
		arch << ver;

		arch << m_size;
		arch << m_blocks;

		arch << m_maxsize;
		arch << uint32(m_strategy);
		arch << FirstLODToIgnoreWarnings;
		arch << uint32(ReductionMethod);
	}

	
	//---------------------------------------------------------------------------------------------
	void Layout::Unserialise(InputArchive& arch)
	{
		uint32 ver;
		arch >> ver;
		check(ver <= 6);

		arch >> m_size;

		if (ver < 6)
		{
			uint32_t Size = 0;
			arch >> Size;
			m_blocks.SetNum(Size);

			for (uint32_t BlockIndex = 0; BlockIndex < Size; ++BlockIndex)
			{
				// Unserialise taking into account the Layout version
				m_blocks[BlockIndex].UnserialiseOldVersion(arch, ver);
			}
		}
		else
		{
			arch >> m_blocks;
		}

		arch >> m_maxsize;

		uint32 temp;
		arch >> temp;
		m_strategy = EPackStrategy(temp);

		if (ver >= 4)
		{
			arch >> FirstLODToIgnoreWarnings;
		}

		if (ver >= 5)
		{
			arch >> temp;
			ReductionMethod = EReductionMethod(temp);
		}
	}


	//---------------------------------------------------------------------------------------------
	bool Layout::IsSimilar(const Layout& o) const
	{
		if (m_size != o.m_size || m_maxsize != o.m_maxsize ||
			m_blocks.Num() != o.m_blocks.Num() || m_strategy != o.m_strategy)
			return false;

		for (int32 i = 0; i < m_blocks.Num(); ++i)
		{
			if (!m_blocks[i].IsSimilar(o.m_blocks[i])) return false;
		}

		return true;

	}


	//---------------------------------------------------------------------------------------------
	int32 Layout::FindBlock(int32_t id) const
	{
		check(id >= 0);
		int res = -1;
		for (int32 i = 0; res < 0 && i < m_blocks.Num(); ++i)
		{
			if (m_blocks[i].m_id == id)
			{
				res = i;
			}
		}

		return res;
	}


	//---------------------------------------------------------------------------------------------
	bool Layout::IsSingleBlockAndFull() const
	{
		if (m_blocks.Num() == 1
			&& m_blocks[0].m_min == UE::Math::TIntVector2<uint16>(0, 0)
			&& m_blocks[0].m_size == m_size)
		{
			return true;
		}
		return false;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetIgnoreLODWarnings(int32 LOD)
	{
		FirstLODToIgnoreWarnings = LOD;
	}


	//---------------------------------------------------------------------------------------------
	int32 Layout::GetIgnoreLODWarnings()
	{
		return FirstLODToIgnoreWarnings;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetBlockReductionMethod(EReductionMethod Method)
	{
		ReductionMethod = Method;
	}


	//---------------------------------------------------------------------------------------------
	EReductionMethod Layout::GetBlockReductionMethod() const
	{
		return ReductionMethod;
	}

	
	//---------------------------------------------------------------------------------------------
	void Layout::FBlock::Serialise(OutputArchive& arch) const
	{
		arch << m_min;
		arch << m_size;
		arch << m_id;
		arch << m_priority;
		arch << bReduceBothAxes;
		arch << bReduceByTwo;
	}


	//---------------------------------------------------------------------------------------------
	void Layout::FBlock::Unserialise(InputArchive& arch)
	{
		arch >> m_min;
		arch >> m_size;
		arch >> m_id;
		arch >> m_priority;
		arch >> bReduceBothAxes;
		arch >> bReduceByTwo;
	}

	
	//---------------------------------------------------------------------------------------------
	void Layout::FBlock::UnserialiseOldVersion(InputArchive& Archive, const int32 Version)
	{
		Archive >> m_min;
		Archive >> m_size;
		Archive >> m_id;
		Archive >> m_priority;

		if (Version >= 5)
		{
			Archive >> bReduceBothAxes;
		}
	}
}

