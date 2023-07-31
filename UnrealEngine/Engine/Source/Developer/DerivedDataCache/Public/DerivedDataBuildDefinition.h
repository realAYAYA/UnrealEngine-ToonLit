// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "CoreTypes.h"
#include "DerivedDataSharedStringFwd.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

#define UE_API DERIVEDDATACACHE_API

class FCbObject;
class FCbWriter;
struct FGuid;
struct FIoHash;

namespace UE::DerivedData { class FBuildDefinition; }
namespace UE::DerivedData { class FBuildDefinitionBuilder; }
namespace UE::DerivedData { class FOptionalBuildDefinition; }
namespace UE::DerivedData { struct FBuildKey; }
namespace UE::DerivedData { struct FBuildValueKey; }

namespace UE::DerivedData::Private
{

class IBuildDefinitionInternal
{
public:
	virtual ~IBuildDefinitionInternal() = default;
	virtual const FBuildKey& GetKey() const = 0;
	virtual const FSharedString& GetName() const = 0;
	virtual const FUtf8SharedString& GetFunction() const = 0;
	virtual bool HasConstants() const = 0;
	virtual bool HasInputs() const = 0;
	virtual void IterateConstants(TFunctionRef<void (FUtf8StringView Key, FCbObject&& Value)> Visitor) const = 0;
	virtual void IterateInputBuilds(TFunctionRef<void (FUtf8StringView Key, const FBuildValueKey& ValueKey)> Visitor) const = 0;
	virtual void IterateInputBulkData(TFunctionRef<void (FUtf8StringView Key, const FGuid& BulkDataId)> Visitor) const = 0;
	virtual void IterateInputFiles(TFunctionRef<void (FUtf8StringView Key, FUtf8StringView Path)> Visitor) const = 0;
	virtual void IterateInputHashes(TFunctionRef<void (FUtf8StringView Key, const FIoHash& RawHash)> Visitor) const = 0;
	virtual void Save(FCbWriter& Writer) const = 0;
	virtual void AddRef() const = 0;
	virtual void Release() const = 0;
};

FBuildDefinition CreateBuildDefinition(IBuildDefinitionInternal* Definition);

class IBuildDefinitionBuilderInternal
{
public:
	virtual ~IBuildDefinitionBuilderInternal() = default;
	virtual void AddConstant(FUtf8StringView Key, const FCbObject& Value) = 0;
	virtual void AddInputBuild(FUtf8StringView Key, const FBuildValueKey& ValueKey) = 0;
	virtual void AddInputBulkData(FUtf8StringView Key, const FGuid& BulkDataId) = 0;
	virtual void AddInputFile(FUtf8StringView Key, FUtf8StringView Path) = 0;
	virtual void AddInputHash(FUtf8StringView Key, const FIoHash& RawHash) = 0;
	virtual FBuildDefinition Build() = 0;
};

FBuildDefinitionBuilder CreateBuildDefinitionBuilder(IBuildDefinitionBuilderInternal* DefinitionBuilder);

} // UE::DerivedData::Private

namespace UE::DerivedData
{

/**
 * A build definition is an immutable reference to a build function and its inputs.
 *
 * The purpose of a definition is to capture everything required to execute a derived data build.
 * The definition is partly fixed (function name, constants, input hashes) and is partly variable
 * (function version, build dependencies, bulk data, files), and the variable components mean the
 * output from building the definition can vary depending on the versions of those components.
 *
 * The key for the definition uniquely identifies the definition using the hash of the serialized
 * compact binary representation of the definition.
 *
 * The keys for constants and inputs are names that are unique within the build definition.
 *
 * To build a definition against a specific version of the function and inputs, queue it to build
 * on a build session, which uses its build input provider to convert it to a build action.
 *
 * @see FBuildDefinitionBuilder
 * @see FBuildSession
 */
class FBuildDefinition
{
public:
	/** Returns the key that uniquely identifies this build definition. */
	inline const FBuildKey& GetKey() const { return Definition->GetKey(); }

	/** Returns the name by which to identify this definition for logging and profiling. */
	inline const FSharedString& GetName() const { return Definition->GetName(); }

	/** Returns the name of the build function with which to build this definition. */
	inline const FUtf8SharedString& GetFunction() const { return Definition->GetFunction(); }

	/** Returns whether the definition has any constants. */
	inline bool HasConstants() const { return Definition->HasConstants(); }

	/** Returns whether the definition has any inputs. */
	inline bool HasInputs() const { return Definition->HasInputs(); }

	/** Visits every constant in order by key. The key view is valid for the lifetime of the definition. */
	inline void IterateConstants(TFunctionRef<void (FUtf8StringView Key, FCbObject&& Value)> Visitor) const
	{
		return Definition->IterateConstants(Visitor);
	}

	/** Visits every input build value in order by key. The key view is valid for the lifetime of the definition. */
	inline void IterateInputBuilds(TFunctionRef<void (FUtf8StringView Key, const FBuildValueKey& ValueKey)> Visitor) const
	{
		return Definition->IterateInputBuilds(Visitor);
	}

	/** Visits every input bulk data in order by key. The key view is valid for the lifetime of the definition. */
	inline void IterateInputBulkData(TFunctionRef<void (FUtf8StringView Key, const FGuid& BulkDataId)> Visitor) const
	{
		return Definition->IterateInputBulkData(Visitor);
	}

