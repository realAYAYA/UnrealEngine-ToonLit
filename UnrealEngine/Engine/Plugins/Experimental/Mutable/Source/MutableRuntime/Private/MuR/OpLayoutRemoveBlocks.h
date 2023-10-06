// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Layout.h"
#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	inline Ptr<Layout> LayoutFromMesh_RemoveBlocks(const Mesh* InMesh, int32 InLayoutIndex)
	{
		if (!InMesh || InMesh->GetLayoutCount() <= InLayoutIndex)
		{
			return nullptr;
		}

		const Layout* Source = InMesh->GetLayout(InLayoutIndex);

		// Create the list of blocks in the mesh
		TArray<bool> blocksFound;
		blocksFound.SetNumZeroed(1024);

		UntypedMeshBufferIteratorConst itBlocks(InMesh->GetVertexBuffers(), MBS_LAYOUTBLOCK, InLayoutIndex);
		if (itBlocks.GetFormat() == MBF_UINT16)
		{
			const uint16* pBlocks = reinterpret_cast<const uint16*>(itBlocks.ptr());
			for (int32 i = 0; i < InMesh->GetVertexCount(); ++i)
			{
				if (pBlocks[i] >= blocksFound.Num())
				{
					blocksFound.SetNumZeroed(pBlocks[i] + 1024);
				}

				blocksFound[pBlocks[i]] = true;
			}
		}
		else if (itBlocks.GetFormat() == MBF_NONE)
		{
			// This seems to happen.
			// May this happen when entire meshes are removed?
			return Source->Clone();
		}
		else
		{
			// Format not supported yet
			check(false);
		}

		// Remove blocks that are not in the mesh
		Ptr<Layout> pResult = Source->Clone();
		int32 dest = 0;
		for (int32 b = 0; b < pResult->m_blocks.Num(); ++b)
		{
			int blockIndex = pResult->m_blocks[b].m_id;
			if (blockIndex < (int)blocksFound.Num() && blocksFound[blockIndex])
			{
				// keep
				pResult->m_blocks[dest] = pResult->m_blocks[b];
				++dest;
			}
		}
		pResult->SetBlockCount(dest);

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	inline Ptr<Layout> LayoutRemoveBlocks(const Layout* Source, const Layout* ReferenceLayout)
	{
		// Create the list of blocks in the mesh
		TArray<bool,TInlineAllocator<1024>> BlocksFound;
		BlocksFound.SetNumZeroed(1024);

		if (ReferenceLayout)
		{
			for (const Layout::FBlock& Block: ReferenceLayout->m_blocks)
			{
				if (Block.m_id >= BlocksFound.Num())
				{
					BlocksFound.SetNumZeroed(Block.m_id + 1024);
				}

				BlocksFound[Block.m_id] = true;
			}
		}

		// Remove blocks that are not in the mesh
		Ptr<Layout> pResult = Source->Clone();
		int dest = 0;
		for (int32 b = 0; b < pResult->m_blocks.Num(); ++b)
		{
			int blockIndex = pResult->m_blocks[b].m_id;
			if (blockIndex < (int)BlocksFound.Num() && BlocksFound[blockIndex])
			{
				// keep
				pResult->m_blocks[dest] = pResult->m_blocks[b];
				++dest;
			}
		}
		pResult->SetBlockCount(dest);

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	inline Ptr<Layout> LayoutMerge(const Layout* pA, const Layout* pB )
	{
		Ptr<Layout> pResult = pA->Clone();

		// This is faster but fails in the rare case of a block being in both layouts, which may 
		// happen if we merge a mesh with itself.
		//pResult->GetPrivate()->m_blocks.insert
		//(
		//	pResult->GetPrivate()->m_blocks.end(),
		//	pB->GetPrivate()->m_blocks.begin(),
		//	pB->GetPrivate()->m_blocks.end()
		//);

		for ( const Layout::FBlock& block: pB->m_blocks )
		{
			if ( pResult->FindBlock(block.m_id)<0 )
			{
				pResult->m_blocks.Add(block);
			}
		}

		return pResult;
	}


}
