// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Layout.h"

#include "HAL/LowLevelMemTracker.h"
#include "Math/IntPoint.h"
#include "MuR/MutableMath.h"


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
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	bool Layout::operator==( const Layout& o ) const
	{
		return  (m_size == o.m_size) &&
			(m_maxsize == o.m_maxsize) &&
			(m_blocks == o.m_blocks) &&
			(m_strategy == o.m_strategy);
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
	void Layout::GetBlock( int index, int* pMinX, int* pMinY, int* pSizeX, int* pSizeY ) const
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
	void Layout::GetBlockPriority(int index, int* pPriority) const
	{
		check(index >= 0 && index < m_blocks.Num());
		check(pPriority);

		if (pPriority)
		{
			*pPriority = m_blocks[index].m_priority;
		}
	}


	//---------------------------------------------------------------------------------------------
    void Layout::SetBlock( int index, int minx, int miny, int sizex, int sizey )
	{
		check( index >=0 && index < m_blocks.Num() );

		// Keeps the id
		m_blocks[index].m_min = vec2<uint16>((uint16)minx, (uint16)miny);
		m_blocks[index].m_size = vec2<uint16>((uint16)sizex, (uint16)sizey);
	}


	//---------------------------------------------------------------------------------------------
	void Layout::SetBlockPriority(int index, int priority)
	{
		check(index >= 0 && index < m_blocks.Num());

		m_blocks[index].m_priority = priority;
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
			&& m_blocks[0].m_min == vec2<uint16>(0, 0)
			&& m_blocks[0].m_size == m_size)
		{
			return true;
		}
		return false;
	}
}

