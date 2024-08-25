// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/Transform.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Containers/StringView.h"
#include "CoreTypes.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/ThreadSingleton.h"
#include "Misc/CString.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/StructuredArchive.h"
#include "Templates/Function.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

class FArchive;
class FOutputDevice;
struct FPropertyTag;
struct FUObjectSerializeContext;

/**
 * A struct that contains a string reference to an object, either a package, a top level asset or a subobject.
 * This can be used to make soft references to assets that are loaded on demand.
 * This is stored internally as an FTopLevelAssetPath pointing to the top level asset (/package/path.assetname) and an optional string subobject path.
 * If the MetaClass metadata is applied to a FProperty with this the UI will restrict to that type of asset.
 */
struct FSoftObjectPath
{
	FSoftObjectPath() = default;
	FSoftObjectPath(const FSoftObjectPath& Other) = default;
	FSoftObjectPath(FSoftObjectPath&& Other) = default;
	~FSoftObjectPath() = default;
	FSoftObjectPath& operator=(const FSoftObjectPath& Path) = default;
	FSoftObjectPath& operator=(FSoftObjectPath&& Path) = default;

	/** Construct from a path string. Non-explicit for backwards compatibility. */
	FSoftObjectPath(const FString& Path)						{ SetPath(FStringView(Path)); }
	FSoftObjectPath(FTopLevelAssetPath InAssetPath, FString InSubPathString)
	{
		SetPath(InAssetPath, MoveTemp(InSubPathString));
	}
	FSoftObjectPath(FName InPackageName, FName InAssetName, FString InSubPathString)
	{
		SetPath(FTopLevelAssetPath(InPackageName, InAssetName), MoveTemp(InSubPathString));
	}
	/** Explicitly extend a top-level object path with an empty subobject path. */
	explicit FSoftObjectPath(FTopLevelAssetPath InAssetPath)	{ SetPath(InAssetPath, FString()); }
	explicit FSoftObjectPath(FWideStringView Path)				{ SetPath(Path); }
	explicit FSoftObjectPath(FAnsiStringView Path)				{ SetPath(Path); }
	explicit FSoftObjectPath(const WIDECHAR* Path)				{ SetPath(FWideStringView(Path)); }
	explicit FSoftObjectPath(const ANSICHAR* Path)				{ SetPath(FAnsiStringView(Path)); }
	explicit FSoftObjectPath(TYPE_OF_NULLPTR)					{}

	UE_DEPRECATED(5.1, "Asset path FNames have been deprecated. This constructor should be used only temporarily to fix up old codepaths that produce an FName.")
	explicit FSoftObjectPath(FName Path) 
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS;
		SetPath(Path); 
PRAGMA_ENABLE_DEPRECATION_WARNINGS;
	}

	/** Construct from an asset FName and subobject pair */
	UE_DEPRECATED(5.1, "Asset path FNames have been deprecated. This constructor should be used only temporarily to fix up old codepaths that produce an FName.")
	COREUOBJECT_API FSoftObjectPath(FName InAssetPathName, FString InSubPathString);
	
	template <typename T>
	FSoftObjectPath(const TObjectPtr<T>& InObject)
	{
		SetPath(InObject.GetPathName());
	}

	FSoftObjectPath(const FObjectPtr& InObject)
	{
		SetPath(InObject.GetPathName());
	}

	/** Construct from an existing object in memory */
	FSoftObjectPath(const UObject* InObject)
	{
		if (InObject)
		{
			SetPath(InObject->GetPathName());
		}
	}

	FSoftObjectPath& operator=(const FTopLevelAssetPath Path)			{ SetPath(Path); return *this; }
	FSoftObjectPath& operator=(const FString& Path)						{ SetPath(FStringView(Path)); return *this; }
	FSoftObjectPath& operator=(FWideStringView Path)					{ SetPath(Path); return *this; }
	FSoftObjectPath& operator=(FAnsiStringView Path)					{ SetPath(Path); return *this; }
	FSoftObjectPath& operator=(const WIDECHAR* Path)					{ SetPath(FWideStringView(Path)); return *this; }
	FSoftObjectPath& operator=(const ANSICHAR* Path)					{ SetPath(FAnsiStringView(Path)); return *this; }
	FSoftObjectPath& operator=(TYPE_OF_NULLPTR)							{ Reset(); return *this; }

	UE_DEPRECATED(5.1, "Asset path FNames have been deprecated. This assignment operator should be used only temporarily to fix up old codepaths that produce an FName.")
	FSoftObjectPath& operator=(FName Path) 
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS;
		SetPath(Path); 
