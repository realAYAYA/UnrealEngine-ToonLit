// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "DirectLinkCommon.h"
#include "DirectLinkParameterStore.h"
#include "UObject/NameTypes.h"

class FArchive;

namespace DirectLink
{
class IReferenceResolutionProvider;
class ISceneGraphNode;


enum class ESerializationStatus
{
	Ok,
	StreamError,
	VersionMinNotRespected,
	VersionMaxNotRespected,
};


struct DIRECTLINK_API FReferenceSnapshot
{
	void Serialize(FArchive& Ar);
	FElementHash Hash() const;

	struct FReferenceGroup
	{
		FName Name;
		TArray<FSceneGraphId> ReferencedIds;
	};

	TArray<FReferenceGroup> Groups;
};



class DIRECTLINK_API FElementSnapshot
{
public:
	FElementSnapshot() = default;
	FElementSnapshot(const ISceneGraphNode& Node);

	friend FArchive& operator<<(FArchive& Ar, FElementSnapshot& This);

	ESerializationStatus Serialize(FArchive& Ar);

	FElementHash GetHash() const;
	FElementHash GetDataHash() const; // #ue_directlink_sync: serialize hashs
	FElementHash GetRefHash() const;

	void UpdateNodeReferences(IReferenceResolutionProvider& Resolver, ISceneGraphNode& Node) const;
	void UpdateNodeData(ISceneGraphNode& Node) const;

	FSceneGraphId GetNodeId() const { return NodeId; }

	template<typename T>
	bool GetValueAs(FName Name, T& Out) const
	{
		return DataSnapshot.GetValueAs(Name, Out);
	}

private:
	FSceneGraphId NodeId;
	mutable FElementHash DataHash = InvalidHash;
	mutable FElementHash RefHash = InvalidHash;
	FParameterStoreSnapshot DataSnapshot;
	FReferenceSnapshot RefSnapshot;
};

} // namespace DirectLink
