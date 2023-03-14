// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGMetadataEntryKeyIterator.h"

#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadataAttribute.h"

// FPCGMetadataEntryAttributeIterator

FPCGMetadataEntryAttributeIterator::FPCGMetadataEntryAttributeIterator(const FPCGMetadataAttributeBase& InAttribute, bool bRepeat)
	: IPCGMetadataEntryIterator(bRepeat)
	, OriginalAttribute(InAttribute)
	, CurrentAttribute(&InAttribute)
{
	ItPtr = new Iterator(InAttribute.GetEntryToValueKeyMap_NotThreadSafe());

	FindValidPtr();

	if (!(*ItPtr) && CurrentAttribute->GetParent() == nullptr)
	{
		bIsInvalid = true;
	}
}

void FPCGMetadataEntryAttributeIterator::FindValidPtr()
{
	while (!(*ItPtr) && CurrentAttribute->GetParent() != nullptr)
	{
		CurrentAttribute = CurrentAttribute->GetParent();
		ResetItPtr();
	}
}

void FPCGMetadataEntryAttributeIterator::ResetItPtr()
{
	// Since Iterators doesn't support copy nor move constructors we need to
	// do this ugly dance with an explicit call to the destructor and a placement new to
	// call the constructor again.
	if (!bIsInvalid && CurrentAttribute)
	{
		ItPtr->~Iterator();
		new(ItPtr) Iterator(CurrentAttribute->GetEntryToValueKeyMap_NotThreadSafe());
	}
}

FPCGMetadataEntryAttributeIterator::~FPCGMetadataEntryAttributeIterator()
{
	delete ItPtr;
}

PCGMetadataEntryKey FPCGMetadataEntryAttributeIterator::operator*()
{
	return (*ItPtr)->Key;
}

IPCGMetadataEntryIterator& FPCGMetadataEntryAttributeIterator::operator++()
{
	Iterator& It = *ItPtr;

	check(!bIsInvalid && !!It);

	// Special case where there is no parent, has a single element and is repeat, we do nothing.
	if (OriginalAttribute.GetNumberOfEntries() == 1 && OriginalAttribute.GetParent() == nullptr && IsRepeat())
	{
		return *this;
	}

	// Otherwise, increment the iterator
	if (!(++It))
	{
		// If it is not valid, try to find a valid one in its parents
		FindValidPtr();

		// If we reached the root of the parents, the iterator is still not valid and we repeat, 
		// we reset to the original attribute.
		if (!It && CurrentAttribute->GetParent() == nullptr && IsRepeat())
		{
			CurrentAttribute = &OriginalAttribute;
			ResetItPtr();
			FindValidPtr();
		}
	}

	return *this;
}

bool FPCGMetadataEntryAttributeIterator::IsEnd() const
{
	return bIsInvalid || !!(*ItPtr);
}

// FPCGMetadataEntryPointIterator

FPCGMetadataEntryPointIterator::FPCGMetadataEntryPointIterator(const UPCGPointData* InPointData, bool bRepeat)
	: IPCGMetadataEntryIterator(bRepeat)
	, PointData(InPointData)
	, CurrentIndex(0)
{

}

PCGMetadataEntryKey FPCGMetadataEntryPointIterator::operator*()
{
	return PointData->GetPoints()[CurrentIndex].MetadataEntry;
}

IPCGMetadataEntryIterator& FPCGMetadataEntryPointIterator::operator++()
{
	if (++CurrentIndex >= PointData->GetPoints().Num() && IsRepeat())
	{
		CurrentIndex = 0;
	}

	return *this;
}

bool FPCGMetadataEntryPointIterator::IsEnd() const
{
	return CurrentIndex >= PointData->GetPoints().Num();
}

// FPCGMetadataEntryConstantIterator

FPCGMetadataEntryConstantIterator::FPCGMetadataEntryConstantIterator(PCGMetadataEntryKey InKey, bool bRepeat)
	: IPCGMetadataEntryIterator(bRepeat)
	, Key(InKey)
	, bHasEnded(false)
{

}

PCGMetadataEntryKey FPCGMetadataEntryConstantIterator::operator*()
{
	return Key;
}

IPCGMetadataEntryIterator& FPCGMetadataEntryConstantIterator::operator++()
{
	bHasEnded = !IsRepeat();
	return *this;
}

bool FPCGMetadataEntryConstantIterator::IsEnd() const
{
	return bHasEnded;
}