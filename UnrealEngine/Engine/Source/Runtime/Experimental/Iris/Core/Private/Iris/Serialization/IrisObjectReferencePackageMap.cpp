// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/IrisObjectReferencePackageMap.h"

bool UIrisObjectReferencePackageMap::SerializeObject(FArchive& Ar, UClass* InClass, UObject*& Obj, FNetworkGUID* OutNetGUID)
{
	if (!References)
	{
		return false;
	}

	if (Ar.IsSaving())
	{
		int32 Index;
		if (!References->Find(Obj, Index))
		{
			Index = References->Add(Obj);
		}
		uint8 IndexByte = Index;
		if (ensureAlwaysMsgf(IndexByte < References->Num(), TEXT("UIrisObjectReferencePackageMap::SerializeObject, failed to serialize object reference. A Maximum of 256 references are currently supported by this PackageMap")))
		{
			Ar << IndexByte;
		}
		else
		{
			return false;
		}
	}
	else
	{
		uint8 IndexByte = 255U;
		Ar << IndexByte;
		if (ensureAlwaysMsgf(IndexByte < References->Num(), TEXT("UIrisObjectReferencePackageMap::SerializeObject, failed to read object reference index %u is out of bounds"), IndexByte))
		{
			Obj = (*References)[IndexByte];
		}
		else
		{
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

