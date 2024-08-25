// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PoseSearch/PoseSearchDerivedDataKey.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimCurveCompressionSettings.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimationModifier.h"
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

FKeyBuilder::FKeyBuilder(const UObject* Object, bool bUseDataVer, bool bPerformConditionalPostLoadIfRequired)
: FKeyBuilder()
{
	check(Object);
	bPerformConditionalPostLoad = bPerformConditionalPostLoadIfRequired;

	if (bUseDataVer)
	{
		// used to invalidate the key without having to change POSESEARCHDB_DERIVEDDATA_VER all the times
		int32 POSESEARCHDB_DERIVEDDATA_VER_SMALL = 247;
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
		#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		UE_LOG(LogPoseSearch, Log, TEXT("%s x %s (ShouldSkipProperty)"), *GetIndentation(), *InProperty->GetFullName());
		#endif
		return true;
	}

	if (InProperty->HasAllPropertyFlags(CPF_Transient))
	{
		#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		UE_LOG(LogPoseSearch, Log, TEXT("%s x %s (Transient)"), *GetIndentation(), *InProperty->GetFullName());
		#endif
		return true;
	}

	if (InProperty->HasMetaData(ExcludeFromHashName))
	{
		#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
		UE_LOG(LogPoseSearch, Log, TEXT("%s x %s (ExcludeFromHash)"), *GetIndentation(), *InProperty->GetFullName());
		#endif
		return true;
	}
		
	check(!InProperty->HasMetaData(IgnoreForMemberInitializationTestName));
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
		if (Object->HasAnyFlags(RF_NeedPostLoad))
		{
			if (bPerformConditionalPostLoad)
			{
				Object->ConditionalPostLoad();
			}
			else
			{
				bAnyAssetNotReady = true;
			}
		}
		
		if (!bAnyAssetNotReady)
		{
			#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
			++Indentation;
			#endif

			if (Object->HasAnyFlags(RF_Transient))
			{
				#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
				UE_LOG(LogPoseSearch, Log, TEXT("%sTransient '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
				#endif
			}
			else if (IsExcludedType(Object))
			{
				#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
				UE_LOG(LogPoseSearch, Log, TEXT("%sExcluded '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
				#endif
			}
			else
			{
				bool bAlreadyProcessed = false;
				Dependencies.Add(Object, &bAlreadyProcessed);

				// If we haven't already serialized this object
				if (bAlreadyProcessed)
				{
					#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
					UE_LOG(LogPoseSearch, Log, TEXT("%sAlreadyProcessed '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
					#endif
				}
				// for specific types we only add their names to the hash
				else if (IsAddNameOnlyType(Object))
				{
					#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
					UE_LOG(LogPoseSearch, Log, TEXT("%sAddingNameOnly '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
					#endif

					FString ObjectName = GetFullNameSafe(Object);
					*this << ObjectName;
				}
				else
				{
					const UObject* PreviousObjectBeingSerialized = ObjectBeingSerialized;
					ObjectBeingSerialized = Object;

					#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
					UE_LOG(LogPoseSearch, Log, TEXT("%sBegin '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
					#endif

					const_cast<UObject*>(Object)->Serialize(*this);

					#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
					UE_LOG(LogPoseSearch, Log, TEXT("%sEnd '%s' (%s)"), *GetIndentation(), *Object->GetName(), *Object->GetClass()->GetName());
					#endif

					ObjectBeingSerialized = PreviousObjectBeingSerialized;
				}
			}

			#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
			--Indentation;
			#endif
		}
	}

	return *this;
}

FString FKeyBuilder::GetArchiveName() const
{
	return TEXT("FDerivedDataKeyBuilder");
}
	
bool FKeyBuilder::AnyAssetNotReady() const
{
	return bAnyAssetNotReady;
}

FIoHash FKeyBuilder::Finalize() const
{
	check(!bAnyAssetNotReady); // otherwise key can be non deterministic
	// Stores a BLAKE3-160 hash, taken from the first 20 bytes of a BLAKE3-256 hash
	return FIoHash(Hasher.Finalize());
}

const TSet<const UObject*>& FKeyBuilder::GetDependencies() const
{
	return Dependencies;
}

// to keep the key generation lightweight, we don't hash these types
bool FKeyBuilder::IsExcludedType(class UObject* Object)
{
	return
		nullptr != Cast<UAnimationModifier>(Object);
}

// to keep the key generation lightweight, we hash only the full names for these types. Object(s) will be added to Dependencies
bool FKeyBuilder::IsAddNameOnlyType(class UObject* Object)
{
	return
		nullptr != Cast<IAnimationDataModel>(Object) ||
		nullptr != Cast<UActorComponent>(Object) ||
		nullptr != Cast<UAnimBoneCompressionSettings>(Object) ||
		nullptr != Cast<UAnimCurveCompressionSettings>(Object) ||
		nullptr != Cast<UAssetImportData>(Object) ||
		nullptr != Cast<UFunction>(Object) ||
		nullptr != Cast<UMirrorDataTable>(Object) ||
		nullptr != Cast<USkeletalMesh>(Object) ||
		nullptr != Cast<USkeletalMeshSocket>(Object) ||
		nullptr != Cast<USkeleton>(Object) ||
		nullptr != Cast<UStreamableRenderAsset>(Object);
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