PRAGMA_ENABLE_DEPRECATION_WARNINGS;
		return *this;
	}

	/** Returns string representation of reference, in form /package/path.assetname[:subpath] */
	COREUOBJECT_API FString ToString() const;

	UE_DEPRECATED(5.1, "Asset path FNames have been deprecated. This function should be used only temporarily to interface with old APIs that require an FName.")
	FName ToFName() const { return *ToString(); }

	/** Append string representation of reference, in form /package/path.assetname[:subpath] */
	COREUOBJECT_API void ToString(FStringBuilderBase& Builder) const;

	/** Append string representation of reference, in form /package/path.assetname[:subpath] */
	COREUOBJECT_API void AppendString(FString& Builder) const;
	COREUOBJECT_API void AppendString(FStringBuilderBase& Builder) const;

	/** Returns the top-level asset part of this path, without the subobject path. */
	FTopLevelAssetPath GetAssetPath() const
	{
		return AssetPath;
	}

	/** Returns this path without the SubPath component, restricting the result to a top level asset but keeping the type as FSoftObjectPath in contrast to GetAssetPath. */
	FSoftObjectPath GetWithoutSubPath() const
	{
		return FSoftObjectPath(AssetPath, {});
	}
	
	/** Returns the entire asset path as an FName, including both package and asset but not sub object */
	UE_DEPRECATED(5.1, "Asset path FNames have been deprecated. Use GetAssetPath instead.")
	COREUOBJECT_API FName GetAssetPathName() const;
	
	UE_DEPRECATED(5.1, "Asset path FNames have been deprecated. Use SetAssetPath instead.")
	COREUOBJECT_API void SetAssetPathName(FName InAssetPathName);

	/** Returns string version of asset path, including both package and asset but not sub object */
	FORCEINLINE FString GetAssetPathString() const
	{
		if (AssetPath.IsNull())
		{
			return FString();
		}

		return AssetPath.ToString();
	}

	/** Returns the sub path, which is often empty */
	FORCEINLINE const FString& GetSubPathString() const
	{
		return SubPathString;
	}

	FORCEINLINE void SetSubPathString(const FString& InSubPathString)
	{
		SubPathString = InSubPathString;
	}

	/** Returns /package/path, leaving off the asset name and sub object */
	FString GetLongPackageName() const
	{
		FName PackageName = GetAssetPath().GetPackageName();
		return PackageName.IsNone() ? FString() : PackageName.ToString();
	}

	/** Returns /package/path, leaving off the asset name and sub object */
	FName GetLongPackageFName() const
	{
		return GetAssetPath().GetPackageName();
	}

	/** Returns assetname string, leaving off the /package/path part and sub object */
	FString GetAssetName() const
	{
		FName AssetName = GetAssetPath().GetAssetName();
		return AssetName.IsNone() ? FString() : AssetName.ToString();
	}

	/** Returns assetname string, leaving off the /package/path part and sub object */
	FName GetAssetFName() const
	{
		return GetAssetPath().GetAssetName();
	}

	/** Sets asset path of this reference based on a string path */
	COREUOBJECT_API void SetPath(const FTopLevelAssetPath& InAssetPath, FString InSubPathString = FString());
	COREUOBJECT_API void SetPath(FWideStringView Path);
	COREUOBJECT_API void SetPath(FAnsiStringView Path);
	COREUOBJECT_API void SetPath(FUtf8StringView Path);
	UE_DEPRECATED(5.1, "Asset path FNames have been deprecated. This function should be used only temporarily to fix up old codepaths that produce an FName.")
	COREUOBJECT_API void SetPath(FName Path);
	void SetPath(const WIDECHAR* Path)			{ SetPath(FWideStringView(Path)); }
	void SetPath(const ANSICHAR* Path)			{ SetPath(FAnsiStringView(Path)); }
	void SetPath(const FString& Path)			{ SetPath(FStringView(Path)); }

	/**
	 * Attempts to load the asset, this will call LoadObject which can be very slow
	 * @param InLoadContext Optional load context when called from nested load callstack
	 * @return Loaded UObject, or nullptr if the reference is null or the asset fails to load
	 */
	COREUOBJECT_API UObject* TryLoad(FUObjectSerializeContext* InLoadContext = nullptr) const;

	/**
	 * Attempts to find a currently loaded object that matches this path
	 *
	 * @return Found UObject, or nullptr if not currently in memory
	 */
	COREUOBJECT_API UObject* ResolveObject() const;

	/** Resets reference to point to null */
	void Reset()
	{		
		AssetPath.Reset();
		SubPathString.Reset();
	}
	
	/** Check if this could possibly refer to a real object, or was initialized to null */
	FORCEINLINE bool IsValid() const
	{
		return AssetPath.IsValid();
	}

	/** Checks to see if this is initialized to null */
	FORCEINLINE bool IsNull() const
	{
		return AssetPath.IsNull();
	}

	/** Check if this represents an asset, meaning it is not null but does not have a sub path */
	FORCEINLINE bool IsAsset() const
	{
		return !AssetPath.IsNull() && SubPathString.IsEmpty();
	}

	/** Check if this represents a sub object, meaning it has a sub path */
	FORCEINLINE bool IsSubobject() const
	{
		return !AssetPath.IsNull() && !SubPathString.IsEmpty();
	}

	/** Return true if this path appears before Other in lexical order */
	bool LexicalLess(const FSoftObjectPath& Other) const
	{
		int32 PathCompare = AssetPath.Compare(Other.AssetPath);
		if (PathCompare != 0)
		{
			return PathCompare < 0;
		}
		return SubPathString.Compare(Other.SubPathString) < 0; 
	}

	/** Return true if this path appears before Other using fast index-based fname order */
	bool FastLess(const FSoftObjectPath& Other) const
	{
		int32 PathCompare = AssetPath.CompareFast(Other.AssetPath);
		if (PathCompare != 0)
		{
			return PathCompare < 0;
		}
		return SubPathString.Compare(Other.SubPathString) < 0; 
	}

	/** Struct overrides */
	COREUOBJECT_API bool Serialize(FArchive& Ar);
	COREUOBJECT_API bool Serialize(FStructuredArchive::FSlot Slot);
	COREUOBJECT_API bool operator==(FSoftObjectPath const& Other) const;
	bool operator!=(FSoftObjectPath const& Other) const
	{
		return !(*this == Other);
	}

	COREUOBJECT_API bool ExportTextItem(FString& ValueStr, FSoftObjectPath const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	COREUOBJECT_API bool ImportTextItem( const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText, FArchive* InSerializingArchive = nullptr );
	COREUOBJECT_API bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FStructuredArchive::FSlot Slot);

	/** Serializes the internal path and also handles save/PIE fixups. Call this from the archiver overrides */
	COREUOBJECT_API void SerializePath(FArchive& Ar);

	/** Serializes the internal path without any save/PIE fixups. Only call this directly if you know what you are doing */
	COREUOBJECT_API void SerializePathWithoutFixup(FArchive& Ar);

	/** Fixes up path for saving, call if saving with a method that skips SerializePath. This can modify the path, it will return true if it was modified */
	COREUOBJECT_API bool PreSavePath(bool* bReportSoftObjectPathRedirects = nullptr);

	/** 
	 * Handles when a path has been loaded, call if loading with a method that skips SerializePath. This does not modify path but might call callbacks
	 * @param InArchive The archive that loaded this path
	 */
	COREUOBJECT_API void PostLoadPath(FArchive* InArchive) const;

	/** Fixes up this SoftObjectPath to add the PIE prefix depending on what is currently active, returns true if it was modified. The overload that takes an explicit PIE instance is preferred, if it's available. */
	COREUOBJECT_API bool FixupForPIE(TFunctionRef<void(int32, FSoftObjectPath&)> InPreFixupForPIECustomFunction = [](int32, FSoftObjectPath&) {});

	/** Fixes up this SoftObjectPath to add the PIE prefix for the given PIEInstance index, returns true if it was modified */
	COREUOBJECT_API bool FixupForPIE(int32 PIEInstance, TFunctionRef<void(int32, FSoftObjectPath&)> InPreFixupForPIECustomFunction = [](int32, FSoftObjectPath&) {});

	/** Fixes soft object path for CoreRedirects to handle renamed native objects, returns true if it was modified */
	COREUOBJECT_API bool FixupCoreRedirects();

	FORCEINLINE friend uint32 GetTypeHash(FSoftObjectPath const& This)
	{
		uint32 Hash = 0;

		Hash = HashCombine(Hash, GetTypeHash(This.AssetPath));
		Hash = HashCombine(Hash, GetTypeHash(This.SubPathString));
		return Hash;
	}

	UE_DEPRECATED(5.4, "The current object tag is no longer used by TSoftObjectPtr, you can remove all calls")
	static int32 GetCurrentTag()
	{
		return 0;
	}
	UE_DEPRECATED(5.4, "The current object tag is no longer used by TSoftObjectPtr, you can remove all calls")
	static int32 InvalidateTag()
	{
		return 0;
	}
	static COREUOBJECT_API FSoftObjectPath GetOrCreateIDForObject(const UObject* Object);
	
	/** Adds list of packages names that have been created specifically for PIE, this is used for editor fixup */
	static COREUOBJECT_API void AddPIEPackageName(FName NewPIEPackageName);
	
	/** Disables special PIE path handling, call when PIE finishes to clear list */
	static COREUOBJECT_API void ClearPIEPackageNames();

