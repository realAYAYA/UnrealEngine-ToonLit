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

namespace UE::DerivedData { class FBuildAction; }
namespace UE::DerivedData { class FBuildActionBuilder; }
namespace UE::DerivedData { class FOptionalBuildAction; }
namespace UE::DerivedData { struct FBuildActionKey; }

namespace UE::DerivedData::Private
{

class IBuildActionInternal
{
public:
	virtual ~IBuildActionInternal() = default;
	virtual const FBuildActionKey& GetKey() const = 0;
	virtual const FSharedString& GetName() const = 0;
	virtual const FUtf8SharedString& GetFunction() const = 0;
	virtual const FGuid& GetFunctionVersion() const = 0;
	virtual const FGuid& GetBuildSystemVersion() const = 0;
	virtual bool HasConstants() const = 0;
	virtual bool HasInputs() const = 0;
	virtual void IterateConstants(TFunctionRef<void (FUtf8StringView Key, FCbObject&& Value)> Visitor) const = 0;
	virtual void IterateInputs(TFunctionRef<void (FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)> Visitor) const = 0;
	virtual void Save(FCbWriter& Writer) const = 0;
	virtual void AddRef() const = 0;
	virtual void Release() const = 0;
};

FBuildAction CreateBuildAction(IBuildActionInternal* Action);

class IBuildActionBuilderInternal
{
public:
	virtual ~IBuildActionBuilderInternal() = default;
	virtual void AddConstant(FUtf8StringView Key, const FCbObject& Value) = 0;
	virtual void AddInput(FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize) = 0;
	virtual FBuildAction Build() = 0;
};

FBuildActionBuilder CreateBuildActionBuilder(IBuildActionBuilderInternal* ActionBuilder);

} // UE::DerivedData::Private

namespace UE::DerivedData
{

/**
 * A build action is an immutable reference to a build function and its inputs.
 *
 * The purpose of an action is to capture everything required to execute a derived data build for
 * a fixed version of the build function and its constants and inputs.
 *
 * The key for the action uniquely identifies the action and is derived by hashing the serialized
 * compact binary representation of the action.
 *
 * The keys for constants and inputs are names that are unique within the build action.
 *
 * @see FBuildDefinition
 * @see FBuildSession
 */
class FBuildAction
{
public:
	/** Returns the key that uniquely identifies this build action. */
	inline const FBuildActionKey& GetKey() const { return Action->GetKey(); }

	/** Returns the name by which to identify this action for logging and profiling. */
	inline const FSharedString& GetName() const { return Action->GetName(); }

	/** Returns the name of the build function with which to build this action. */
	inline const FUtf8SharedString& GetFunction() const { return Action->GetFunction(); }

	/** Returns whether the action has any constants. */
	inline bool HasConstants() const { return Action->HasConstants(); }

	/** Returns whether the action has any inputs. */
	inline bool HasInputs() const { return Action->HasInputs(); }

	/** Returns the version of the build function with which to build this action. */
	inline const FGuid& GetFunctionVersion() const { return Action->GetFunctionVersion(); }

	/** Returns the version of the build system required to build this action. */
	inline const FGuid& GetBuildSystemVersion() const { return Action->GetBuildSystemVersion(); }

	/** Visits every constant in order by key. The key view is valid for the lifetime of the action. */
	inline void IterateConstants(TFunctionRef<void (FUtf8StringView Key, FCbObject&& Value)> Visitor) const
	{
		Action->IterateConstants(MoveTemp(Visitor));
	}

	/** Visits every input in order by key. The key view is valid for the lifetime of the action. */
	inline void IterateInputs(TFunctionRef<void (FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)> Visitor) const
	{
		Action->IterateInputs(MoveTemp(Visitor));
	}

	/** Saves the build action to a compact binary object. Calls BeginObject and EndObject. */
	inline void Save(FCbWriter& Writer) const
	{
		return Action->Save(Writer);
	}

	/**
	 * Load a build action from compact binary.
	 *
	 * @param Name     The name by which to identify this action for logging and profiling.
	 * @param Action   The saved action to load.
	 * @return A valid build action, or null on error.
	 */
	UE_API static FOptionalBuildAction Load(const FSharedString& Name, FCbObject&& Action);

private:
	friend class FOptionalBuildAction;
	friend FBuildAction Private::CreateBuildAction(Private::IBuildActionInternal* Action);

	/** Construct a build action. Use Build() on a builder from IBuild::CreateAction(). */
	inline explicit FBuildAction(Private::IBuildActionInternal* InAction)
		: Action(InAction)
	{
	}

	TRefCountPtr<Private::IBuildActionInternal> Action;
};

/**
 * A build action builder is used to construct a build action.
 *
 * Create using IBuild::CreateAction() which must be given a build function name.
 *
 * @see FBuildAction
 */
class FBuildActionBuilder
{
public:
	/** Add a constant object with a key that is unique within this action. */
	inline void AddConstant(FUtf8StringView Key, const FCbObject& Value)
	{
		ActionBuilder->AddConstant(Key, Value);
	}

	/** Add an input with a key that is unique within this action. */
	inline void AddInput(FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)
	{
		ActionBuilder->AddInput(Key, RawHash, RawSize);
	}

	/** Build a build action, which makes this builder subsequently unusable. */
	inline FBuildAction Build()
	{
		ON_SCOPE_EXIT { ActionBuilder = nullptr; };
		return ActionBuilder->Build();
	}

private:
	friend FBuildActionBuilder Private::CreateBuildActionBuilder(Private::IBuildActionBuilderInternal* ActionBuilder);

	/** Construct a build action builder. Use IBuild::CreateAction(). */
	inline explicit FBuildActionBuilder(Private::IBuildActionBuilderInternal* InActionBuilder)
		: ActionBuilder(InActionBuilder)
	{
	}

	TUniquePtr<Private::IBuildActionBuilderInternal> ActionBuilder;
};

/**
 * A build action that can be null.
 *
 * @see FBuildAction
 */
class FOptionalBuildAction : private FBuildAction
{
public:
	inline FOptionalBuildAction() : FBuildAction(nullptr) {}

	inline FOptionalBuildAction(FBuildAction&& InAction) : FBuildAction(MoveTemp(InAction)) {}
	inline FOptionalBuildAction(const FBuildAction& InAction) : FBuildAction(InAction) {}
	inline FOptionalBuildAction& operator=(FBuildAction&& InAction) { FBuildAction::operator=(MoveTemp(InAction)); return *this; }
	inline FOptionalBuildAction& operator=(const FBuildAction& InAction) { FBuildAction::operator=(InAction); return *this; }

	/** Returns the build action. The caller must check for null before using this accessor. */
	inline const FBuildAction& Get() const & { return *this; }
	inline FBuildAction Get() && { return MoveTemp(*this); }

	inline bool IsNull() const { return !IsValid(); }
	inline bool IsValid() const { return Action.IsValid(); }
	inline explicit operator bool() const { return IsValid(); }

	inline void Reset() { *this = FOptionalBuildAction(); }
};

} // UE::DerivedData

#undef UE_API
