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

class FCompressedBuffer;

namespace UE::DerivedData { class FBuildInputs; }
namespace UE::DerivedData { class FBuildInputsBuilder; }

namespace UE::DerivedData::Private
{

class IBuildInputsInternal
{
public:
	virtual ~IBuildInputsInternal() = default;
	virtual const FSharedString& GetName() const = 0;
	virtual const FCompressedBuffer& FindInput(FUtf8StringView Key) const = 0;
	virtual void IterateInputs(TFunctionRef<void (FUtf8StringView Key, const FCompressedBuffer& Buffer)> Visitor) const = 0;
	virtual void AddRef() const = 0;
	virtual void Release() const = 0;
};

FBuildInputs CreateBuildInputs(IBuildInputsInternal* Input);

class IBuildInputsBuilderInternal
{
public:
	virtual ~IBuildInputsBuilderInternal() = default;
	virtual void AddInput(FUtf8StringView Key, const FCompressedBuffer& Buffer) = 0;
	virtual FBuildInputs Build() = 0;
};

FBuildInputsBuilder CreateBuildInputsBuilder(IBuildInputsBuilderInternal* InputsBuilder);

} // UE::DerivedData::Private

namespace UE::DerivedData
{

/**
 * Build inputs are an immutable container of key/value pairs for the inputs to a build function.
 *
 * The keys for inputs are names that are unique within the build inputs.
 *
 * @see FBuildAction
 */
class FBuildInputs
{
public:
	/** Returns the name by which to identify the inputs for logging and profiling. */
	inline const FSharedString& GetName() const { return Inputs->GetName(); }

	/** Finds an input by key, or a null buffer if not found. */
	inline const FCompressedBuffer& FindInput(FUtf8StringView Key) const { return Inputs->FindInput(Key); }

	/** Visits every input in order by key. The key view is valid for the lifetime of the inputs. */
	inline void IterateInputs(TFunctionRef<void (FUtf8StringView Key, const FCompressedBuffer& Buffer)> Visitor) const
	{
		Inputs->IterateInputs(Visitor);
	}

private:
	friend class FOptionalBuildInputs;
	friend FBuildInputs Private::CreateBuildInputs(Private::IBuildInputsInternal* Inputs);

	/** Construct build inputs. Use Build() on a builder from IBuild::CreateInputs(). */
	inline explicit FBuildInputs(Private::IBuildInputsInternal* InInputs)
		: Inputs(InInputs)
	{
	}

	TRefCountPtr<Private::IBuildInputsInternal> Inputs;
};

/**
 * A build inputs builder is used to construct build inputs.
 *
 * Create using IBuild::CreateInputs().
 *
 * @see FBuildInputs
 */
class FBuildInputsBuilder
{
public:
	/** Add an input with a key that is unique within this input. */
	inline void AddInput(FUtf8StringView Key, const FCompressedBuffer& Buffer)
	{
		InputsBuilder->AddInput(Key, Buffer);
	}

	/** Build build inputs, which makes this builder subsequently unusable. */
	inline FBuildInputs Build()
	{
		ON_SCOPE_EXIT { InputsBuilder = nullptr; };
		return InputsBuilder->Build();
	}

private:
	friend FBuildInputsBuilder Private::CreateBuildInputsBuilder(Private::IBuildInputsBuilderInternal* InputsBuilder);

	/** Construct a build inputs builder. Use IBuild::CreateInputs(). */
	inline explicit FBuildInputsBuilder(Private::IBuildInputsBuilderInternal* InInputsBuilder)
		: InputsBuilder(InInputsBuilder)
	{
	}

	TUniquePtr<Private::IBuildInputsBuilderInternal> InputsBuilder;
};

/**
 * Build inputs that can be null.
 *
 * @see FBuildInputs
 */
class FOptionalBuildInputs : private FBuildInputs
{
public:
	inline FOptionalBuildInputs() : FBuildInputs(nullptr) {}

	inline FOptionalBuildInputs(FBuildInputs&& InInputs) : FBuildInputs(MoveTemp(InInputs)) {}
	inline FOptionalBuildInputs(const FBuildInputs& InInputs) : FBuildInputs(InInputs) {}
	inline FOptionalBuildInputs& operator=(FBuildInputs&& InInputs) { FBuildInputs::operator=(MoveTemp(InInputs)); return *this; }
	inline FOptionalBuildInputs& operator=(const FBuildInputs& InInputs) { FBuildInputs::operator=(InInputs); return *this; }

	/** Returns the build inputs. The caller must check for null before using this accessor. */
	inline const FBuildInputs& Get() const & { return *this; }
	inline FBuildInputs Get() && { return MoveTemp(*this); }

	inline bool IsNull() const { return !IsValid(); }
	inline bool IsValid() const { return Inputs.IsValid(); }
	inline explicit operator bool() const { return IsValid(); }

	inline void Reset() { *this = FOptionalBuildInputs(); }
};

} // UE::DerivedData
