// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalHashedVertexDescriptor.h: Metal RHI Hashed Vertex Descriptor.
=============================================================================*/

#pragma once


//------------------------------------------------------------------------------

#pragma mark - Metal Hashed Vertex Descriptor


/**
 * The MTLVertexDescriptor and a pre-calculated hash value used to simplify
 * comparisons (as vendor MTLVertexDescriptor implementations are not all
 * comparable).
 */
struct FMetalHashedVertexDescriptor
{
	NSUInteger VertexDescHash;
	mtlpp::VertexDescriptor VertexDesc;

	FMetalHashedVertexDescriptor();
	FMetalHashedVertexDescriptor(mtlpp::VertexDescriptor Desc, uint32 Hash);
	FMetalHashedVertexDescriptor(FMetalHashedVertexDescriptor const& Other);
	~FMetalHashedVertexDescriptor();

	FMetalHashedVertexDescriptor& operator=(FMetalHashedVertexDescriptor const& Other);
	bool operator==(FMetalHashedVertexDescriptor const& Other) const;

	friend uint32 GetTypeHash(FMetalHashedVertexDescriptor const& Hash)
	{
		return Hash.VertexDescHash;
	}
};
