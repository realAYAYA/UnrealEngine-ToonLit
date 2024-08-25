// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VerseVM/VVMNameValueMap.h"

class UPackage;

namespace Verse
{
enum class EDigestVariant : uint8
{
	PublicAndEpicInternal = 0,
	PublicOnly = 1,
};

struct VPackage : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VUTF8String> DigestCode[2]; // One for each variant

	VUTF8String& GetName() const { return *PackageName; }

	uint32 Num() const { return Map.Num(); }
	const VUTF8String& GetName(uint32 Index) const { return Map.GetName(Index); }
	VValue GetDefinition(uint32 Index) const { return Map.GetValue(Index); }
	void AddDefinition(FAllocationContext Context, FUtf8StringView Name, VValue Definition) { Map.AddValue(Context, Name, Definition); }
	void AddDefinition(FAllocationContext Context, VUTF8String& Name, VValue Definition) { Map.AddValue(Context, Name, Definition); }
	VValue LookupDefinition(FUtf8StringView Name) const { return Map.Lookup(Name); }
	template <typename CellType>
	CellType* LookupDefinition(FUtf8StringView Name) const { return Map.LookupCell<CellType>(Name); }

	bool HasUPackage() const { return !!AssociatedUPackage; }
	UPackage* GetUPackage() const { return AssociatedUPackage ? reinterpret_cast<UPackage*>(AssociatedUPackage.Get().AsUObject()) : nullptr; }
	UPackage* GetOrCreateUPackage(FAllocationContext Context) { return AssociatedUPackage ? reinterpret_cast<UPackage*>(AssociatedUPackage.Get().AsUObject()) : CreateUPackage(Context); }
	enum class EPackageStage : uint8
	{
		Global,
		Temp,
		Dead
	};
	COREUOBJECT_API FString GetUPackageName(EPackageStage Stage) const;

	static VPackage& New(FAllocationContext Context, VUTF8String& Name, uint32 Capacity)
	{
		return *new (Context.AllocateFastCell(sizeof(VPackage))) VPackage(Context, Name, Capacity);
	}

private:
	VPackage(FAllocationContext Context, VUTF8String& Name, uint32 Capacity)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, PackageName(Context, &Name)
		, Map(Context, Capacity)
	{
	}

	COREUOBJECT_API UPackage* CreateUPackage(FAllocationContext Context);

	TWriteBarrier<VUTF8String> PackageName;
	VNameValueMap Map;
	TWriteBarrier<VValue> AssociatedUPackage;
};
} // namespace Verse
#endif // WITH_VERSE_VM
