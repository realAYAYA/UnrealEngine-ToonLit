// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairDescription.h"
#include "Misc/SecureHash.h"
#include "Serialization/BulkDataReader.h"
#include "Serialization/BulkDataWriter.h"

const FStrandID FStrandID::Invalid(TNumericLimits<uint32>::Max());
const FGroomID FGroomID::Invalid(TNumericLimits<uint32>::Max());

FHairDescription::FHairDescription()
	: NumVertices(0)
	, NumStrands(0)
{
	// Required attributes
	StrandAttributesSet.RegisterAttribute<int>(HairAttribute::Strand::VertexCount);
	VertexAttributesSet.RegisterAttribute<FVector3f>(HairAttribute::Vertex::Position, 1, FVector3f::ZeroVector);

	// Only one set of groom attributes
	GroomAttributesSet.Initialize(1);
}

void FHairDescription::InitializeVertices(int32 InNumVertices)
{
	NumVertices = InNumVertices;
	VertexAttributesSet.Initialize(InNumVertices);
}

void FHairDescription::InitializeStrands(int32 InNumStrands)
{
	NumStrands = InNumStrands;
	StrandAttributesSet.Initialize(InNumStrands);
}

FVertexID FHairDescription::AddVertex()
{
	FVertexID VertexID = FVertexID(NumVertices++);
	VertexAttributesSet.Insert(VertexID);
	return VertexID;
}

FStrandID FHairDescription::AddStrand()
{
	FStrandID StrandID = FStrandID(NumStrands++);
	StrandAttributesSet.Insert(StrandID);
	return StrandID;
}

void FHairDescription::Reset()
{
	NumVertices = 0;
	NumStrands = 0;

	VertexAttributesSet.Initialize(0);
	StrandAttributesSet.Initialize(0);
	GroomAttributesSet.Initialize(0);
}

bool FHairDescription::IsValid() const
{
	return (NumStrands > 0) && (NumVertices > 0);
}


void FHairDescription::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	Ar << NumVertices;
	Ar << NumStrands;

	Ar << VertexAttributesSet;
	Ar << StrandAttributesSet;
	Ar << GroomAttributesSet;
}

#if WITH_EDITORONLY_DATA

void FHairDescriptionBulkData::Serialize(FArchive& Ar, UObject* Owner)
{
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	if (Ar.IsTransacting())
	{
		// If transacting, keep these members alive the other side of an undo, otherwise their values will get lost
		CustomVersions.Serialize(Ar);
		Ar << bBulkDataUpdated;
	}
	else
	{
		if (Ar.IsSaving())
		{
			// If the bulk data hasn't been updated since this was loaded, there's a possibility that it has old versioning.
			// Explicitly load and resave the FHairDescription so that its version is in sync with the FHairDescriptionBulkData.
			if (!bBulkDataUpdated)
			{
				FHairDescription HairDescription;
				LoadHairDescription(HairDescription);
				SaveHairDescription(HairDescription);
			}
		}
	}

	BulkData.Serialize(Ar, Owner);

	Ar << Guid;

	if (!Ar.IsTransacting() && Ar.IsLoading())
	{
		// If loading, take the package custom version so it can be applied to the bulk data archive
		// when unpacking HairDescription from it
		// TODO: Save the UEVersion and LicenseeVersion as well
		FPackageFileVersion UEVersion;
		int32 LicenseeUEVersion;
		BulkData.GetBulkDataVersions(Ar, UEVersion, LicenseeUEVersion, CustomVersions);
	}
}

void FHairDescriptionBulkData::SaveHairDescription(FHairDescription& HairDescription)
{
	BulkData.RemoveBulkData();

	if (HairDescription.IsValid())
	{
		FBulkDataWriter Ar(BulkData, /*bIsPersistent*/ true);
		HairDescription.Serialize(Ar);

		// Preserve CustomVersions at save time so we can reuse the same ones when reloading direct from memory
		CustomVersions = Ar.GetCustomVersions();
	}

	// Use bulk data hash instead of guid to identify content to improve DDC cache hit
	ComputeGuidFromHash();

	// Mark the HairDescriptionBulkData as having been updated.
	// This means we know that its version is up-to-date.
	bBulkDataUpdated = true;
}


void FHairDescriptionBulkData::LoadHairDescription(FHairDescription& HairDescription)
{
	HairDescription.Reset();

	if (!IsEmpty())
	{
		FBulkDataReader Ar(BulkData, /*bIsPersistent*/ true);

		// Propagate the custom version information from the package to the bulk data, so that the HairDescription
		// is serialized with the same versioning
		Ar.SetCustomVersions(CustomVersions);

		HairDescription.Serialize(Ar);
	}
}

void FHairDescriptionBulkData::Empty()
{
	BulkData.RemoveBulkData();
}

FString FHairDescriptionBulkData::GetIdString() const
{
	FString GuidString = Guid.ToString();
	GuidString += TEXT("X");
	return GuidString;
}

void FHairDescriptionBulkData::ComputeGuidFromHash()
{
	uint32 Hash[5] = {};

	if (BulkData.GetBulkDataSize() > 0)
	{
		void* Buffer = BulkData.Lock(LOCK_READ_ONLY);
		FSHA1::HashBuffer(Buffer, BulkData.GetBulkDataSize(), (uint8*)Hash);
		BulkData.Unlock();
	}

	Guid = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
}

#endif // #if WITH_EDITORONLY_DATA
