// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/IrisObjectReferencePackageMap.h"

bool UIrisObjectReferencePackageMap::SerializeObject(FArchive& Ar, UClass* InClass, UObject*& Obj, FNetworkGUID* OutNetGUID)
{
	if (!References)
	{
		return false;
	}

	constexpr uint8 MaxNumReferences = 255U;

	if (Ar.IsSaving())
	{
		int32 Index = MaxNumReferences;
		if (!References->Find(Obj, Index) && References->Num() < MaxNumReferences)
		{
			Index = References->Add(Obj);
		}

		if (References->IsValidIndex(Index))
		{
			uint8 IndexByte = static_cast<uint8>(Index);
			Ar << IndexByte;
		}
		else
		{
			ensureMsgf(false, TEXT("UIrisObjectReferencePackageMap::SerializeObject, failed to serialize object reference with Index %u (%s). A Maximum of %u references are currently supported by this PackageMap"),
				Index, *GetNameSafe(Obj), MaxNumReferences);
			uint8 IndexByte = MaxNumReferences;
			Ar << IndexByte;
			return false;
		}
	}
	else
	{
		uint8 IndexByte = MaxNumReferences;
		Ar << IndexByte;
		if (References->IsValidIndex(IndexByte) && IndexByte < MaxNumReferences)
		{
			Obj = (*References)[IndexByte];
		}
		else
		{
			ensureMsgf(false, TEXT("UIrisObjectReferencePackageMap::SerializeObject, failed to read object reference index %u is out of bounds. Current ObjectReference num: %u"), IndexByte, References->Num());
			return false;
		}
	}

	return true;
}

void UIrisObjectReferencePackageMap::InitForRead(const FObjectReferenceArray* InReferences)
{ 
	References = const_cast<FObjectReferenceArray*>(InReferences);
}

void UIrisObjectReferencePackageMap::InitForWrite(FObjectReferenceArray* InReferences)
{
	References = InReferences; 
	if (References)
	{
		References->Reset();
	}
}

