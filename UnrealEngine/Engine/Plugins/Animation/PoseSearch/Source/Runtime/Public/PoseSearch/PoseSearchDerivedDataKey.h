// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Hash/Blake3.h"
#include "IO/IoHash.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/UnrealType.h"

namespace UE::PoseSearch
{

// log properties and UObjects names
#ifndef UE_POSE_SEARCH_DERIVED_DATA_LOGGING
	#define UE_POSE_SEARCH_DERIVED_DATA_LOGGING 0
#endif

// log properties data
#ifndef UE_POSE_SEARCH_DERIVED_DATA_LOGGING_VERBOSE
	#define UE_POSE_SEARCH_DERIVED_DATA_LOGGING_VERBOSE 0
#endif

class POSESEARCH_API FKeyBuilder : public FArchiveUObject
{
public:
	using Super = FArchiveUObject;
	using HashDigestType = FBlake3Hash;
	using HashBuilderType = FBlake3;

	inline static const FName ExcludeFromHashName = FName(TEXT("ExcludeFromHash"));
	inline static const FName NeverInHashName = FName(TEXT("NeverInHash"));
	inline static const FName IgnoreForMemberInitializationTestName = FName(TEXT("IgnoreForMemberInitializationTest"));
	
	FKeyBuilder();
	FKeyBuilder(const UObject* Object, bool bUseDataVer, bool bPerformConditionalPostLoadIfRequired);

	using Super::IsSaving;
	using Super::operator<<;

	// Begin FArchive Interface
	virtual void Seek(int64 InPos) override;
	virtual bool ShouldSkipProperty(const FProperty* InProperty) const override;
	virtual void Serialize(void* Data, int64 Length) override;
	virtual FArchive& operator<<(FName& Name) override;
	virtual FArchive& operator<<(class UObject*& Object) override;
	virtual FString GetArchiveName() const override;
	// End FArchive Interface
	
	bool AnyAssetNotReady() const;
	FIoHash Finalize() const;
	const TSet<const UObject*>& GetDependencies() const;

protected:
	// to keep the key generation lightweight, we don't hash these types
	static bool IsExcludedType(class UObject* Object);

	// to keep the key generation lightweight, we hash only the full names for these types. Object(s) will be added to Dependencies
	static bool IsAddNameOnlyType(class UObject* Object);

#if UE_POSE_SEARCH_DERIVED_DATA_LOGGING
	FString GetIndentation() const;
	int32 Indentation = 0;
#endif

	HashBuilderType Hasher;

	// Set of objects that have already been serialized
	TSet<const UObject*> Dependencies;

	// Object currently being serialized
	const UObject* ObjectBeingSerialized = nullptr;

	// true if some dependent assets are not ready (fully loaded)
	bool bAnyAssetNotReady = false;

	// if true ConditionalPostLoad will be performed on the dependant assets requiring it
	bool bPerformConditionalPostLoad = false;
};

} // namespace UE::PoseSearch

#endif // WITH_EDITOR