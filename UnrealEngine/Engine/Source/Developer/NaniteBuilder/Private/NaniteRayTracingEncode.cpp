// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteEncode.h"

#include "Rendering/NaniteResources.h"
#include "Hash/CityHash.h"
#include "Math/UnrealMath.h"
#include "Cluster.h"
#include "ClusterDAG.h"
#include "Async/ParallelFor.h"
#include "Misc/Compression.h"

#include <embree3/rtcore.h>
#include <embree3/rtcore_builder.h>

namespace Nanite
{

// Embree wrappers for internal and leaf nodes
class FEmbreeBVH8Node
{
public:
	static const uint32 MaxNumChildren = 8;

	FVector3f ChildrenMin[MaxNumChildren];
	FVector3f ChildrenMax[MaxNumChildren];
	FEmbreeBVH8Node* Children[MaxNumChildren] = {};
	uint32 ChildCount = 0;

	virtual bool IsLeaf() const 
	{ 
		return false;
	}	

	static void* Create(RTCThreadLocalAllocator Allocator, unsigned int ChildCount, void* UserPtr)
	{
		void* MemPtr = rtcThreadLocalAlloc(Allocator, sizeof(FEmbreeBVH8Node), 16);
		return (void*) new(MemPtr) FEmbreeBVH8Node;
	}

	static void SetNodeChildren(void* NodePtr, void** Children, unsigned int ChildCount, void* UserPtr)
	{
		check(ChildCount > 0);

		FEmbreeBVH8Node* Node = (FEmbreeBVH8Node*)NodePtr;
		Node->ChildCount = ChildCount;

		for (uint32 i = 0; i < ChildCount; i++)
		{
			Node->Children[i] = ((FEmbreeBVH8Node**)Children)[i];
		}		
	}

	static void SetNodeBounds(void* NodePtr, const struct RTCBounds** Bounds, unsigned int ChildCount, void* UserPtr)
	{
		FEmbreeBVH8Node* Node = (FEmbreeBVH8Node*)NodePtr;
		for (uint32 i = 0; i < ChildCount; i++)
		{
			Node->ChildrenMin[i] = FVector3f(Bounds[i]->lower_x, Bounds[i]->lower_y, Bounds[i]->lower_z);
			Node->ChildrenMax[i] = FVector3f(Bounds[i]->upper_x, Bounds[i]->upper_y, Bounds[i]->upper_z);
		}
	}
};

class FEmbreeBVH8LeafNode : public FEmbreeBVH8Node
{
public:
	static const uint32 MaxNumPrimitives = 3;

	uint32 LeafPrimitiveIDs[MaxNumPrimitives];
	uint32 LeafPrimitiveCount = 0;

	bool IsLeaf() const override final
	{
		return true;
	}

	static void* Create(RTCThreadLocalAllocator Allocator, const RTCBuildPrimitive* Primitives, size_t PrimitiveCount, void* UserPtr)
	{
		check(PrimitiveCount > 0 && PrimitiveCount <= MaxNumPrimitives);
		
		void* MemPtr = rtcThreadLocalAlloc(Allocator, sizeof(FEmbreeBVH8LeafNode), 16);
		FEmbreeBVH8LeafNode* NewNode = new(MemPtr) FEmbreeBVH8LeafNode;
		NewNode->LeafPrimitiveCount = (uint32)PrimitiveCount;

		for (uint32 i = 0; i < PrimitiveCount; i++)
		{
			NewNode->LeafPrimitiveIDs[i] = Primitives[i].primID;
		}
		
		return NewNode;
	}
};

void SplitPrimitive(const struct RTCBuildPrimitive* Primitive, unsigned int dimension, float Position, struct RTCBounds* LeftBounds, struct RTCBounds* RightBounds, void* userPtr)
{
	LeftBounds->lower_x = Primitive->lower_x;
	LeftBounds->lower_y = Primitive->lower_y;
	LeftBounds->lower_z = Primitive->lower_z;

	LeftBounds->upper_x = Primitive->upper_x;
	LeftBounds->upper_y = Primitive->upper_y;
	LeftBounds->upper_z = Primitive->upper_z;

	RightBounds->lower_x = Primitive->lower_x;
	RightBounds->lower_y = Primitive->lower_y;
	RightBounds->lower_z = Primitive->lower_z;

	RightBounds->upper_x = Primitive->upper_x;
	RightBounds->upper_y = Primitive->upper_y;
	RightBounds->upper_z = Primitive->upper_z;

	switch (dimension)
	{
	case 0:
		LeftBounds->upper_x = RightBounds->lower_x = Position;
		break;
	case 1:
		LeftBounds->upper_y = RightBounds->lower_y = Position;
		break;
	case 2:
		LeftBounds->upper_z = RightBounds->lower_z = Position;
		break;
	default:
		checkNoEntry();
	}
}

// todo: review what is actually needed and what can be reused from cluster
// most likely candidate is LocalGridOrigin
struct FCompressedBVHNodeData
{
	// float4 (offset 0)
	float LocalGridOrigin[3];
	int8 Exponent[3];
	uint8 InternalNodeMask;

