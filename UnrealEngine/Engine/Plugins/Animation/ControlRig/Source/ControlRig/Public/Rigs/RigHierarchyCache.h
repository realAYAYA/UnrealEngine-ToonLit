// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "RigHierarchyElements.h"

#include "RigHierarchyCache.generated.h"

class URigHierarchy;

USTRUCT(BlueprintType)
struct CONTROLRIG_API FCachedRigElement
{
	GENERATED_BODY()

public:

	FCachedRigElement()
		: Key()
		, Index(UINT16_MAX)
		, ContainerVersion(INDEX_NONE)
	{}

	FCachedRigElement(const FRigElementKey& InKey, const URigHierarchy* InHierarchy, bool bForceStoreKey = false)
		: Key()
		, Index(UINT16_MAX)
		, ContainerVersion(INDEX_NONE)
	{
		UpdateCache(InKey, InHierarchy);
		if(bForceStoreKey)
		{
			Key = InKey;
		}
	}

	bool IsValid() const
	{
		return GetIndex() != INDEX_NONE && Key.IsValid();
	}

	void Reset()
	{
		Key = FRigElementKey();
		Index = UINT16_MAX;
		ContainerVersion = INDEX_NONE;
		Element = nullptr;
	}

	explicit operator bool() const
	{
		return IsValid();
	}

	operator int32() const
	{
		return GetIndex();
	}

	explicit operator FRigElementKey() const
	{
		return Key;
	}

	int32 GetIndex() const
	{
		if(Index == UINT16_MAX)
		{
			return INDEX_NONE;
		}
		return (int32)Index;
	}

	const FRigElementKey& GetKey() const
	{
		return Key;
	}

	const FRigElementKey& GetResolvedKey() const;

	const FRigBaseElement* GetElement() const
	{
		return Element;
	}

	bool UpdateCache(const URigHierarchy* InHierarchy);

	bool UpdateCache(const FRigElementKey& InKey, const URigHierarchy* InHierarchy);

	friend uint32 GetTypeHash(const FCachedRigElement& Cache)
	{
		return GetTypeHash(Cache.Key) * 13 + (uint32)Cache.Index;
	}

	bool IsIdentical(const FRigElementKey& InKey, const URigHierarchy* InHierarchy);

	bool operator ==(const FCachedRigElement& Other) const
	{
		return Index == Other.Index && Key == Other.Key;
	}

	bool operator !=(const FCachedRigElement& Other) const
	{
		return Index != Other.Index || Key != Other.Key;
	}

	bool operator ==(const FRigElementKey& Other) const
	{
		return Key == Other;
	}

	bool operator !=(const FRigElementKey& Other) const
	{
		return Key != Other;
	}

	bool operator ==(const int32& Other) const
	{
		return GetIndex() == Other;
	}

	bool operator !=(const int32& Other) const
	{
		return GetIndex() != Other;
	}

	bool operator <(const FCachedRigElement& Other) const
	{
		if (Key < Other.Key)
		{
			return true;
		}
		return Index < Other.Index;
	}

	bool operator >(const FCachedRigElement& Other) const
	{
		if (Key > Other.Key)
		{
			return true;
		}
		return Index > Other.Index;
	}

private:

	UPROPERTY()
	FRigElementKey Key;

	UPROPERTY()
	uint16 Index;

	UPROPERTY()
	int32 ContainerVersion;

	const FRigBaseElement* Element;
};

class CONTROLRIG_API FRigElementKeyRedirector
{
public:

	FRigElementKeyRedirector()
		: InternalKeyToExternalKey()
		, Hash(UINT32_MAX)
	{}

	FRigElementKeyRedirector(const TMap<FRigElementKey, FRigElementKey>& InMap, const URigHierarchy* InHierarchy);
	FRigElementKeyRedirector(const FRigElementKeyRedirector& InOther, const URigHierarchy* InHierarchy);

	bool Contains(const FRigElementKey& InKey) const { return InternalKeyToExternalKey.Contains(InKey); }
	const FCachedRigElement* Find(const FRigElementKey& InKey) const { return InternalKeyToExternalKey.Find(InKey); }
	const FRigElementKey* FindExternalKey(const FRigElementKey& InKey) const { return ExternalKeys.Find(InKey); }
	FCachedRigElement* Find(const FRigElementKey& InKey) { return InternalKeyToExternalKey.Find(InKey); }
	const FRigElementKey* FindReverse(const FRigElementKey& InKey) const;
	uint32 GetHash() const { return Hash; }
	
private:

	void Add(const FRigElementKey& InSource, const FRigElementKey& InTarget, const URigHierarchy* InHierarchy);

	TMap<FRigElementKey, FCachedRigElement> InternalKeyToExternalKey;
	TMap<FRigElementKey, FRigElementKey> ExternalKeys;
	uint32 Hash;

	friend class URigHierarchy;
	friend class URigHierarchyController;
	friend class FRigModuleInstanceDetails;
	friend class UModularRig;
	friend class FControlRigSchematicModel;
};