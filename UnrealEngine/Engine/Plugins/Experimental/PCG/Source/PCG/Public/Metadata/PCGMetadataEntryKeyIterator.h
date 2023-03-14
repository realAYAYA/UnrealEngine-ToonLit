// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metadata/PCGMetadataCommon.h"

class UPCGPointData;
class FPCGMetadataAttributeBase;

/**
 * Base class for an iterator on EntryKeys for Metadata
 * This is used to abstract multiple ways to iterate over entry keys.
 * Its main use is for metadata operations
 * 
 * It is behaving as a normal iterator. It can be dereferenced, pre-incremented
 * and we can check if we reached the end of the iterator.
 * Also, the iterator can be marked to automatically "reset" if it reached the end,
 * allowing to loop over the same data.
 */
class IPCGMetadataEntryIterator
{
public:
	IPCGMetadataEntryIterator(bool bInRepeat) : bRepeat(bInRepeat) {}
	virtual ~IPCGMetadataEntryIterator() = default;

	// Can't copy
	IPCGMetadataEntryIterator(const IPCGMetadataEntryIterator& Other) = delete;
	IPCGMetadataEntryIterator& operator=(const IPCGMetadataEntryIterator& Other) = delete;

	IPCGMetadataEntryIterator(IPCGMetadataEntryIterator&& Other) = default;
	IPCGMetadataEntryIterator& operator=(IPCGMetadataEntryIterator&& Other) = default;

	virtual PCGMetadataEntryKey operator*() = 0;
	virtual IPCGMetadataEntryIterator& operator++() = 0;
	virtual bool IsEnd() const = 0;

	bool IsRepeat() { return bRepeat; }

private:
	bool bRepeat = false;
};

/**
 * Will iterate over all the EntryKeys in the EntryKeyToValueKey mapping
 * for this attribute, including its parents.
 * 
 * Note that this iterator is not thread safe on the Attribute entry key map.
 */
class FPCGMetadataEntryAttributeIterator : public IPCGMetadataEntryIterator
{
public:
	FPCGMetadataEntryAttributeIterator(const FPCGMetadataAttributeBase& InAttribute, bool bRepeat);
	~FPCGMetadataEntryAttributeIterator();
	virtual PCGMetadataEntryKey operator*() override;
	virtual IPCGMetadataEntryIterator& operator++() override;
	virtual bool IsEnd() const override;

private:
	using Iterator = TMap<PCGMetadataEntryKey, PCGMetadataValueKey>::TConstIterator;

	void FindValidPtr();
	void ResetItPtr();

	const FPCGMetadataAttributeBase& OriginalAttribute;
	const FPCGMetadataAttributeBase* CurrentAttribute = nullptr;
	Iterator* ItPtr = nullptr;
	bool bIsInvalid = false;
};

/**
 * Will iterate over all the EntryKeys for each point in the point data.
 */
class FPCGMetadataEntryPointIterator : public IPCGMetadataEntryIterator
{
public:
	FPCGMetadataEntryPointIterator(const UPCGPointData* InPointData, bool bRepeat);
	virtual PCGMetadataEntryKey operator*() override;
	virtual IPCGMetadataEntryIterator& operator++() override;
	virtual bool IsEnd() const override;

private:
	const UPCGPointData* PointData;
	int32 CurrentIndex = 0;
};

/**
 * Will return a constant value. If repeat is not set, it will return a constant value
 * only once before marking it ended.
 */
class FPCGMetadataEntryConstantIterator : public IPCGMetadataEntryIterator
{
public:
	FPCGMetadataEntryConstantIterator(PCGMetadataEntryKey InKey, bool bRepeat);
	virtual PCGMetadataEntryKey operator*() override;
	virtual IPCGMetadataEntryIterator& operator++() override;
	virtual bool IsEnd() const override;

private:
	PCGMetadataEntryKey Key;
	bool bHasEnded = false;
};