private:
	/** Asset path, patch to a top level object in a package. This is /package/path.assetname */
	FTopLevelAssetPath AssetPath;

	/** Optional FString for subobject within an asset. This is the sub path after the : */
	FString SubPathString;

	/** Package names currently being duplicated, needed by FixupForPIE */
	static COREUOBJECT_API TSet<FName> PIEPackageNames;

	COREUOBJECT_API UObject* ResolveObjectInternal() const;
	COREUOBJECT_API UObject* ResolveObjectInternal(const TCHAR* PathString) const;

	friend struct Z_Construct_UScriptStruct_FSoftObjectPath_Statics;
};

/** Fast non-alphabetical order that is only stable during this process' lifetime. */
struct FSoftObjectPathFastLess
{
	bool operator()(const FSoftObjectPath& Lhs, const FSoftObjectPath& Rhs) const
	{
		return Lhs.FastLess(Rhs);
	}
};

/** Slow alphabetical order that is stable / deterministic over process runs. */
struct FSoftObjectPathLexicalLess
{
	bool operator()(const FSoftObjectPath& Lhs, const FSoftObjectPath& Rhs) const
	{
		return Lhs.LexicalLess(Rhs);
	}
};

inline FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FSoftObjectPath& Path)
{
	Path.ToString(Builder);
	return Builder;
}

