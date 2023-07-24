// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"

struct FManagedArrayCollection;

namespace GeometryCollection::Facades
{
	/**
	 * this class helps reading and writing the packed data used in the managed array
	 */
	struct CHAOS_API FRemoveOnBreakData
	{
	public:
		inline static const FVector4f DisabledPackedData{ -1, 0, 0, 0 };

		FRemoveOnBreakData();
		FRemoveOnBreakData(const FVector4f& InPackedData);
		FRemoveOnBreakData(bool bEnable, const FVector2f& BreakTimer, bool bClusterCrumbling, const FVector2f& RemovalTimer);

		const FVector4f& GetPackedData() const;

		bool IsEnabled() const;
		bool GetClusterCrumbling() const;
		FVector2f GetBreakTimer() const;
		FVector2f GetRemovalTimer() const;
		
		void SetEnabled(bool bEnable);
		void SetClusterCrumbling(bool bClusterCrumbling);
		void SetBreakTimer(float MinTime, float MaxTime);
		void SetRemovalTimer(float MinTime, float MaxTime);

	private:
		FVector4f PackedData;
	};

	/**
	 * Provide an API to set and get removal on break data on a collection
	 */
	class CHAOS_API FCollectionRemoveOnBreakFacade
	{
	public:
		FCollectionRemoveOnBreakFacade(FManagedArrayCollection& InCollection);
		FCollectionRemoveOnBreakFacade(const FManagedArrayCollection& InCollection);

		/** Create the facade attributes. */
		void DefineSchema();

		/** remove the facade attributes */
		void RemoveSchema();

		/** Valid if related attributes are available */
		bool IsValid() const;

		/** Is this facade const access */
		bool IsConst() const;

		/** set a specific value to a array of transforms */
		void SetFromIndexArray(const TArray<int32>& Indices, const FRemoveOnBreakData& Data);

		/** set a specific value to all transforms */
		void SetToAll(const FRemoveOnBreakData& Data);

		/** Get a specific value by index */
		const FRemoveOnBreakData GetData(int32 Index) const;

	private:
		TManagedArrayAccessor<FVector4f> RemoveOnBreakAttribute;
	};
}