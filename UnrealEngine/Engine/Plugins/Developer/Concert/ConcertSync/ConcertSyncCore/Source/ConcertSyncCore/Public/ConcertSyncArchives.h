// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "IdentifierTable/ConcertTransportArchives.h"

struct FLazyObjectPtr;
struct FObjectPtr;
struct FSoftObjectPtr;
struct FWeakObjectPtr;

struct FConcertSessionVersionInfo;

DECLARE_DELEGATE_OneParam(FConcertSyncRemapObjectPath, FString& /*ObjectPath*/)
DECLARE_DELEGATE_RetVal_OneParam(bool, FConcertSyncObjectPathBelongsToWorld, const FStringView /*ObjectPath*/)
DECLARE_DELEGATE_OneParam(FConcertSyncEncounteredMissingObject, const FStringView /*ObjectPath*/)

namespace ConcertSyncUtil
{
CONCERTSYNCCORE_API bool CanExportProperty(const FProperty* Property, const bool InIncludeEditorOnlyData);

CONCERTSYNCCORE_API void ResetObjectPropertiesToArchetypeValues(UObject* Object, const bool InIncludeEditorOnlyData);

CONCERTSYNCCORE_API const FSoftObjectPath& GetSkipObjectPath();
}

/** Util to handle remapping objects within the source world to be the equivalent objects in the destination world */
class CONCERTSYNCCORE_API FConcertSyncWorldRemapper
{
public:
	FConcertSyncWorldRemapper() = default;

	FConcertSyncWorldRemapper(FString InSourceWorldPathName, FString InDestWorldPathName)
		: SourceWorldPathName(MoveTemp(InSourceWorldPathName))
		, DestWorldPathName(MoveTemp(InDestWorldPathName))
	{
	}

	FString RemapObjectPathName(const FString& InObjectPathName) const;

	bool ObjectBelongsToWorld(const FString& InObjectPathName) const;

	bool HasMapping() const;

	FConcertSyncRemapObjectPath RemapDelegate;
	FConcertSyncObjectPathBelongsToWorld ObjectPathBelongsToWorldDelegate;

private:
	FString SourceWorldPathName;
	FString DestWorldPathName;
};

/** Archive for writing objects in a way that they can be sent to another instance via Concert */
class CONCERTSYNCCORE_API FConcertSyncObjectWriter : public FConcertIdentifierWriter
{
public:

	FConcertSyncObjectWriter(FConcertLocalIdentifierTable* InLocalIdentifierTable, UObject* InObj, TArray<uint8>& OutBytes, const bool InIncludeEditorOnlyData, const bool InSkipAssets, const FConcertSyncRemapObjectPath& InRemapDelegate);
	FConcertSyncObjectWriter(FConcertLocalIdentifierTable* InLocalIdentifierTable, UObject* InObj, TArray<uint8>& OutBytes, const bool InIncludeEditorOnlyData, const bool InSkipAssets);

	void SerializeObject(const UObject* InObject, const TArray<const FProperty*>* InPropertiesToWrite = nullptr, bool bAllowOuters = false);
	void SerializeProperty(const FProperty* InProp, const UObject* InObject);

	using FConcertIdentifierWriter::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(UObject*& Obj) override;
	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	virtual FArchive& operator<<(FObjectPtr& Obj) override;
	virtual FArchive& operator<<(FSoftObjectPtr& AssetPtr) override;
	virtual FArchive& operator<<(FSoftObjectPath& AssetPtr) override;
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
	virtual FString GetArchiveName() const override;
	virtual bool ShouldSkipProperty(const FProperty* InProperty) const override;
	//~ End FArchive Interface

	void SetSerializeNestedObjects(bool bInSerializeNestedObjects)
	{
		bSerializeNestedObjects = bInSerializeNestedObjects;
	}
private:
	typedef TFunction<bool(const FProperty*)> FShouldSkipPropertyFunc;
	bool bSkipAssets;
	bool bSerializeNestedObjects = false;
	FString PackageName;
	FShouldSkipPropertyFunc ShouldSkipPropertyFunc;
	FConcertSyncRemapObjectPath RemapObjectPathDelegate;
	TSet<FString> CollectedObjects;
};

/** Archive for reading objects that have been received from another instance via Concert */
class CONCERTSYNCCORE_API FConcertSyncObjectReader : public FConcertIdentifierReader
{
public:
	FConcertSyncObjectReader(const FConcertLocalIdentifierTable* InLocalIdentifierTable, FConcertSyncWorldRemapper InWorldRemapper, const FConcertSessionVersionInfo* InVersionInfo, UObject* InObj, const TArray<uint8>& InBytes, const FConcertSyncEncounteredMissingObject& InEncounteredMissingObjectDelegate);
	FConcertSyncObjectReader(const FConcertLocalIdentifierTable* InLocalIdentifierTable, FConcertSyncWorldRemapper InWorldRemapper, const FConcertSessionVersionInfo* InVersionInfo, UObject* InObj, const TArray<uint8>& InBytes);

	void SerializeObject(UObject* InObject);
	void SerializeProperty(const FProperty* InProp, UObject* InObject);

	using FConcertIdentifierReader::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(UObject*& Obj) override;
	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	virtual FArchive& operator<<(FObjectPtr& Obj) override;
	virtual FArchive& operator<<(FSoftObjectPtr& AssetPtr) override;
	virtual FArchive& operator<<(FSoftObjectPath& AssetPtr) override;
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
	virtual FString GetArchiveName() const override;
	//~ End FArchive Interface

	void SetSerializeNestedObjects(bool bInSerializeNestedObjects)
	{
		bSerializeNestedObjects = bInSerializeNestedObjects;
	}
protected:
	virtual void OnObjectSerialized(const FSoftObjectPath& Obj) {}

private:
	bool bSerializeNestedObjects = false;
	UObject* CurrentOuter = nullptr;
	FConcertSyncWorldRemapper WorldRemapper;
	FConcertSyncEncounteredMissingObject EncounteredMissingObjectDelegate;
};

/** Archive for rewriting identifiers (currently names) so that they belong to a different identifier table */
class CONCERTSYNCCORE_API FConcertSyncObjectRewriter : public FConcertIdentifierRewriter
{
public:
	FConcertSyncObjectRewriter(const FConcertLocalIdentifierTable* InLocalIdentifierTable, FConcertLocalIdentifierTable* InRewriteIdentifierTable, const FConcertSessionVersionInfo* InVersionInfo, TArray<uint8>& InBytes);

	void RewriteObject(const UClass* InClass);
	void RewriteProperty(const FProperty* InProp);

	using FConcertIdentifierRewriter::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(UObject*& Obj) override;
	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	virtual FArchive& operator<<(FObjectPtr& Obj) override;
	virtual FArchive& operator<<(FSoftObjectPtr& AssetPtr) override;
	virtual FArchive& operator<<(FSoftObjectPath& AssetPtr) override;
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
	virtual FString GetArchiveName() const override;
	//~ End FArchive Interface

protected:
	virtual void OnObjectSerialized(FSoftObjectPath& Obj) {}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