/**
 * A struct that contains a string reference to a class, can be used to make soft references to classes
 */
struct FSoftClassPath : public FSoftObjectPath
{
	FSoftClassPath() = default;
	FSoftClassPath(const FSoftClassPath& Other) = default;
	FSoftClassPath(FSoftClassPath&& Other) = default;
	~FSoftClassPath() = default;
	FSoftClassPath& operator=(const FSoftClassPath& Path) = default;
	FSoftClassPath& operator=(FSoftClassPath&& Path) = default;

	/**
	 * Construct from a path string
	 */
	FSoftClassPath(const FString& PathString)
		: FSoftObjectPath(PathString)
	{ }

	/**
	 * Construct from an existing class, will do some string processing
	 */
	FSoftClassPath(const UClass* InClass)
		: FSoftObjectPath(InClass)
	{ }

	/**
	* Attempts to load the class.
	* @return Loaded UObject, or null if the class fails to load, or if the reference is not valid.
	*/
	template<typename T>
	UClass* TryLoadClass() const
	{
		if ( IsValid() )
		{
			return LoadClass<T>(nullptr, *ToString(), nullptr, LOAD_None, nullptr);
		}

		return nullptr;
	}

	/**
	 * Attempts to find a currently loaded object that matches this object ID
	 * @return Found UClass, or NULL if not currently loaded
	 */
	COREUOBJECT_API UClass* ResolveClass() const;