	// float4 (offset 16)
	uint32 ChildBaseIndex; //x
	uint32 TriangleBaseOffset; //y
	uint8 ChildMetaField[8]; //zw
	
	// float4 (offset 32)
	// qlo xyz
	uint8 QuantizedBoxMin_x[8];
	uint8 QuantizedBoxMin_y[8];
	uint8 QuantizedBoxMin_z[8];

	// qhi xyz
	uint8 QuantizedBoxMax_x[8];
	uint8 QuantizedBoxMax_y[8];
	uint8 QuantizedBoxMax_z[8];
};

static_assert(sizeof(FCompressedBVHNodeData) == NANITE_RAY_TRACING_NODE_DATA_SIZE_IN_BYTES, "FCompressedBVHNodeData size needs to be exactly 80");

// Reassign nodes for octant traversal using auction algorithm
static void ReassignNodeChildren(FEmbreeBVH8Node* InOutNode, FVector3f BoundingBoxMin, FVector3f BoundingBoxMax)
{
	const FVector3f NodeCentroid = (BoundingBoxMin + BoundingBoxMax) * 0.5f;
	
	float Cost[8][8];

	for (uint32 SlotIndex = 0; SlotIndex < 8; SlotIndex++)
	{
		FVector3f RayDirection = FVector3f(
			(((SlotIndex >> 2) & 1) == 1) ? -1.0f : 1.0f,
			(((SlotIndex >> 1) & 1) == 1) ? -1.0f : 1.0f,
			(((SlotIndex >> 0) & 1) == 1) ? -1.0f : 1.0f
		);

		for (int ChildIndex = 0; ChildIndex < 8; ChildIndex++)
		{
			if (InOutNode->Children[ChildIndex] != nullptr)
			{
				FVector3f ChildCentroid = (InOutNode->ChildrenMin[ChildIndex] + InOutNode->ChildrenMax[ChildIndex]) * 0.5f;
				Cost[SlotIndex][ChildIndex] = -FVector3f::DotProduct(ChildCentroid - NodeCentroid, RayDirection);
			}
		}
	}

	// Simple auction algorithm for slot assignment		
	const uint32 NumSlots = 8;
	const uint32 NumChildren = InOutNode->ChildCount;
	const uint32 NumIterations = 1;

	const float AuctionEpsilonDecay = 0.25f;

	int32 Assignment[8];
	int32 ChildToSlot[8];
	float Prices[8];

	for (int i = 0; i < 8; i++)
	{
		Prices[i] = 0.0f;
	}

	uint32 Iter = 0;
	float AuctionEpsilon = 1.0f;
	while (Iter < NumIterations)
	{
		for (int i = 0; i < 8; i++)
		{
			Assignment[i] = -1;
			ChildToSlot[i] = -1;
		}

		uint32 NumAssigned = 0;

		while (NumAssigned < NumChildren)
		{
			float Bids[8][8] = {};
			uint8 BidsMask = 0;

			// Bidding Phase
			for (uint32 Slot = 0; Slot < NumSlots; Slot++)
			{
				if (Assignment[Slot] == -1)
				{
					float TopValues[2] = { -FLT_MAX, -FLT_MAX };
					uint32 TopValueIndex = -1;

					for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
					{
						float CurrentValue = Cost[Slot][ChildIndex] - Prices[ChildIndex];
						if (CurrentValue > TopValues[0])
						{
							TopValues[1] = TopValues[0];
							TopValues[0] = CurrentValue;

							TopValueIndex = ChildIndex;
						}
						else if (CurrentValue > TopValues[1])
						{
							TopValues[1] = CurrentValue;
						}
					}

					CA_ASSUME(TopValueIndex < 8);

					const float Bid = TopValues[0] - TopValues[1] + AuctionEpsilon;
					Bids[Slot][TopValueIndex] = Bid;
					BidsMask |= (1 << TopValueIndex);
				}
			}

			// Assignment Phase
			for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
			{
				const bool bBids = (BidsMask >> ChildIndex) & 1;
				if (bBids)
				{
					float HighestBid = 0.0f;
					int32 HighestBidder = -1;

					for (uint32 Slot = 0; Slot < NumSlots; Slot++)
					{
						if (Bids[Slot][ChildIndex] > HighestBid)
						{
							HighestBid = Bids[Slot][ChildIndex];
							HighestBidder = Slot;
						}
					}

					int32 CurrentOwner = ChildToSlot[ChildIndex];
					if (CurrentOwner >= 0)
					{
						Assignment[CurrentOwner] = -1;
					}
					else
					{
						NumAssigned++;
					}

					Prices[ChildIndex] += HighestBid;
					Assignment[HighestBidder] = ChildIndex;
					ChildToSlot[ChildIndex] = HighestBidder;
				}
			}
		}

		Iter++;
		AuctionEpsilon *= AuctionEpsilonDecay;
	}

	FEmbreeBVH8Node TempNode = *InOutNode;
	for (uint8 i = 0; i < 8; i++)
	{
		if (Assignment[i] != -1)
		{
			check(TempNode.Children[Assignment[i]]);
			InOutNode->Children[i] = TempNode.Children[Assignment[i]];
			InOutNode->ChildrenMin[i] = TempNode.ChildrenMin[Assignment[i]];
			InOutNode->ChildrenMax[i] = TempNode.ChildrenMax[Assignment[i]];
		}
		else
		{
			InOutNode->Children[i] = nullptr;
		}		
	}
}

static void CompressWideBVHNode(FEmbreeBVH8Node* Node, FCompressedBVHNodeData& DestNode, FVector3f BoundingBoxMin, FVector3f BoundingBoxMax, uint32 BaseChildOffset, TArray<uint8>& OutIndices)
{
	const uint32 Nq = 8;
	const float ExponentDenominator = 1.0f / (float)((1 << Nq) - 1);

	FVector3f BoundingBoxExtent = BoundingBoxMax - BoundingBoxMin;
	BoundingBoxExtent.X = FMath::Max(BoundingBoxExtent.X, UE_SMALL_NUMBER);
	BoundingBoxExtent.Y = FMath::Max(BoundingBoxExtent.Y, UE_SMALL_NUMBER);
	BoundingBoxExtent.Z = FMath::Max(BoundingBoxExtent.Z, UE_SMALL_NUMBER);

	const int32 Exponent_x = FMath::CeilToInt(FMath::Log2(BoundingBoxExtent.X * ExponentDenominator));
	const int32 Exponent_y = FMath::CeilToInt(FMath::Log2(BoundingBoxExtent.Y * ExponentDenominator));
	const int32 Exponent_z = FMath::CeilToInt(FMath::Log2(BoundingBoxExtent.Z * ExponentDenominator));

	const float InvPow2Exponent_x = FMath::Exp2(-(float)Exponent_x);
	const float InvPow2Exponent_y = FMath::Exp2(-(float)Exponent_y);
	const float InvPow2Exponent_z = FMath::Exp2(-(float)Exponent_z);

	uint32 LeafChildPrimitiveCount = 0;
	uint32 LeafPrimitiveOffset = OutIndices.Num();
	uint8 InternalNodeMask = 0;

	for (uint32 ChildIndex = 0; ChildIndex < 8; ChildIndex++)
	{
		if (Node->Children[ChildIndex] == nullptr)
		{
			continue;
		}

		FEmbreeBVH8Node* Child = Node->Children[ChildIndex];

		DestNode.QuantizedBoxMin_x[ChildIndex] = (uint8)FMath::FloorToInt((Node->ChildrenMin[ChildIndex].X - BoundingBoxMin.X) * InvPow2Exponent_x);
		DestNode.QuantizedBoxMin_y[ChildIndex] = (uint8)FMath::FloorToInt((Node->ChildrenMin[ChildIndex].Y - BoundingBoxMin.Y) * InvPow2Exponent_y);
		DestNode.QuantizedBoxMin_z[ChildIndex] = (uint8)FMath::FloorToInt((Node->ChildrenMin[ChildIndex].Z - BoundingBoxMin.Z) * InvPow2Exponent_z);
		DestNode.QuantizedBoxMax_x[ChildIndex] = (uint8)FMath::CeilToInt((Node->ChildrenMax[ChildIndex].X - BoundingBoxMin.X) * InvPow2Exponent_x);
		DestNode.QuantizedBoxMax_y[ChildIndex] = (uint8)FMath::CeilToInt((Node->ChildrenMax[ChildIndex].Y - BoundingBoxMin.Y) * InvPow2Exponent_y);
		DestNode.QuantizedBoxMax_z[ChildIndex] = (uint8)FMath::CeilToInt((Node->ChildrenMax[ChildIndex].Z - BoundingBoxMin.Z) * InvPow2Exponent_z);

		if (Child->IsLeaf())
		{
			FEmbreeBVH8LeafNode* LeafNode = (FEmbreeBVH8LeafNode*)Child;
			check(LeafNode->LeafPrimitiveCount > 0);

			uint32 UnaryEncodedPrimitiveCount = (1 << (LeafNode->LeafPrimitiveCount + 1)) - 1;
			uint32 MetaField = (UnaryEncodedPrimitiveCount << 5) | (LeafChildPrimitiveCount & 0b11111);
	
			DestNode.ChildMetaField[ChildIndex] = (uint8)MetaField;

			for (uint32 i = 0; i < LeafNode->LeafPrimitiveCount; i++)
			{
				uint32 PrimitiveIndex = LeafNode->LeafPrimitiveIDs[i];
				check(PrimitiveIndex >= 0 && PrimitiveIndex < 128);

				LeafChildPrimitiveCount++;
				OutIndices.Add((uint8)PrimitiveIndex);
			}
		}
		else
		{
			InternalNodeMask |= 1 << ChildIndex;

			DestNode.ChildMetaField[ChildIndex] = (1 << 5) | ((24 + ChildIndex) & 0b11111);
		}
	}

	DestNode.LocalGridOrigin[0] = BoundingBoxMin.X;
	DestNode.LocalGridOrigin[1] = BoundingBoxMin.Y;
	DestNode.LocalGridOrigin[2] = BoundingBoxMin.Z;

	DestNode.Exponent[0] = (int8)Exponent_x;
	DestNode.Exponent[1] = (int8)Exponent_y;
	DestNode.Exponent[2] = (int8)Exponent_z;
	DestNode.InternalNodeMask = InternalNodeMask;
	DestNode.ChildBaseIndex = BaseChildOffset;
	DestNode.TriangleBaseOffset = LeafPrimitiveOffset;
}

static void CompressWideBVHInternal(FCluster& Cluster, FEmbreeBVH8Node* Node, uint32 BVHNodeIndex, TArray<FCompressedBVHNodeData>& RTBVHNodesData, TArray<uint8>& OutIndices)
{
	FVector3f BoundingBoxMin = FVector3f(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector3f BoundingBoxMax = FVector3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	uint32 NumInternalNodes = 0;
	for (uint32 ChildIndex = 0; ChildIndex < 8; ChildIndex++)
	{
		if (Node->Children[ChildIndex] == nullptr)
		{
			continue;
		}

		const bool bInternalNode = !Node->Children[ChildIndex]->IsLeaf();
		if (bInternalNode)
		{
			NumInternalNodes++;
		}

		BoundingBoxMin = FVector3f::Min(BoundingBoxMin, Node->ChildrenMin[ChildIndex]);
		BoundingBoxMax = FVector3f::Max(BoundingBoxMax, Node->ChildrenMax[ChildIndex]);
	}

#define USE_CLUSTER_BOUNDS 0
#if USE_CLUSTER_BOUNDS
	BoundingBoxMin = Cluster.Bounds.Min;
#endif

	ReassignNodeChildren(Node, BoundingBoxMin, BoundingBoxMax);

	uint32 BaseChildOffset = RTBVHNodesData.Num();	
	if (NumInternalNodes > 0)
	{
		RTBVHNodesData.AddZeroed(NumInternalNodes);
	}	

	CompressWideBVHNode(Node, RTBVHNodesData[BVHNodeIndex], BoundingBoxMin, BoundingBoxMax, BaseChildOffset, OutIndices);

	uint32 InternalNodeOffset = BaseChildOffset;
	for (uint32 ChildIndex = 0; ChildIndex < 8; ChildIndex++)
	{
		const bool bInternalNode = Node->Children[ChildIndex] && !Node->Children[ChildIndex]->IsLeaf();
		if (bInternalNode)
		{
			CompressWideBVHInternal(Cluster, Node->Children[ChildIndex], InternalNodeOffset, RTBVHNodesData, OutIndices);
			InternalNodeOffset++;
		}
	}
}

static void PatchIndicesOffset(TArray<FCompressedBVHNodeData>& RTBVHNodeData)
{
	uint32 BaseByteOffset = RTBVHNodeData.Num() * NANITE_RAY_TRACING_NODE_DATA_SIZE_IN_BYTES;
	for (int32 i = 0; i < RTBVHNodeData.Num(); i++)
	{		
		RTBVHNodeData[i].TriangleBaseOffset += BaseByteOffset;
	}
}

static void ConvertToCompressedWideBVH(FCluster& Cluster, FEmbreeBVH8Node* Root, TArray<FCompressedBVHNodeData>& RTBVHNodeData, TArray<uint8>& OutIndices)
{
	// Root
	RTBVHNodeData.AddZeroed();

	CompressWideBVHInternal(Cluster, Root, 0, RTBVHNodeData, OutIndices);

	PatchIndicesOffset(RTBVHNodeData);
}

void BuildRayTracingData(FResources& Resources, TArray< FCluster >& Clusters)
{	
	FCriticalSection AddDataCS;
	
	ParallelFor(TEXT("Nanite.BuildRayTracingData.PF"), Clusters.Num(), 1, [&](uint32 ClusterIndex)
		{
			FCluster& Cluster = Clusters[ClusterIndex];

			TArray<RTCBuildPrimitive> AABBs;
			AABBs.Reserve(Cluster.NumTris * FEmbreeBVH8Node::MaxNumChildren);

			for (uint32 TriIndex = 0; TriIndex < Cluster.NumTris; TriIndex++)
			{
				RTCBuildPrimitive& AABB = AABBs.AddZeroed_GetRef();

				const FVector3f& v0 = Cluster.GetPosition(Cluster.Indexes[TriIndex * 3 + 0]);
				const FVector3f& v1 = Cluster.GetPosition(Cluster.Indexes[TriIndex * 3 + 1]);
				const FVector3f& v2 = Cluster.GetPosition(Cluster.Indexes[TriIndex * 3 + 2]);

				AABB.lower_x = FMath::Min3(v0.X, v1.X, v2.X);
				AABB.lower_y = FMath::Min3(v0.Y, v1.Y, v2.Y);
				AABB.lower_z = FMath::Min3(v0.Z, v1.Z, v2.Z);

				AABB.upper_x = FMath::Max3(v0.X, v1.X, v2.X);
				AABB.upper_y = FMath::Max3(v0.Y, v1.Y, v2.Y);
				AABB.upper_z = FMath::Max3(v0.Z, v1.Z, v2.Z);
				AABB.geomID = 0;
				AABB.primID = TriIndex;
			}

			RTCDevice EmbreeDevice = rtcNewDevice(nullptr);

			RTCBVH EmbreeBvh = rtcNewBVH(EmbreeDevice);
			RTCBuildArguments EmbreeBvhBuildArgs = rtcDefaultBuildArguments();
			EmbreeBvhBuildArgs.maxBranchingFactor = FEmbreeBVH8Node::MaxNumChildren;
			EmbreeBvhBuildArgs.maxLeafSize = FEmbreeBVH8LeafNode::MaxNumPrimitives;
			EmbreeBvhBuildArgs.buildQuality = RTC_BUILD_QUALITY_HIGH;
			EmbreeBvhBuildArgs.bvh = EmbreeBvh;
			EmbreeBvhBuildArgs.primitives = AABBs.GetData();
			EmbreeBvhBuildArgs.primitiveCount = Cluster.NumTris;
			EmbreeBvhBuildArgs.primitiveArrayCapacity = Cluster.NumTris * FEmbreeBVH8Node::MaxNumChildren;
			EmbreeBvhBuildArgs.createNode = FEmbreeBVH8Node::Create;
			EmbreeBvhBuildArgs.setNodeChildren = FEmbreeBVH8Node::SetNodeChildren;
			EmbreeBvhBuildArgs.setNodeBounds = FEmbreeBVH8Node::SetNodeBounds;
			EmbreeBvhBuildArgs.createLeaf = FEmbreeBVH8LeafNode::Create;
#if 0
			EmbreeBvhBuildArgs.splitPrimitive = SplitPrimitive;
#endif

			FEmbreeBVH8Node* EmbreeRootNode = (FEmbreeBVH8Node*)rtcBuildBVH(&EmbreeBvhBuildArgs);			

			TArray<FCompressedBVHNodeData> OutputData;
			TArray<uint8> OutputIndices;
			ConvertToCompressedWideBVH(Cluster, EmbreeRootNode, OutputData, OutputIndices);

			rtcReleaseBVH(EmbreeBvh);
			rtcReleaseDevice(EmbreeDevice);

// Disabled until it's hooked up properly
#if 0
			{
				FScopeLock Lock(&AddDataCS);

				uint32 RayTracingIndex = Resources.RayTracingNodes.Num();				

				Cluster.RayTracingIndex = RayTracingIndex;
				Resources.RayTracingNodes.AddDefaulted(OutputData.Num());

				uint32 RayTracingNodesSizeInBytes = OutputData.Num() * sizeof(OutputData[0]);
				FMemory::Memcpy(&Resources.RayTracingNodes[RayTracingIndex], OutputData.GetData(), RayTracingNodesSizeInBytes);

				// Copy indices right after the cluster node data and make sure it's in multiples of NANITE_RAY_TRACING_NODE_DATA_SIZE_IN_BYTES
				// so we can still read the node data using Load4. We will waste a bit of memory but as compressed node gets smaller it should be less of a problem.
				uint32 IndicesSizeInBytes = sizeof(OutputIndices[0]) * OutputIndices.Num();
				uint32 IndicesSizeAligned = FMath::DivideAndRoundUp(IndicesSizeInBytes, (uint32)NANITE_RAY_TRACING_NODE_DATA_SIZE_IN_BYTES);
				Resources.RayTracingNodes.AddZeroed(IndicesSizeAligned);

				uint32 CopyDstIndex = RayTracingIndex + OutputData.Num();
				FMemory::Memcpy(&Resources.RayTracingNodes[CopyDstIndex], OutputIndices.GetData(), IndicesSizeInBytes);
			}
#endif
			
		}
	);

#if 0
	{
		int32 NumRayTracingNodes = Resources.RayTracingNodes.Num();
		UE_LOG(LogStaticMesh, Log, TEXT("NumRayTracingNodes %d taking %d bytes"), NumRayTracingNodes, NumRayTracingNodes * NANITE_RAY_TRACING_NODE_DATA_SIZE_IN_BYTES);
	}
#endif
}

} // namespace Nanite

