// Copyright Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveObjectCrc32.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogArchiveObjectCrc32, Log, All);

//#define DEBUG_ARCHIVE_OBJECT_CRC32

/*----------------------------------------------------------------------------
	FArchiveObjectCrc32
----------------------------------------------------------------------------*/

FArchiveObjectCrc32::FArchiveObjectCrc32()
	: MemoryWriter(SerializedObjectData)
	, ObjectBeingSerialized(NULL)
	, RootObject(NULL)
{
	ArIgnoreOuterRef = true;

	// Set FArchiveObjectCrc32 to be a saving archive instead of a reference collector.
	// Reference collection causes FSoftObjectPtrs to be serialized by their weak pointer,
	// which doesn't give a stable CRC.  Serializing these to a saving archive will
	// use a string reference instead, which is a more meaningful CRC'able state.
	SetIsSaving(true);
}

void FArchiveObjectCrc32::Serialize(void* Data, int64 Length)
{
	MemoryWriter.Serialize(Data, Length);
}

FArchive& FArchiveObjectCrc32::operator<<(class FName& Name)
{
	checkSlow(ObjectBeingSerialized);

	// Don't include the name of the object being serialized, since that isn't technically part of the object's state
	if(Name != ObjectBeingSerialized->GetFName())
	{
		MemoryWriter << Name;
	}

	return *this;
}

FArchive& FArchiveObjectCrc32::operator<<(class UObject*& Object)
{
	FArchive& Ar = *this;

	if (!RootObject || !Object || !Object->IsIn(RootObject))
	{
		auto UniqueName = GetPathNameSafe(Object);
		Ar << UniqueName;
	}
	else
	{
		ObjectsToSerialize.Enqueue(Object);
	}

	return Ar;
}

FArchive& FArchiveObjectCrc32::operator<<(FObjectPtr& ObjectPtr)
{
	FArchive& Ar = *this;
	if (!ObjectPtr.IsResolved())
	{
		auto UniqueName = ObjectPtr.GetPathName();
		Ar << UniqueName;
	}
	else
	{
		UObject* Object = ObjectPtr.Get();
		Ar << Object;
	}
	return Ar;
}

uint32 FArchiveObjectCrc32::Crc32(UObject* Object, UObject* Root, uint32 CRC)
{
#ifdef DEBUG_ARCHIVE_OBJECT_CRC32
	const double StartTime = FPlatformTime::Seconds();
	UE_LOG(LogArchiveObjectCrc32, Log, TEXT("### Calculating CRC for object: %s with outer: %s"), *Object->GetName(), Object->GetOuter() ? *Object->GetOuter()->GetName() : TEXT("NULL"));
#endif
	RootObject = Root;
	if (Object)
	{
		TSet<UObject*> SerializedObjects;

		// Start with the given object
		ObjectsToSerialize.Enqueue(Object);

		// Continue until we no longer have any objects to serialized
		while (ObjectsToSerialize.Dequeue(Object))
		{
			bool bAlreadyProcessed = false;
			SerializedObjects.Add(Object, &bAlreadyProcessed);
			// If we haven't already serialized this object
			if (!bAlreadyProcessed)
			{
#ifdef DEBUG_ARCHIVE_OBJECT_CRC32
				UE_LOG(LogArchiveObjectCrc32, Log, TEXT("- Serializing object: %s with outer: %s"), *Object->GetName(), Object->GetOuter() ? *Object->GetOuter()->GetName() : TEXT("NULL"));
#endif
				// Serialize it
				ObjectBeingSerialized = Object;
				if (!CustomSerialize(Object))
				{
					Object->Serialize(*this);
				}
				ObjectBeingSerialized = NULL;

				// Calculate the CRC, compounding it with the checksum calculated from the previous object
				CRC = FCrc::MemCrc32(SerializedObjectData.GetData(), SerializedObjectData.Num(), CRC);
#ifdef DEBUG_ARCHIVE_OBJECT_CRC32
				UE_LOG(LogArchiveObjectCrc32, Log, TEXT("=> object: '%s', total size: %d bytes, checksum: 0x%08x"), *GetPathNameSafe(Object), SerializedObjectData.Num(), CRC);
#endif
				// Cleanup
				MemoryWriter.Seek(0L);
				SerializedObjectData.Empty();
			}
		}

		// Cleanup
		SerializedObjects.Empty();
		RootObject = NULL;
	}

#ifdef DEBUG_ARCHIVE_OBJECT_CRC32
	UE_LOG(LogArchiveObjectCrc32, Log, TEXT("### Finished (%.02f ms), final checksum: 0x%08x"), (FPlatformTime::Seconds() - StartTime) * 1000.0f, CRC);
#endif
	return CRC;
}

uint32 FArchiveObjectCrc32::Crc32(UObject* Object, uint32 CRC)
{
	return Crc32(Object, Object, CRC);
}
