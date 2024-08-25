// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


// 1 + 7 + 24 bits_
#define  INSTANCE_HIERARCHY_ITEM_CHUNK_COUNT_SHIFT  (24u)
#define  INSTANCE_HIERARCHY_ITEM_CHUNK_ID_MASK  ((1u << INSTANCE_HIERARCHY_ITEM_CHUNK_COUNT_SHIFT) - 1u)
#define  INSTANCE_HIERARCHY_ITEM_CHUNK_COMPRESSED_FLAG  (1u << 31u)
#define  INSTANCE_HIERARCHY_ITEM_CHUNK_COMPRESSED_PAYLOAD_MASK  ((1u << 31u) - 1u)
#define  INSTANCE_HIERARCHY_MAX_CHUNK_SIZE  (64u)

#ifdef __cplusplus
#include "HLSLTypeAliases.h"

namespace UE::HLSL
{
#endif
#ifndef __cplusplus //HLSL
#include "/Engine/Private/LargeWorldCoordinates.ush"
#endif

// Representation in hierarchy buffer
struct FPackedCellHeader
{
	uint ItemChunksOffset;
	uint NumStaticDynamicItemChunks; // 2x16 bits: (Static << 16) + Dynamic
};

// Unpacked version of the struct
struct FCellHeader
{
	uint NumItemChunks;
	uint ItemChunksOffset;
	uint NumStaticChunks;
};

inline FCellHeader UnpackCellHeader(FPackedCellHeader Packed)
{
	FCellHeader Result;
	Result.ItemChunksOffset = Packed.ItemChunksOffset;
	Result.NumStaticChunks = Packed.NumStaticDynamicItemChunks >> 16;
	Result.NumItemChunks = (Packed.NumStaticDynamicItemChunks & 0xFFFFu) + Result.NumStaticChunks;
	return Result;
}

struct FCellBlockData
{
	FDFVector3 WorldPos;
	float LevelCellSize; // Note, not the block size, but the cell size.
	uint Pad;
};

/**
 * Represent one item of work for the hierarchical culling stage, linking a cell to either a group of views (main-pass) or a singlular view (post-pass).
 */
struct FCellDraw
{
	uint CellId;
	uint ViewGroupId;
};

/**
 * Represent one item of work for the post-pass of the hierarchical culling stage.
 */
struct FOccludedCellDraw
{
	FCellDraw CellDraw;
	uint OccludedViewMask;
	uint Pad;
};
/**
 * Represents a group of views, e.g., for a clipmap or point light, or anything else really that share the same broad-phase culling result.
 * Wrt mip-views, there is no explicit handling in the hierarchical culling stage, as they are expected to come in a compact range (post view compaction)
 * A view group should typically share view flags, might want/need to make assumptions around that.
 */
struct FViewDrawGroup
{
	uint FirstView;
	uint NumViews;
};

// Info for one instance culling workgroup (64 threads).
// TODO: Pack/unpack into fewer bits?
// TODO: Move to some nanite specific header probably.
struct FInstanceCullingGroupWork
{
	uint ViewGroupId;
	uint PackedItemChunkDesc;
	uint ActiveViewMask; // Up to 32 active views in the group (NOTE: this may overflow for example if all mip levels were mapped at the same time on a point light, 48 mips)
	uint Pad;
};



#ifdef __cplusplus
} // namespace

using FCellHeader = UE::HLSL::FCellHeader;
using FPackedCellHeader = UE::HLSL::FPackedCellHeader;
using FCellBlockData = UE::HLSL::FCellBlockData;
using FCellDraw = UE::HLSL::FCellDraw;
using FOccludedCellDraw = UE::HLSL::FOccludedCellDraw;
using FViewDrawGroup = UE::HLSL::FViewDrawGroup;
using FInstanceCullingGroupWork = UE::HLSL::FInstanceCullingGroupWork;

#endif
