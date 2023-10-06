// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/BoundingVolumeHierarchy.h"
#include "Chaos/ImplicitFwd.h"
#include "Chaos/Serializable.h"

namespace Chaos
{
	class FChaosArchive;

	namespace Private
	{
		class FImplicitBVH;
		class FImplicitBVTree;
		class FImplicitBVTreeNode;

		// A item in a ImplicitBVH holding the leaf geometry and transform. Each FImplicitBVHNode node holds a set of these.
		class FImplicitBVHObject
		{
		public:
			friend class FImplicitBVH;

			friend FChaosArchive& operator<<(FChaosArchive& Ar, FImplicitBVHObject& BVHObjecty);

			FImplicitBVHObject();
			FImplicitBVHObject(const TSerializablePtr<FImplicitObject>& InGeometry, const FVec3& InX, const FRotation3& InR, const FAABB3& InBounds, const int32 InRootObjectIndex, const int32 InObjectIndex);

			const FImplicitObject* GetGeometry() const { return Geometry.Get(); }

			const FVec3f& GetX() const { return X; }

			const FRotation3f& GetR() const { return R; }

			const FAABB3f& GetBounds() const { return Bounds; }

			const FVec3f GetBoundsCenter() const { return Bounds.Center(); }

			FRigidTransform3f GetTransformf() const { return FRigidTransform3f(GetX(), GetR()); }

			FRigidTransform3 GetTransform() const { return FRigidTransform3(FVec3(GetX()), FRotation3(GetR())); }

			// A unique index for this object in the hierarchy. E.g., if the same FImplicitObject is referenced 
			// multiple times in the hierarchy in a union of transformed objects, each will have a different ObjectIndex 
			// (it is the index into the pre-order depth first traversal).
			int32 GetObjectIndex() const { return ObjectIndex; }

			// The index of our most distant ancestor. I.e., the index in the root Union. This is used to map
			// each object to a ShapeInstance.
			int32 GetRootObjectIndex() const { return RootObjectIndex; }

		private:
			FChaosArchive& Serialize(FChaosArchive& Ar);

			// Transform and bounds in the space of the BVH owner (Union Implicit)
			FRotation3f R;
			FVec3f X;
			FAABB3f Bounds;

			// The leaf geometry stripped of decorators (but not Instanced or Scaled)
			TSerializablePtr<FImplicitObject> Geometry;

			// The index of our ancestor in the array of RootObjects that was provided when creating the BVH
			int32 RootObjectIndex;

			// Our index in the hierarchy. This could be used to uniquely identity copies of the same implicit in the hierarchy
			int32 ObjectIndex;
		};

		// A node in an FImplicitBVH
		class FImplicitBVHNode
		{
		public:
			FImplicitBVHNode(const FAABB3f& InBounds, const int32 InObjectBeginIndex, const int32 InObjectEndIndex)
				: Bounds(InBounds)
				, ObjectBeginIndex(InObjectBeginIndex)
				, ObjectEndIndex(InObjectEndIndex)
				, ChildNodeIndices{ INDEX_NONE, INDEX_NONE }
			{
			}

			bool IsLeaf() const
			{
				// NOTE: We either have zero or two children
				return ChildNodeIndices[0] == INDEX_NONE;
			}

		private:
			friend class FImplicitBVH;

			FAABB3f Bounds;
			int32 ObjectBeginIndex;
			int32 ObjectEndIndex;
			int32 ChildNodeIndices[2];
		};


		// A Bounding Volume Hierarchy of a set of Implicit Objects
		class FImplicitBVH
		{
		public:
			using FObjects = TArray<FImplicitBVHObject>;

			friend FChaosArchive& operator<<(FChaosArchive& Ar, FImplicitBVH& BVH);

			// Make an empty BVH (for serialization only)
			static TUniquePtr<FImplicitBVH> MakeEmpty();

			// Utility for processing the hierarchy
			static int32 CountLeafObjects(const TArrayView<const TUniquePtr<FImplicitObject>>& InRootObjects);
			static FObjects CollectLeafObjects(const TArrayView<const TUniquePtr<FImplicitObject>>& InRootObjects);

			// Create a BVH around a set of ImplicitObjects. Usually these are the immediate child elements of an FImplcitObjectUnion
			// TryMake will then recurse into the geometry hierachy and add all descendents to the BVH. Will return null if the 
			// number of descendents is less that MinObjscts.
			static TUniquePtr<FImplicitBVH> TryMake(const TArrayView<const TUniquePtr<FImplicitObject>>& InRootObjects, const int32 MinObjects, const int32 InMaxBVHDepth);

