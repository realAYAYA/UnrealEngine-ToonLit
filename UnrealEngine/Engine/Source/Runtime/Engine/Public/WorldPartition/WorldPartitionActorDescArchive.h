// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

class FWorldPartitionActorDesc;

class FActorDescArchive : public FArchiveProxy
{
public:
	FActorDescArchive(FArchive& InArchive, FWorldPartitionActorDesc* InActorDesc);

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(FText& Value) override { unimplemented(); return *this; }
	virtual FArchive& operator<<(UObject*& Value) override { unimplemented(); return *this; }
	virtual FArchive& operator<<(FLazyObjectPtr& Value) override { unimplemented(); return *this; }
	virtual FArchive& operator<<(FObjectPtr& Value) override { unimplemented(); return *this; }
	virtual FArchive& operator<<(FSoftObjectPtr& Value) override { unimplemented(); return *this; }
	virtual FArchive& operator<<(FSoftObjectPath& Value) override;
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override { unimplemented(); return *this; }
	//~ End FArchive Interface

	template <typename DestPropertyType, typename SourcePropertyType>
	struct TDeltaSerializer
	{
		using FDeprecateFunction = TFunction<void(DestPropertyType&, const SourcePropertyType&)>;

		explicit TDeltaSerializer(DestPropertyType& InValue)
			: Value(InValue)
		{}

		explicit TDeltaSerializer(DestPropertyType& InValue, FDeprecateFunction InFunc)
			: Value(InValue)
			, Func(InFunc)
		{}

		friend FArchive& operator<<(FArchive& Ar, const TDeltaSerializer<DestPropertyType, SourcePropertyType>& V)
		{
			FActorDescArchive& ActorDescAr = (FActorDescArchive&)Ar;
			
			check(ActorDescAr.ClassDesc || Ar.IsSaving());
			check(ActorDescAr.ActorDesc);

			Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

			auto GetClassDefaultValue = [&V, &ActorDescAr]() -> const DestPropertyType*
			{
				const UPTRINT PropertyOffset = (UPTRINT)&V.Value - *(UPTRINT*)&ActorDescAr.ActorDesc;

				if (PropertyOffset < ActorDescAr.ClassDescSizeof)
				{
					check((PropertyOffset + sizeof(V)) <= ActorDescAr.ClassDescSizeof);
					const DestPropertyType* RefValue = (const DestPropertyType*)(*(UPTRINT*)&ActorDescAr.ClassDesc + PropertyOffset);
					return RefValue;
				}

				check(ActorDescAr.bIsMissingClassDesc);
				return nullptr;
			};

			uint8 bSerialize = 1;

			if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionActorClassDescSerialize)
			{
				if (Ar.IsSaving() && ActorDescAr.ClassDesc)
				{
					if constexpr (std::is_same_v<DestPropertyType, SourcePropertyType>)
					{
						// When saving, we expect the class descriptor to be the exact type as what we are serializing.
						const DestPropertyType* ClassDefaultValue = GetClassDefaultValue();
						check(ClassDefaultValue);

						bSerialize = (V.Value != *ClassDefaultValue) ? 1 : 0;
					}
				}

				Ar << bSerialize;
			}

			if (bSerialize)
			{
				if constexpr (std::is_same_v<DestPropertyType, SourcePropertyType>)
				{
					Ar << V.Value;
				}
				else
				{
					check(Ar.IsLoading());

					SourcePropertyType SourceValue;
					Ar << SourceValue;
					V.Func(V.Value, SourceValue);
				}
			}
			else if (Ar.IsLoading())
			{
				// When loading, we need to handle a different class descriptor in case of missing classes, etc.
				if (const DestPropertyType* ClassDefaultValue = GetClassDefaultValue())
				{
					V.Value = *ClassDefaultValue;
				}
			}

			return Ar;
		}

		DestPropertyType& Value;
		FDeprecateFunction Func;
	};

	FWorldPartitionActorDesc* ActorDesc;
	const FWorldPartitionActorDesc* ClassDesc;
	uint32 ClassDescSizeof;
	bool bIsMissingClassDesc;
};

template <typename DestPropertyType, typename SourcePropertyType = DestPropertyType>
using TDeltaSerialize = FActorDescArchive::TDeltaSerializer<DestPropertyType, SourcePropertyType>;
#endif