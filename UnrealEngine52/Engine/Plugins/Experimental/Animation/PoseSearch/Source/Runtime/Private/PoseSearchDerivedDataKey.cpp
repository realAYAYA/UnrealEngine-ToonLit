// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PoseSearch/PoseSearchDerivedDataKey.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimCurveCompressionSettings.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StreamableRenderAsset.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Components/SkeletalMeshComponent.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "UObject/DevObjectVersion.h"

namespace UE::PoseSearch
{

FKeyBuilder::FKeyBuilder()
{
	ArIgnoreOuterRef = true;

	// Set FDerivedDataKeyBuilder to be a saving archive instead of a reference collector.
	// Reference collection causes FSoftObjectPtrs to be serialized by their weak pointer,
	// which doesn't give a stable hash.  Serializing these to a saving archive will
	// use a string reference instead, which is a more meaningful hash value.
	SetIsSaving(true);
}

FKeyBuilder::FKeyBuilder(const UObject* Object, bool bUseDataVer)
: FKeyBuilder()
{
	check(Object);

	if (bUseDataVer)
	{
		// used to invalidate the key without having to change POSESEARCHDB_DERIVEDDATA_VER all the times
		int32 POSESEARCHDB_DERIVEDDATA_VER_SMALL = 17;
		FGuid VersionGuid = FDevSystemGuids::GetSystemGuid(FDevSystemGuids::Get().POSESEARCHDB_DERIVEDDATA_VER);

		*this << VersionGuid;
		*this << POSESEARCHDB_DERIVEDDATA_VER_SMALL;
	}

	// FKeyBuilder is a saving only archiver, and since it doesn't modify the input Object it's safe to do a const_cast 
	UObject* NonConstObject = const_cast<UObject*>(Object);
	*this << NonConstObject;
}

void FKeyBuilder::Seek(int64 InPos)
{
	checkf(InPos == Tell(), TEXT("A hash cannot be computed when serialization relies on seeking."));
	FArchiveUObject::Seek(InPos);
}

bool FKeyBuilder::ShouldSkipProperty(const FProperty* InProperty) const
{
	if (InProperty == nullptr)
	{
		return false;
	}

	if (Super::ShouldSkipProperty(InProperty))
	{
		return true;
	}

	if (!InProperty->HasAllPropertyFlags(CPF_Edit)) // bIsEditAnywhereProperty
	{
		return true;
	}

	if (InProperty->HasAllPropertyFlags(CPF_Transient))
	{
		return true;
	}

	if (InProperty->HasMetaData(ExcludeFromHashName))
	{
		return true;
	}
		
	check(!InProperty->HasMetaData(NeverInHashName));

	#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
	UE_LOG(LogPoseSearch, Log, TEXT("%s - %s"), *GetIndentation(), *InProperty->GetFullName());
	#endif

	return false;
}

void FKeyBuilder::Serialize(void* Data, int64 Length)
{
	const uint8* HasherData = reinterpret_cast<uint8*>(Data);

	#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING_VERBOSE
	FString RawBytesString = BytesToString(HasherData, Length);
	UE_LOG(LogPoseSearch, Log, TEXT("%s  > %s"), *GetIndentation(), *RawBytesString);
	#endif

	Hasher.Update(HasherData, Length);
}

FArchive& FKeyBuilder::operator<<(FName& Name)
{
	// Don't include the name of the object being serialized, since that isn't technically part of the object's state
	if (!ObjectBeingSerialized || (Name != ObjectBeingSerialized->GetFName()))
	{
		// we cannot use GetTypeHash(Name) since it's bound to be non deterministic between editor restarts, so we convert the name into an FString and let the Serialize(void* Data, int64 Length) deal with it
		FString NameString = Name.ToString();
		*this << NameString;
	}
	return *this;
}

FArchive& FKeyBuilder::operator<<(class UObject*& Object)
{
	if (Object)
	{
		#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		++Indentation;
		#endif

		bool bAlreadyProcessed = false;
		ObjectsAlreadySerialized.Add(Object, &bAlreadyProcessed);

		// If we haven't already serialized this object
		if (bAlreadyProcessed)
		{
			#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
			UE_LOG(LogPoseSearch, Log, TEXT("%sAlreadyProcessed '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
			#endif
		}
		// for specific types we only add their names to the hash
		else if (AddNameOnly(Object))
		{
			#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
			UE_LOG(LogPoseSearch, Log, TEXT("%sAddingNameOnly '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
			#endif

			FString ObjectName = GetFullNameSafe(Object);
			*this << ObjectName;
		}
		else
		{
			// Serialize it
			ObjectBeingSerialized = Object;

			#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
			UE_LOG(LogPoseSearch, Log, TEXT("%sBegin '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
			#endif

			const_cast<UObject*>(Object)->Serialize(*this);

			#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
			UE_LOG(LogPoseSearch, Log, TEXT("%sEnd '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
			#endif

			ObjectBeingSerialized = nullptr;
		}
			
		#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		--Indentation;
		#endif
	}

	return *this;
}

FString FKeyBuilder::GetArchiveName() const
{
	return TEXT("FDerivedDataKeyBuilder");
}
	
FIoHash FKeyBuilder::Finalize() const
{
	// Stores a BLAKE3-160 hash, taken from the first 20 bytes of a BLAKE3-256 hash
	return FIoHash(Hasher.Finalize());
}

const TSet<const UObject*>& FKeyBuilder::GetDependencies() const
{
	return ObjectsAlreadySerialized;
}

bool FKeyBuilder::AddNameOnly(class UObject* Object) const
{
	return
		Cast<UAnimBoneCompressionSettings>(Object) ||
		Cast<UAnimCurveCompressionSettings>(Object) ||
		Cast<UFbxAnimSequenceImportData>(Object) ||
		Cast<UMirrorDataTable>(Object) ||
		Cast<USkeleton>(Object) ||
		Cast<USkeletalMeshComponent>(Object) ||
		Cast<USkeletalMeshSocket>(Object) ||
		Cast<USkinnedMeshComponent>(Object) ||
		Cast<UMeshComponent>(Object) ||
		Cast<UPrimitiveComponent>(Object) ||
		Cast<USceneComponent>(Object) ||
		Cast<UActorComponent>(Object) ||
		Cast<UStreamableRenderAsset>(Object) ||
		Cast<USkinnedAsset>(Object) ||
		Cast<USkeletalMesh>(Object);
}

#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
FString FKeyBuilder::GetIndentation() const
{
	FString IndentationString;
	for (int32 i = 0; i < Indentation; ++i)
	{
		IndentationString.Append(" ");
	}
	return IndentationString;
}
#endif

} // namespace UE::PoseSearch

#endif // WITH_EDITOR