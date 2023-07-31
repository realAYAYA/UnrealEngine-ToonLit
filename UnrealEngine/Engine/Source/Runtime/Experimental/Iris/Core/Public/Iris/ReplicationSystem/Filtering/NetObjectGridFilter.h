// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Net/Core/NetBitArray.h"
#include "Containers/ChunkedArray.h"
#include "Math/Vector.h"
#include "UObject/StrongObjectPtr.h"
#include "NetObjectGridFilter.generated.h"

namespace UE::Net
{
	class FWorldLocations;
}

UCLASS(transient, config=Engine, MinimalAPI)
class UNetObjectGridFilterConfig : public UNetObjectFilterConfig
{
	GENERATED_BODY()

public:
	/** How many frames a view position should be considered relevant. To avoid culling issues when player borders cells. */
	UPROPERTY(Config)
	uint32 ViewPosRelevancyFrameCount = 2;

	UPROPERTY(Config)
	float CellSizeX = 20000.0f;

	UPROPERTY(Config)
	float CellSizeY = 20000.0f;

	/** Objects with larger sqrt(NetCullDistanceSqr) will be rejected. */
	UPROPERTY(Config)
	float MaxCullDistance = 20000.0f;

	/** Objects without a NetCullDistanceSquared property will assume to have this value but squared. */
	UPROPERTY(Config)
	float DefaultCullDistance = 15000.0f;

	/** Coordinates will be clamped to MinPos and MaxPos. */
	UPROPERTY(Config)
	FVector MinPos = {-0.5f*2097152.0f, -0.5f*2097152.0f, -0.5f*2097152.0f};

	/** Coordinates will be clamped to MinPos and MaxPos. */
	UPROPERTY(Config)
	FVector MaxPos = {+0.5f*2097152.0f, +0.5f*2097152.0f, +0.5f*2097152.0f};
};

UCLASS()
class UNetObjectGridFilter : public UNetObjectFilter
{
	GENERATED_BODY()

protected:
	// UNetObjectFilter interface
	IRISCORE_API virtual void Init(FNetObjectFilterInitParams&) override;
	IRISCORE_API virtual void AddConnection(uint32 ConnectionId) override;
	IRISCORE_API virtual void RemoveConnection(uint32 ConnectionId) override;
	IRISCORE_API virtual bool AddObject(uint32 ObjectIndex, FNetObjectFilterAddObjectParams&) override;
	IRISCORE_API virtual void RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo&) override;
	IRISCORE_API virtual void UpdateObjects(FNetObjectFilterUpdateParams&) override;
	IRISCORE_API virtual void PreFilter(FNetObjectPreFilteringParams&) override;
	IRISCORE_API virtual void Filter(FNetObjectFilteringParams&) override;

protected:
	struct FObjectLocationInfo : public FNetObjectFilteringInfo
	{
		bool IsUsingWorldLocations() const { return GetLocationStateIndex() == InvalidStateIndex; }
		bool IsUsingLocationInState() const { return GetLocationStateIndex() != InvalidStateIndex; }

		void SetLocationStateOffset(uint16 Offset) { Data[0] = Offset; }
		uint16 GetLocationStateOffset() const { return Data[0]; }

		void SetLocationStateIndex(uint16 Index) { Data[1] = Index; }
		uint16 GetLocationStateIndex() const { return Data[1]; }

		void SetInfoIndex(uint32 Index) { Data[2] = Index & 65535U; Data[3] = Index >> 16U; }
		uint32 GetInfoIndex() const { return (uint32(Data[3]) << 16U) | uint32(Data[2]); }
	};

	IRISCORE_API void AddCellInfoForObject(const FObjectLocationInfo& ObjectInfo, const UE::Net::FReplicationInstanceProtocol* InstanceProtocol);
	IRISCORE_API void RemoveCellInfoForObject(const FObjectLocationInfo& ObjectInfo);
	IRISCORE_API void UpdateCellInfoForObject(const FObjectLocationInfo& ObjectInfo, const UE::Net::FReplicationInstanceProtocol* InstanceProtocol);

private:
	enum : unsigned
	{
		ObjectInfosChunkSize = 64 * 1024,
		InvalidStateIndex = 65535U,
		InvalidStateOffset = 65535U,
	};

	struct FCellBox
	{
		int32 MinX;
		int32 MaxX;
		int32 MinY;
		int32 MaxY;

		bool operator==(const FCellBox&) const;
		bool operator!=(const FCellBox&) const;
	};

	// We can't fit all info we need in 4x16bits.
	struct FPerObjectInfo
	{
		FVector Position = {0,0,0};
		FCellBox CellBox = {};
		float CullDistance = 0;
		uint32 ObjectIndex = 0;
		uint16 CullDistanceSqrStateIndex = InvalidStateIndex;
		uint16 CullDistanceSqrStateOffset = InvalidStateOffset;
	};

	struct FCellCoord
	{
		int32 X;
		int32 Y;

	private:
		friend bool operator==(const FCellCoord& A, const FCellCoord& B)
		{
			return (A.X == B.X) & (A.Y == B.Y);
		}

		friend uint32 GetTypeHash(const FCellCoord& Coords)
		{
			return ::GetTypeHash((uint64(uint32(Coords.X)) << 32U) | uint64(uint32(Coords.Y)));
		}
	};

	struct FCellObjects
	{
		TSet<uint32> ObjectIndices;
	};

	struct FCellAndTimestamp
	{
		FCellCoord Cell;
		uint32 Timestamp;

	private:
		friend bool operator==(const FCellAndTimestamp& A, const FCellAndTimestamp& B)
		{
			return (A.Cell == B.Cell) & (A.Timestamp == B.Timestamp);
		}
	};

	struct FPerConnectionInfo
	{
		// We don't expect a lot of view positions from a single connection
		TArray<FCellAndTimestamp, TInlineAllocator<32>> RecentCells;
	};

private:
	uint32 AllocObjectInfo();
	void FreeObjectInfo(uint32 Index);

	void UpdatePositionAndCullDistance(const FObjectLocationInfo& ObjectLocationInfo, FPerObjectInfo& PerObjectInfo, const UE::Net::FReplicationInstanceProtocol* InstanceProtocol);
	void CalculateCellBox(const FPerObjectInfo& PerObjectInfo, FCellBox& OutCellBox);
	void CalculateCellCoord(FCellCoord& OutCoord, const FVector& Pos);

	static bool AreCellsDisjoint(const FCellBox& A, const FCellBox& B);
	static bool DoesCellContainCoord(const FCellBox& Cell, const FCellCoord& Coord);

	TStrongObjectPtr<UNetObjectGridFilterConfig> Config;
	TArray<FPerConnectionInfo> PerConnectionInfos;
	TChunkedArray<FPerObjectInfo, ObjectInfosChunkSize> ObjectInfos;
	UE::Net::FNetBitArray AssignedObjectInfoIndices;

	TMap<FCellCoord, FCellObjects> Cells;
	uint32 FrameIndex = 0;

	const UE::Net::FWorldLocations* WorldLocations = nullptr;
};


//
inline bool UNetObjectGridFilter::FCellBox::operator==(const UNetObjectGridFilter::FCellBox& Other) const
{
	return (MinX == Other.MinX) & (MinY == Other.MinY) & (MaxX == Other.MaxX) & (MaxY == Other.MaxY);
}

inline bool UNetObjectGridFilter::FCellBox::operator!=(const UNetObjectGridFilter::FCellBox& Other) const
{
	return (MinX != Other.MinX) | (MinY != Other.MinY) | (MaxX != Other.MaxX) | (MaxY != Other.MaxY);
}