	COREUOBJECT_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

	static COREUOBJECT_API FSoftClassPath GetOrCreateIDForClass(const UClass *InClass);

private:
	/** Forbid creation for UObject. This class is for UClass only. Use FSoftObjectPath instead. */
	FSoftClassPath(const UObject* InObject) { }

	/** Forbidden. This class is for UClass only. Use FSoftObjectPath instead. */
	static COREUOBJECT_API FSoftObjectPath GetOrCreateIDForObject(const UObject *Object);
};

UE_DEPRECATED(5.0, "FStringAssetReference was renamed to FSoftObjectPath as it is now not always a string and can also refer to a subobject")
typedef FSoftObjectPath FStringAssetReference;

UE_DEPRECATED(5.0, "FStringClassReference was renamed to FSoftClassPath")
typedef FSoftClassPath FStringClassReference;

/** Options for how to set soft object path collection */
enum class ESoftObjectPathCollectType : uint8
{
	/** The SoftObjectPath being loaded is not in a package, so we do not need to record it in inclusion or exclusion lists*/
	NonPackage,
	/** References is not tracked in any situation, transient reference */
	NeverCollect,
	/** Editor only reference, this is tracked for redirector fixup but not for cooking */
	EditorOnlyCollect,
	/** Game reference, this is gathered for both redirector fixup and cooking */
	AlwaysCollect,
};

/** Rules for actually serializing the internals of soft object paths */
enum class ESoftObjectPathSerializeType : uint8
{
	/** Never serialize the raw names */
	NeverSerialize,
	/** Only serialize if the archive has no size */
	SkipSerializeIfArchiveHasSize,
	/** Always serialize the soft object path internals */
	AlwaysSerialize,
};

class COREUOBJECT_API FSoftObjectPathThreadContext : public TThreadSingleton<FSoftObjectPathThreadContext>
{
	friend TThreadSingleton<FSoftObjectPathThreadContext>;
	friend struct FSoftObjectPathSerializationScope;

	FSoftObjectPathThreadContext() {}

	struct FSerializationOptions
	{
		FName PackageName;
		FName PropertyName;
		ESoftObjectPathCollectType CollectType;
		ESoftObjectPathSerializeType SerializeType;

		FSerializationOptions() : CollectType(ESoftObjectPathCollectType::AlwaysCollect) {}
		FSerializationOptions(FName InPackageName, FName InPropertyName, ESoftObjectPathCollectType InCollectType, ESoftObjectPathSerializeType InSerializeType) : PackageName(InPackageName), PropertyName(InPropertyName), CollectType(InCollectType), SerializeType(InSerializeType) {}
	};

	TArray<FSerializationOptions> OptionStack;
public:
	/** 
	 * Returns the current serialization options that were added using SerializationScope or LinkerLoad
	 *
	 * @param OutPackageName Package that this string asset belongs to
	 * @param OutPropertyName Property that this path belongs to
	 * @param OutCollectType Type of collecting that should be done
	 * @param Archive The FArchive that is serializing this path if known. If null it will check FUObjectThreadContext
	 */
	bool GetSerializationOptions(FName& OutPackageName, FName& OutPropertyName, ESoftObjectPathCollectType& OutCollectType, ESoftObjectPathSerializeType& OutSerializeType, FArchive* Archive = nullptr) const;
};