	/** Visits every input file in order by key. The key and path views are valid for the lifetime of the definition. */
	inline void IterateInputFiles(TFunctionRef<void (FUtf8StringView Key, FUtf8StringView Path)> Visitor) const
	{
		return Definition->IterateInputFiles(Visitor);
	}

	/** Visits every input hash in order by key. The key view is valid for the lifetime of the definition. */
	inline void IterateInputHashes(TFunctionRef<void (FUtf8StringView Key, const FIoHash& RawHash)> Visitor) const
	{
		return Definition->IterateInputHashes(Visitor);
	}

	/** Saves the build definition to a compact binary object. Calls BeginObject and EndObject. */
	inline void Save(FCbWriter& Writer) const
	{
		return Definition->Save(Writer);
	}

	/**
	 * Load a build definition from compact binary.
	 *
	 * @param Name         The name by which to identify this definition for logging and profiling.
	 * @param Definition   An object saved from a build definition. Holds a reference and is cloned if not owned.
	 * @return A valid build definition, or null on error.
	 */
	UE_API static FOptionalBuildDefinition Load(const FSharedString& Name, FCbObject&& Definition);

private:
	friend class FOptionalBuildDefinition;
	friend FBuildDefinition Private::CreateBuildDefinition(Private::IBuildDefinitionInternal* Definition);

	/** Construct a build definition. Use Build() on a builder from IBuild::CreateDefinition(). */
	inline explicit FBuildDefinition(Private::IBuildDefinitionInternal* InDefinition)
		: Definition(InDefinition)
	{
	}

	TRefCountPtr<Private::IBuildDefinitionInternal> Definition;
};

/**
 * A build definition builder is used to construct a build definition.
 *
 * Create using IBuild::CreateDefinition() which must be given a build function name.
 *
 * @see FBuildDefinition
 */
class FBuildDefinitionBuilder
{
public:
	/** Add a constant object with a key that is unique within this definition. */
	inline void AddConstant(FUtf8StringView Key, const FCbObject& Value)
	{
		DefinitionBuilder->AddConstant(Key, Value);
	}

	/** Add a value from another build with a key that is unique within this definition. */
	inline void AddInputBuild(FUtf8StringView Key, const FBuildValueKey& ValueKey)
	{
		DefinitionBuilder->AddInputBuild(Key, ValueKey);
	}

	/**
	 * Add a bulk data input with a key that is unique within this definition.
	 *
	 * @param BulkDataId   Identifier that uniquely identifies this data in the IBuildInputResolver.
	 */
	inline void AddInputBulkData(FUtf8StringView Key, const FGuid& BulkDataId)
	{
		DefinitionBuilder->AddInputBulkData(Key, BulkDataId);
	}

	/**
	 * Add a file input with a key that is unique within this definition.
	 *
	 * @param Path   Path to the file relative to a mounted content root.
	 */
	inline void AddInputFile(FUtf8StringView Key, FUtf8StringView Path)
	{
		DefinitionBuilder->AddInputFile(Key, Path);
	}

	/**
	 * Add a hash input with a key that is unique within this definition.
	 *
	 * @param RawHash   Hash of the raw data that will resolve it in the IBuildInputResolver.
	 */
	inline void AddInputHash(FUtf8StringView Key, const FIoHash& RawHash)
	{
		DefinitionBuilder->AddInputHash(Key, RawHash);
	}

	/**
	 * Build a build definition, which makes this builder subsequently unusable.
	 */
	inline FBuildDefinition Build()
	{
		ON_SCOPE_EXIT { DefinitionBuilder = nullptr; };
		return DefinitionBuilder->Build();
	}

private:
	friend FBuildDefinitionBuilder Private::CreateBuildDefinitionBuilder(Private::IBuildDefinitionBuilderInternal* DefinitionBuilder);

	/** Construct a build definition builder. Use IBuild::CreateDefinition(). */
	inline explicit FBuildDefinitionBuilder(Private::IBuildDefinitionBuilderInternal* InDefinitionBuilder)
		: DefinitionBuilder(InDefinitionBuilder)
	{
	}

	TUniquePtr<Private::IBuildDefinitionBuilderInternal> DefinitionBuilder;
};

/**
 * A build definition that can be null.
 *
 * @see FBuildDefinition
 */
class FOptionalBuildDefinition : private FBuildDefinition
{
public:
	inline FOptionalBuildDefinition() : FBuildDefinition(nullptr) {}

	inline FOptionalBuildDefinition(FBuildDefinition&& InDefinition) : FBuildDefinition(MoveTemp(InDefinition)) {}
	inline FOptionalBuildDefinition(const FBuildDefinition& InDefinition) : FBuildDefinition(InDefinition) {}
	inline FOptionalBuildDefinition& operator=(FBuildDefinition&& InDefinition) { FBuildDefinition::operator=(MoveTemp(InDefinition)); return *this; }
	inline FOptionalBuildDefinition& operator=(const FBuildDefinition& InDefinition) { FBuildDefinition::operator=(InDefinition); return *this; }

	/** Returns the build definition. The caller must check for null before using this accessor. */
	inline const FBuildDefinition& Get() const & { return *this; }
	inline FBuildDefinition Get() && { return MoveTemp(*this); }

	inline bool IsNull() const { return !IsValid(); }
	inline bool IsValid() const { return Definition.IsValid(); }
	inline explicit operator bool() const { return IsValid(); }

	inline void Reset() { *this = FOptionalBuildDefinition(); }
};

} // UE::DerivedData

#undef UE_API