			~FImplicitBVH();

			int32 GetNumObjects() const { return Objects.Num(); }
			int32 GetDepth() const { return TreeDepth; }

			const FImplicitBVHObject& GetObject(const int32 ObjectIndex) const { return Objects[ObjectIndex]; }

			const FImplicitObject* GetGeometry(const int32 ObjectIndex) const { return Objects[ObjectIndex].GetGeometry(); }

			const FVec3f& GetX(const int32 ObjectIndex) const { return Objects[ObjectIndex].GetX(); }

			const FRotation3f& GetR(const int32 ObjectIndex) const { return Objects[ObjectIndex].GetR(); }

			const FAABB3f& GetBounds(const int32 ObjectIndex) const { return Objects[ObjectIndex].GetBounds(); }

			FRigidTransform3f GetTransformf(const int32 ObjectIndex) const { return Objects[ObjectIndex].GetTransformf(); }

			FRigidTransform3 GetTransform(const int32 ObjectIndex) const { return Objects[ObjectIndex].GetTransform(); }

			int32 GetRootObjectIndex(const int32 ObjectIndex) const { return Objects[ObjectIndex].GetRootObjectIndex(); }
			int32 GetObjectIndex(const int32 ObjectIndex) const { return Objects[ObjectIndex].GetObjectIndex(); }

			// Visit all the leaf objects that overlap the specified bounds.
			// @param ObjectVisitor [](const FImplicitObject* Implicit, const FRigidTransform3f& RelativeTransformf, const FAABB3f& RelativeBoundsf, const int32 RootObjectIndex, const int32 LeafObjectIndex) -> void {}
			template<typename TVisitor>
			void VisitAllIntersections(const FAABB3& LocalBounds, const TVisitor& ObjectVisitor) const
			{
				const auto& NodeVisitor = [this, &ObjectVisitor](const FImplicitBVHNode& Node)
				{
					if (Node.IsLeaf())
					{
						VisitNodeObjects(Node, ObjectVisitor);
					}
				};

				VisitOverlappingNodes(LocalBounds, NodeVisitor);
			}

			// Calls the visitor for every overlapping leaf.
			// @tparam TVisitor void(const FImplicitBVHNode& Node)
			template<typename TVisitor>
			void VisitOverlappingNodes(const FAABB3& LocalBounds, const TVisitor& NodeVisitor) const
			{
				if (!Nodes.IsEmpty())
				{
					VisitOverlappingNodesRecursive(Nodes[0], FAABB3f(LocalBounds), NodeVisitor);
				}
			}

			// Recursively visit all nodes in the hierarchy. Will stop visiting children
			// if the visitor returns false. Leaf will be null when visiting an internal node.
			// @param NodeVisitor (const FAABB3f& NodeBounds, const int32 NodeDepth, const FImplicitBVHNode& Node) -> void
			template<typename TVisitor>
			void VisitHierarchy(const TVisitor& NodeVisitor) const
			{
				if (!Nodes.IsEmpty())
				{
					VisitHierarchyRecursive(Nodes[0], 0, NodeVisitor);
				}
			}

			// Visit all the items in the specified leaf node (which is probably obtained from VisitHierarchy)
			// @param ObjectVisitor (const FImplicitObject* Implicit, const FRigidTransform3f& RelativeTransformf, const FAABB3f& RelativeBoundsf, const int32 RootObjectIndex, const int32 LeafObjectIndex) -> void
			template<typename TVisitor>
			void VisitNodeObjects(const FImplicitBVHNode& Node, const TVisitor& ObjectVisitor) const
			{
				for (int32 NodeObjectIndex = Node.ObjectBeginIndex; NodeObjectIndex < Node.ObjectEndIndex; ++NodeObjectIndex)
				{
					const FImplicitBVHObject& Object = Objects[NodeObjectIndices[NodeObjectIndex]];

					ObjectVisitor(Object.GetGeometry(), Object.GetTransformf(), Object.GetBounds(), Object.GetRootObjectIndex(), Object.GetObjectIndex());
				}
			}

			// Does the bounding box overlap any leaf nodes with items in it
			bool IsOverlappingBounds(const FAABB3& LocalBounds) const
			{
				if (!Nodes.IsEmpty())
				{
					return IsOverlappingBoundsRecursive(Nodes[0], FAABB3f(LocalBounds));
				}
				return false;
			}