/** Helper class to set and restore serialization options for soft object paths */
struct FSoftObjectPathSerializationScope
{
	/** 
	 * Create a new serialization scope, which affects the way that soft object paths are saved
	 *
	 * @param SerializingPackageName Package that this string asset belongs to
	 * @param SerializingPropertyName Property that this path belongs to
	 * @param CollectType Set type of collecting that should be done, can be used to disable tracking entirely
	 */
	FSoftObjectPathSerializationScope(FName SerializingPackageName, FName SerializingPropertyName, ESoftObjectPathCollectType CollectType, ESoftObjectPathSerializeType SerializeType)
	{
		FSoftObjectPathThreadContext::Get().OptionStack.Emplace(SerializingPackageName, SerializingPropertyName, CollectType, SerializeType);
	}

	explicit FSoftObjectPathSerializationScope(ESoftObjectPathCollectType CollectType)
	{
		FSoftObjectPathThreadContext::Get().OptionStack.Emplace(NAME_None, NAME_None, CollectType, ESoftObjectPathSerializeType::AlwaysSerialize);
	}

	~FSoftObjectPathSerializationScope()
	{
		FSoftObjectPathThreadContext::Get().OptionStack.Pop();
	}
};

/** Structure for file paths that are displayed in the editor with a picker UI. */
struct FFilePath
{
	/**
	 * The path to the file.
	 */
	FString FilePath;
};

/** Structure for directory paths that are displayed in the editor with a picker UI. */
struct FDirectoryPath
{
	/**
	 * The path to the directory.
	 */
	FString Path;
};

// Fixup archive
struct FSoftObjectPathFixupArchive : public FArchiveUObject
{
	FSoftObjectPathFixupArchive(TFunction<void(FSoftObjectPath&)> InFixupFunction)
		: FixupFunction(InFixupFunction)
	{
		this->SetIsSaving(true);
		this->ArShouldSkipBulkData = true;
		this->SetShouldSkipCompilingAssets(true);
	}

	FSoftObjectPathFixupArchive(const FString& InOldAssetPathString, const FString& InNewAssetPathString)
		: FSoftObjectPathFixupArchive([OldAssetPathString = InOldAssetPathString, NewAssetPathString = InNewAssetPathString](FSoftObjectPath& Value)
		{
			if (!Value.IsNull() && Value.GetAssetPathString().Equals(OldAssetPathString, ESearchCase::IgnoreCase))
			{
				Value = FSoftObjectPath(NewAssetPathString);
			}
		})
	{
	}

	FArchive& operator<<(FSoftObjectPath& Value) override
	{
		FixupFunction(Value);
		return *this;
	}

	virtual FArchive& operator<<(FObjectPtr& Value) override
	{
		//do nothing to avoid resolving
		return *this;
	}

	void Fixup(UObject* Root)
	{
		Root->Serialize(*this);
		TArray<UObject*> SubObjects;
		GetObjectsWithOuter(Root, SubObjects);
		for (UObject* Obj : SubObjects)
		{
			Obj->Serialize(*this);
		}
	}

	TFunction<void(FSoftObjectPath&)> FixupFunction;
};

namespace UE::SoftObjectPath::Private
{
	UE_DEPRECATED(5.1, "This function is only for use in fixing up deprecated APIs.")
	inline TArray<FName> ConvertSoftObjectPaths(TConstArrayView<FSoftObjectPath> InPaths)
	{
		TArray<FName> Out;
		Out.Reserve(InPaths.Num());
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Algo::Transform(InPaths, Out, [](const FSoftObjectPath& Path) { return Path.ToFName(); });
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return Out;
	}

	UE_DEPRECATED(5.1, "This function is only for use in fixing up deprecated APIs.")
	inline TArray<FSoftObjectPath> ConvertObjectPathNames(TConstArrayView<FName> InPaths)
	{
		TArray<FSoftObjectPath> Out;
		Out.Reserve(InPaths.Num());
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Algo::Transform(InPaths, Out, [](FName Name) { return FSoftObjectPath(Name); });
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return Out;
	}
}

#if WITH_LOW_LEVEL_TESTS
#include <ostream>
COREUOBJECT_API std::ostream& operator<<(std::ostream& Stream, const FSoftObjectPath& Value);
#endif

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
