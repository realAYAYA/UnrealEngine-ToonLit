// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "RigInfluenceMap.generated.h"

USTRUCT(BlueprintType)
struct FRigInfluenceEntry
{
	GENERATED_BODY()

	FRigInfluenceEntry()
		: Source()
		, AffectedList()
	{
	}

	FORCEINLINE const FRigElementKey& GetSource() const { return Source; }

	FORCEINLINE int32 Num() const { return AffectedList.Num(); }
	FORCEINLINE const FRigElementKey& operator[](int32 InIndex) const { return AffectedList[InIndex]; }
	FORCEINLINE FRigElementKey& operator[](int32 InIndex) { return AffectedList[InIndex]; }

	FORCEINLINE TArray<FRigElementKey>::RangedForIteratorType      begin() { return AffectedList.begin(); }
	FORCEINLINE TArray<FRigElementKey>::RangedForConstIteratorType begin() const { return AffectedList.begin(); }
	FORCEINLINE TArray<FRigElementKey>::RangedForIteratorType      end() { return AffectedList.end(); }
	FORCEINLINE TArray<FRigElementKey>::RangedForConstIteratorType end() const { return AffectedList.end(); }

	FORCEINLINE int32 AddUnique(const FRigElementKey& InKey)
	{
		return AffectedList.AddUnique(InKey);
	}

	FORCEINLINE void Remove(const FRigElementKey& InKey)
	{
		AffectedList.Remove(InKey);
	}

	FORCEINLINE bool Contains(const FRigElementKey& InKey) const
	{
		return AffectedList.Contains(InKey);
	}

	FORCEINLINE bool Merge(const FRigInfluenceEntry& Other)
	{
		if(Other.Source != Source)
		{
			return false;
		}

		for(const FRigElementKey& OtherAffected : Other)
		{
			AddUnique(OtherAffected);
		}

		return true;
	}

	void OnKeyRemoved(const FRigElementKey& InKey);

	void OnKeyRenamed(const FRigElementKey& InOldKey, const FRigElementKey& InNewKey);

protected:

	UPROPERTY()
	FRigElementKey Source;

	UPROPERTY()
	TArray<FRigElementKey> AffectedList;

	friend struct FRigInfluenceMap;
};

USTRUCT()
struct FRigInfluenceEntryModifier
{
	GENERATED_BODY()

	FRigInfluenceEntryModifier()
		: AffectedList()
	{
	}

	UPROPERTY(EditAnywhere, Category = "Inversion")
	TArray<FRigElementKey> AffectedList;

	FORCEINLINE int32 Num() const { return AffectedList.Num(); }
	FORCEINLINE const FRigElementKey& operator[](int32 InIndex) const { return AffectedList[InIndex]; }
	FORCEINLINE FRigElementKey& operator[](int32 InIndex) { return AffectedList[InIndex]; }

	FORCEINLINE TArray<FRigElementKey>::RangedForIteratorType      begin() { return AffectedList.begin(); }
	FORCEINLINE TArray<FRigElementKey>::RangedForConstIteratorType begin() const { return AffectedList.begin(); }
	FORCEINLINE TArray<FRigElementKey>::RangedForIteratorType      end() { return AffectedList.end(); }
	FORCEINLINE TArray<FRigElementKey>::RangedForConstIteratorType end() const { return AffectedList.end(); }
};


USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigInfluenceMap
{
	GENERATED_BODY()

public:

	FRigInfluenceMap()
	:EventName(NAME_None)
	{}

	const FName& GetEventName() const { return EventName; }

	FORCEINLINE int32 Num() const { return Entries.Num(); }
	FORCEINLINE const FRigInfluenceEntry& operator[](int32 InIndex) const { return Entries[InIndex]; }
	FORCEINLINE FRigInfluenceEntry& operator[](int32 InIndex) { return Entries[InIndex]; }
	FORCEINLINE const FRigInfluenceEntry& operator[](const FRigElementKey& InKey) const { return Entries[GetIndex(InKey)]; }
	FORCEINLINE FRigInfluenceEntry& operator[](const FRigElementKey& InKey) { return FindOrAdd(InKey); }

	FORCEINLINE TArray<FRigInfluenceEntry>::RangedForIteratorType      begin()       { return Entries.begin(); }
	FORCEINLINE TArray<FRigInfluenceEntry>::RangedForConstIteratorType begin() const { return Entries.begin(); }
	FORCEINLINE TArray<FRigInfluenceEntry>::RangedForIteratorType      end()         { return Entries.end();   }
	FORCEINLINE TArray<FRigInfluenceEntry>::RangedForConstIteratorType end() const   { return Entries.end();   }

	FRigInfluenceEntry& FindOrAdd(const FRigElementKey& InKey);

	const FRigInfluenceEntry* Find(const FRigElementKey& InKey) const;

	void Remove(const FRigElementKey& InKey);

	FORCEINLINE bool Contains(const FRigElementKey& InKey) const
	{
		return GetIndex(InKey) != INDEX_NONE;
	}

	FORCEINLINE int32 GetIndex(const FRigElementKey& InKey) const
	{
		const int32* Index = KeyToIndex.Find(InKey);
		if (Index)
		{
			return *Index;
		}
		return INDEX_NONE;
	}

	FRigInfluenceMap Inverse() const;

	bool Merge(const FRigInfluenceMap& Other);

	FRigInfluenceEntryModifier GetEntryModifier(const FRigElementKey& InKey) const;

	void SetEntryModifier(const FRigElementKey& InKey, const FRigInfluenceEntryModifier& InModifier);

	void OnKeyRemoved(const FRigElementKey& InKey);

	void OnKeyRenamed(const FRigElementKey& InOldKey, const FRigElementKey& InNewKey);

protected:

	bool Merge(const FRigInfluenceMap& Other, bool bIgnoreEventName);

	UPROPERTY()
	FName EventName;

	UPROPERTY()
	TArray<FRigInfluenceEntry> Entries;

	UPROPERTY()
	TMap<FRigElementKey, int32> KeyToIndex;

	friend struct FRigInfluenceMapPerEvent;
	friend class UControlRig;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigInfluenceMapPerEvent
{
	GENERATED_BODY()

public:

	FORCEINLINE int32 Num() const { return Maps.Num(); }
	FORCEINLINE const FRigInfluenceMap& operator[](int32 InIndex) const { return Maps[InIndex]; }
	FORCEINLINE FRigInfluenceMap& operator[](int32 InIndex) { return Maps[InIndex]; }
	FORCEINLINE const FRigInfluenceMap& operator[](const FName& InEventName) const { return Maps[GetIndex(InEventName)]; }
	FORCEINLINE FRigInfluenceMap& operator[](const FName& InEventName) { return FindOrAdd(InEventName); }

	FORCEINLINE TArray<FRigInfluenceMap>::RangedForIteratorType      begin()       { return Maps.begin(); }
	FORCEINLINE TArray<FRigInfluenceMap>::RangedForConstIteratorType begin() const { return Maps.begin(); }
	FORCEINLINE TArray<FRigInfluenceMap>::RangedForIteratorType      end()         { return Maps.end();   }
	FORCEINLINE TArray<FRigInfluenceMap>::RangedForConstIteratorType end() const   { return Maps.end();   }

	FRigInfluenceMap& FindOrAdd(const FName& InEventName);

	const FRigInfluenceMap* Find(const FName& InEventName) const;

	void Remove(const FName& InEventName);

	FORCEINLINE bool Contains(const FName& InEventName) const
	{
		return GetIndex(InEventName) != INDEX_NONE;
	}

	FORCEINLINE int32 GetIndex(const FName& InEventName) const
	{
		const int32* Index = EventToIndex.Find(InEventName);
		if (Index)
		{
			return *Index;
		}
		return INDEX_NONE;
	}

	FRigInfluenceMapPerEvent Inverse() const;

	bool Merge(const FRigInfluenceMapPerEvent& Other);

	FRigInfluenceEntryModifier GetEntryModifier(const FName& InEventName, const FRigElementKey& InKey) const;

	void SetEntryModifier(const FName& InEventName, const FRigElementKey& InKey, const FRigInfluenceEntryModifier& InModifier);

	void OnKeyRemoved(const FRigElementKey& InKey);

	void OnKeyRenamed(const FRigElementKey& InOldKey, const FRigElementKey& InNewKey);

protected:

	UPROPERTY()
	TArray<FRigInfluenceMap> Maps;

	UPROPERTY()
	TMap<FName, int32> EventToIndex;
};