		private:
			FImplicitBVH();
			FImplicitBVH(FObjects&& InObjects, const int32 InMaxBVHDepth);
			FChaosArchive& Serialize(FChaosArchive& Ar);

			// Initialize the BVH from the specified set of children.
			// NOTE: InChildren should be immediate children of the BVH owner, not further-removed descendents
			void Init(FObjects&& InObjects, const int32 MaxDepth, const int32 MaxLeafObjects);

			template<typename TVisitor>
			void VisitOverlappingNodesRecursive(const FImplicitBVHNode& Node, const FAABB3f& LocalBounds, const TVisitor& NodeVisitor) const
			{
				if (!Node.Bounds.Intersects(LocalBounds))
				{
					return;
				}

				if (Node.IsLeaf())
				{
					NodeVisitor(Node);
				}
				else
				{
					VisitOverlappingNodesRecursive(Nodes[Node.ChildNodeIndices[0]], LocalBounds, NodeVisitor);
					VisitOverlappingNodesRecursive(Nodes[Node.ChildNodeIndices[1]], LocalBounds, NodeVisitor);
				}
			}

			template<typename TVisitor>
			void VisitHierarchyRecursive(const FImplicitBVHNode& Node, const int32 NodeDepth, const TVisitor& NodeVisitor) const
			{
				// Visit this (non-leaf) node
				const bool bVisitChildren = NodeVisitor(Node.Bounds, NodeDepth, Node);

				// Visit children
				if (bVisitChildren && !Node.IsLeaf())
				{
					VisitHierarchyRecursive(Nodes[Node.ChildNodeIndices[0]], NodeDepth + 1, NodeVisitor);
					VisitHierarchyRecursive(Nodes[Node.ChildNodeIndices[1]], NodeDepth + 1, NodeVisitor);
				}
			}

			bool IsOverlappingBoundsRecursive(const FImplicitBVHNode& Node, const FAABB3f& LocalBounds) const
			{
				if (Node.Bounds.Intersects(LocalBounds))
				{
					// If we hit a leaf, we have an overlap
					if (Node.IsLeaf())
					{
						return true;
					}

					// Check children, stopping if we get an overlap
					if (IsOverlappingBoundsRecursive(Nodes[Node.ChildNodeIndices[0]], LocalBounds))
					{
						return true;
					}
					if (IsOverlappingBoundsRecursive(Nodes[Node.ChildNodeIndices[1]], LocalBounds))
					{
						return true;
					}
				}
				return false;
			}

			struct FWorkData
			{
				TArray<FVec3f> ObjectCenters;
				TArray<int32> NodeObjectIndices0;
				TArray<int32> NodeObjectIndices1;
			};

			void BuildTree(const int32 MaxDepth, const int32 MaxLeafObjects);
			int32 AddNodeRecursive(const FAABB3f& NodeBounds, const int32 ObjectBeginIndex, const int32 ObjectEndIndex, const int32 NodeDepth, const int32 MaxDepth, const int32 MaxLeafObjects, FWorkData& WorkData);
			bool Partition(const FAABB3f& NodeBounds, const int32 ObjectBeginIndex, const int32 ObjectEndIndex, FAABB3f& OutChildNodeBounds0, FAABB3f& OutChildNodeBounds1, int32& OutPartitionIndex, FWorkData& WorkData);

			// A BVH leaf holds an array of indices into the Objects array
			FObjects Objects;
			TArray<int32> NodeObjectIndices;
			TArray<FImplicitBVHNode> Nodes;
			int32 TreeDepth;
		};
	}

	// DO NOT USE: This is only required for serializing the old BVH data
	template<> inline bool HasBoundingBox(const TArray<Private::FImplicitBVHObject>& Objects, const int32 ObjectIndex) { return Objects[ObjectIndex].GetGeometry()->HasBoundingBox(); }
	template<class T, int d> inline const TAABB<T, d> GetWorldSpaceBoundingBox(const TArray<Private::FImplicitBVHObject>& Objects, const int32 ObjectIndex, const TMap<int32, TAABB<T, d>>& WorldSpaceBoxes) { return TAABB<T, d>(Objects[ObjectIndex].GetBounds()); }
	template<class T, int d> void ComputeAllWorldSpaceBoundingBoxes(const TArray<Private::FImplicitBVHObject>& Objects, const TArray<int32>& AllObjects, const bool bUseVelocity, const T Dt, TMap<int32, TAABB<T, d>>& WorldSpaceBoxes) { }
}