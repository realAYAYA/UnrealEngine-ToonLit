// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeLayout.h"

#include "Math/IntPoint.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ConvertData.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeLayoutPrivate.h"
#include "MuT/NodePrivate.h"

#include <memory>
#include <utility>


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static NODE_TYPE s_nodeLayoutType = NODE_TYPE( "NodeLayout", Node::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	void NodeLayout::Serialise( const NodeLayout* p, OutputArchive& arch )
	{
        uint32_t ver = 0;
		arch << ver;

	#define SERIALISE_CHILDREN( C, ID ) \
		( const C* pTyped = dynamic_cast<const C*>(p) )			\
		{ 														\
            arch << (uint32_t)ID;								\
			C::Serialise( pTyped, arch );						\
		}														\

		if SERIALISE_CHILDREN( NodeLayoutBlocks				, 0 )
		//else if SERIALISE_CHILDREN( NodeMeshIdentity		, 1 )

#undef SERIALISE_CHILDREN
	}


	//---------------------------------------------------------------------------------------------
	NodeLayoutPtr NodeLayout::StaticUnserialise( InputArchive& arch )
	{
        uint32_t ver;
		arch >> ver;
		check( ver == 0 );

        uint32_t id;
		arch >> id;

		switch (id)
		{
		case 0 :  return NodeLayoutBlocks::StaticUnserialise( arch ); break;
		default : check(false);
		}

		return 0;
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeLayout::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const NODE_TYPE* NodeLayout::GetStaticType()
	{
		return &s_nodeLayoutType;
	}



	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeLayoutBlocks::Private::s_type =
			NODE_TYPE( "LayoutBlocks", NodeLayout::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeLayoutBlocks, EType::Blocks, Node, Node::EType::Layout)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeLayoutBlocks::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeLayoutBlocks::GetInputNode( int ) const
	{
		check(false);
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    void NodeLayoutBlocks::SetInputNode( int, NodePtr )
	{
		check(false);
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetGridSize( int x, int y )
	{
		m_pD->m_pLayout->SetGridSize( x, y );
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetMaxGridSize(int x, int y)
	{
		m_pD->m_pLayout->SetMaxGridSize(x, y);
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::GetGridSize( int* pX, int* pY ) const
	{
		FIntPoint grid = m_pD->m_pLayout->GetGridSize();
		if (pX) *pX = grid[0];
		if (pY) *pY = grid[1];
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::GetMaxGridSize(int* pX, int* pY) const
	{
		m_pD->m_pLayout->GetMaxGridSize(pX, pY);
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetBlockCount( int n )
	{
		m_pD->m_pLayout->SetBlockCount( n );
	}


	//---------------------------------------------------------------------------------------------
	int NodeLayoutBlocks::GetBlockCount()
	{
		return m_pD->m_pLayout->GetBlockCount();
	}


	//---------------------------------------------------------------------------------------------
    void NodeLayoutBlocks::SetBlock( int index, int minx, int miny, int sizex, int sizey )
	{
        m_pD->m_pLayout->SetBlock( index, minx, miny, sizex, sizey );
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::GetBlock(int index, int* minx, int* miny, int* sizex, int* sizey)
	{
		if (minx && miny && sizex && sizey)
		{
			m_pD->m_pLayout->GetBlock(index, minx, miny, sizex, sizey);
		}
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetBlockPriority(int index, int priority)
	{
		m_pD->m_pLayout->SetBlockPriority(index, priority);
	}


	//---------------------------------------------------------------------------------------------
	void NodeLayoutBlocks::SetLayoutPackingStrategy(mu::EPackStrategy strategy)
	{
		m_pD->m_pLayout->SetLayoutPackingStrategy(strategy);
	}

	//---------------------------------------------------------------------------------------------
	NodeLayoutBlocksPtr NodeLayoutBlocks::GenerateLayoutBlocks(const MeshPtr pMesh, int layoutIndex, int gridSizeX, int gridSizeY)
	{
		NodeLayoutBlocksPtr layout = nullptr;

		if (pMesh && layoutIndex >=0 && gridSizeX+gridSizeY>0)
		{
			int indexCount = pMesh->GetIndexCount();
			vector< vec2f > UVs(indexCount*2);

			UntypedMeshBufferIteratorConst indexIt(pMesh->GetIndexBuffers(), MBS_VERTEXINDEX, 0);
			UntypedMeshBufferIteratorConst texIt(pMesh->GetVertexBuffers(), MBS_TEXCOORDS, layoutIndex);
			
			//Getting UVs face by face
			for (int v = 0; v < indexCount/3; ++v)
			{
				uint32_t i_1 = indexIt.GetAsUINT32(); 
				indexIt++;
				uint32_t i_2 = indexIt.GetAsUINT32();
				indexIt++;
				uint32_t i_3 = indexIt.GetAsUINT32();
				indexIt++;

				float uv_1[2] = { 0.0f,0.0f };
				ConvertData(0, uv_1, MBF_FLOAT32, (texIt + i_1).ptr(), texIt.GetFormat());
				ConvertData(1, uv_1, MBF_FLOAT32, (texIt + i_1).ptr(), texIt.GetFormat());
				
				float uv_2[2] = { 0.0f,0.0f };
				ConvertData(0, uv_2, MBF_FLOAT32, (texIt + i_2).ptr(), texIt.GetFormat());
				ConvertData(1, uv_2, MBF_FLOAT32, (texIt + i_2).ptr(), texIt.GetFormat());

				float uv_3[2] = { 0.0f,0.0f };
				ConvertData(0, uv_3, MBF_FLOAT32, (texIt + i_3).ptr(), texIt.GetFormat());
				ConvertData(1, uv_3, MBF_FLOAT32, (texIt + i_3).ptr(), texIt.GetFormat());

				
				UVs[v * 6 + 0][0] = uv_1[0];
				UVs[v * 6 + 0][1] = uv_1[1];
				UVs[v * 6 + 1][0] = uv_2[0];
				UVs[v * 6 + 1][1] = uv_2[1];
						
				UVs[v * 6 + 2][0] = uv_2[0];
				UVs[v * 6 + 2][1] = uv_2[1];
				UVs[v * 6 + 3][0] = uv_3[0];
				UVs[v * 6 + 3][1] = uv_3[1];
						
				UVs[v * 6 + 4][0] = uv_3[0]; 
				UVs[v * 6 + 4][1] = uv_3[1]; 
				UVs[v * 6 + 5][0] = uv_1[0];
				UVs[v * 6 + 5][1] = uv_1[1];
			}

			layout = new NodeLayoutBlocks;
			layout->SetGridSize(gridSizeX, gridSizeY);
			layout->SetMaxGridSize(gridSizeX, gridSizeY);
			layout->SetLayoutPackingStrategy(EPackStrategy::RESIZABLE_LAYOUT);
			
			vector<box<vec2<int>>> blocks;
			
			//Generating blocks
			for (int i = 0; i < indexCount; ++i)
			{
				vec2<int> a, b;
			
				a[0] = (int)floor(UVs[i * 2][0] * gridSizeX);
				a[1] = (int)floor(UVs[i * 2][1] * gridSizeY);
									  
				b[0] = (int)floor(UVs[i * 2 +1][0] * gridSizeX);
				b[1] = (int)floor(UVs[i * 2 +1][1] * gridSizeY);

				//floor of UV = 1*gridSize is gridSize which is not a valid range
				if (a[0] == gridSizeX){ a[0] = gridSizeX-1; }
				if (a[1] == gridSizeY){	a[1] = gridSizeY-1;	}
				if (b[0] == gridSizeX){	b[0] = gridSizeX-1;	}
				if (b[1] == gridSizeY){ b[1] = gridSizeY-1;	}
			
				//a and b are in the same block
				if (a == b)
				{
					bool contains = false;
			
					for (size_t it = 0; it < blocks.size(); ++it)
					{
						if (blocks[it].Contains(a) || blocks[it].Contains(b))
						{
							contains = true;
						}
					}
					
					//There is no block that contains them 
					if (!contains)
					{
						box<vec2<int>> currBlock;
						currBlock.min = a;
						currBlock.size = vec2<int>(1, 1);
			
						blocks.push_back(currBlock);
					}
				}
				else //they are in different blocks
				{
					int idxA = -1;
					int idxB = -1;
					
					//Getting the blocks that contain them
					for (size_t it = 0; it < blocks.size(); ++it)
					{
						if (blocks[it].Contains(a))
						{
							idxA = (int)it;
						}
						if (blocks[it].Contains(b))
						{
							idxB = (int)it;
						}
					}
					
					//The blocks are not the same
					if (idxA != idxB)
					{
						box<vec2<int>> currBlock;
						
						//One of the blocks doesn't exist
						if (idxA != -1 && idxB == -1)
						{
							currBlock.min = b;
							currBlock.size = vec2<int>(1, 1);
							blocks[idxA].Bound(currBlock);
						}
						else if (idxB != -1 && idxA == -1)
						{
							currBlock.min = a;
							currBlock.size = vec2<int>(1, 1);
							blocks[idxB].Bound(currBlock);
						}
						else //Both exist
						{
							blocks[idxA].Bound(blocks[idxB]);
							blocks.erase(blocks.begin() + idxB);
						}
					}
					else //the blocks doesn't exist
					{
						if (idxA == -1)
						{
							box<vec2<int>> currBlockA;
							box<vec2<int>> currBlockB;
			
							currBlockA.min = a;
							currBlockB.min = b;
							currBlockA.size = vec2<int>(1, 1);
							currBlockB.size = vec2<int>(1, 1);
			
							currBlockA.Bound(currBlockB);
							blocks.push_back(currBlockA);
						}
					}
				}
			}
			
			bool intersections = true;
			
			//Cheking if the blocks intersect with each other
			while (intersections)
			{
				intersections = false;
			
				for (size_t i = 0; !intersections && i < blocks.size(); ++i)
				{
					for (size_t j = 0; j < blocks.size(); ++j)
					{
						if (i != j && blocks[i].IntersectsExclusive(blocks[j]))
						{
							blocks[i].Bound(blocks[j]);
							blocks.erase(blocks.begin() + j);
							intersections = true;
							break;
						}
					}
				}
			}
			
			int numBlocks = (int)blocks.size();
			
			//Generating layout blocks
			if (numBlocks > 0)
			{
				layout->SetBlockCount(numBlocks);
			
				for (int i = 0; i < numBlocks; ++i)
				{
					int blockIndex = i;
					int minX = blocks[i].min[0];
					int minY = blocks[i].min[1];
					int sizeX = blocks[i].size[0];
					int sizeY = blocks[i].size[1];
			
					layout->SetBlock(blockIndex, minX, minY, sizeX, sizeY);
				}
			}
		}

		return layout;
	}
}


