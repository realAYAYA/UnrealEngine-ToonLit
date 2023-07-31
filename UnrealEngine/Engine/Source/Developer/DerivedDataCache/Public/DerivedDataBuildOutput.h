// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "CoreTypes.h"
#include "DerivedDataSharedStringFwd.h"
#include "Misc/ScopeExit.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

#define UE_API DERIVEDDATACACHE_API

class FCbObject;
class FCbWriter;

namespace UE::DerivedData { class FBuildOutput; }
namespace UE::DerivedData { class FBuildOutputBuilder; }
namespace UE::DerivedData { class FCacheRecord; }
namespace UE::DerivedData { class FCacheRecordBuilder; }
namespace UE::DerivedData { class FOptionalBuildOutput; }
namespace UE::DerivedData { class FValue; }
namespace UE::DerivedData { class FValueWithId; }
namespace UE::DerivedData { struct FBuildOutputLog; }
namespace UE::DerivedData { struct FBuildOutputMessage; }
namespace UE::DerivedData { struct FValueId; }

namespace UE::DerivedData::Private
{

class IBuildOutputInternal
{
public:
	virtual ~IBuildOutputInternal() = default;
	virtual const FSharedString& GetName() const = 0;
	virtual const FUtf8SharedString& GetFunction() const = 0;
	virtual const FCbObject& GetMeta() const = 0;
	virtual const FValueWithId& GetValue(const FValueId& Id) const = 0;
	virtual TConstArrayView<FValueWithId> GetValues() const = 0;
	virtual TConstArrayView<FBuildOutputMessage> GetMessages() const = 0;
	virtual TConstArrayView<FBuildOutputLog> GetLogs() const = 0;
	virtual bool HasLogs() const = 0;
	virtual bool HasError() const = 0;
	virtual void Save(FCbWriter& Writer) const = 0;
	virtual void Save(FCacheRecordBuilder& RecordBuilder) const = 0;
	virtual void AddRef() const = 0;
	virtual void Release() const = 0;
};

FBuildOutput CreateBuildOutput(IBuildOutputInternal* Output);

class IBuildOutputBuilderInternal
{
public:
	virtual ~IBuildOutputBuilderInternal() = default;
	virtual void SetMeta(FCbObject&& Meta) = 0;
	virtual void AddValue(const FValueId& Id, const FValue& Value) = 0;
	virtual void AddMessage(const FBuildOutputMessage& Message) = 0;
	virtual void AddLog(const FBuildOutputLog& Log) = 0;
	virtual bool HasError() const = 0;
	virtual FBuildOutput Build() = 0;
};

FBuildOutputBuilder CreateBuildOutputBuilder(IBuildOutputBuilderInternal* OutputBuilder);

} // UE::DerivedData::Private

namespace UE::DerivedData
{

enum class EBuildOutputMessageLevel : uint8
{
	Error,
	Warning,
	Display,
};

/** A build output message is diagnostic output from a build function and must be deterministic. */
struct FBuildOutputMessage
{
	FUtf8StringView Message;
	EBuildOutputMessageLevel Level;
};

enum class EBuildOutputLogLevel : uint8
{
	Error,
	Warning,
};

/**
 * A build output log is a log message captured from a build function.
 *
 * The build function may capture every log above a certain level of verbosity, which means these
 * have no guarantee of being deterministic. The presence of any output logs will disable caching
 * of the build output. To allow caching of build output with warnings or errors, replace the log
 * statements with build messages that are added to the build context.
 */
struct FBuildOutputLog
{
	FUtf8StringView Category;
	FUtf8StringView Message;
	EBuildOutputLogLevel Level;
};

/**
 * A build output is an immutable container of values, messages, and logs produced by a build.
 *
 * The output will not contain any values if it has any errors.
 *
 * The output can be requested without data, which means that the values will have null data.
 */
class FBuildOutput
{
public:
	/** Returns the name by which to identify this output for logging and profiling. */
	inline const FSharedString& GetName() const { return Output->GetName(); }

	/** Returns the name of the build function that produced this output. */
	inline const FUtf8SharedString& GetFunction() const { return Output->GetFunction(); }

	/** Returns the optional metadata. */
	inline const FCbObject& GetMeta() const { return Output->GetMeta(); }

	/** Returns the value matching the ID. Null if no match. Buffer is null if skipped. */
	inline const FValueWithId& GetValue(const FValueId& Id) const { return Output->GetValue(Id); }

	/** Returns the values in the output in order by ID. */
	inline TConstArrayView<FValueWithId> GetValues() const { return Output->GetValues(); }

	/** Returns the messages in the order that they were recorded. */
	inline TConstArrayView<FBuildOutputMessage> GetMessages() const { return Output->GetMessages(); }

	/** Returns the logs in the order that they were recorded. */
	inline TConstArrayView<FBuildOutputLog> GetLogs() const { return Output->GetLogs(); }

	/** Returns whether the output has any logs. */
	inline bool HasLogs() const { return Output->HasLogs(); }

	/** Returns whether the output has any errors. */
	inline bool HasError() const { return Output->HasError(); }

	/** Saves the build output to a compact binary object with values as attachments. */
	void Save(FCbWriter& Writer) const
	{
		Output->Save(Writer);
	}

	/** Saves the build output to a cache record. */
	void Save(FCacheRecordBuilder& RecordBuilder) const
	{
		Output->Save(RecordBuilder);
	}

	/**
	 * Load a build output.
	 *
	 * @param Name       The name by which to identify this output for logging and profiling.
	 * @param Function   The name of the build function that produced this output.
	 * @param Output     The saved output to load.
	 * @return A valid build output, or null on error.
	 */
	UE_API static FOptionalBuildOutput Load(const FSharedString& Name, const FUtf8SharedString& Function, const FCbObject& Output);
	UE_API static FOptionalBuildOutput Load(const FSharedString& Name, const FUtf8SharedString& Function, const FCacheRecord& Output);

private:
	friend class FOptionalBuildOutput;
	friend FBuildOutput Private::CreateBuildOutput(Private::IBuildOutputInternal* Output);

	inline explicit FBuildOutput(Private::IBuildOutputInternal* InOutput)
		: Output(InOutput)
	{
	}

	TRefCountPtr<Private::IBuildOutputInternal> Output;
};

/**
 * A build output builder is used to construct a build output.
 *
 * Create using IBuild::CreateOutput().
 *
 * @see FBuildOutput
 */
class FBuildOutputBuilder
{
public:
	/** Set the metadata for the output. Holds a reference and is cloned if not owned. */
	inline void SetMeta(FCbObject&& Meta)
	{
		return OutputBuilder->SetMeta(MoveTemp(Meta));
	}

	/** Add a value to the output. The ID must be unique in this output. */
	inline void AddValue(const FValueId& Id, const FValue& Value)
	{
		OutputBuilder->AddValue(Id, Value);
	}

	/** Add a message to the output. */
	inline void AddMessage(const FBuildOutputMessage& Message)
	{
		OutputBuilder->AddMessage(Message);
	}

	/** Add a log to the output. */
	inline void AddLog(const FBuildOutputLog& Log)
	{
		OutputBuilder->AddLog(Log);
	}

	/** Returns whether the output has any errors. */
	inline bool HasError() const
	{
		return OutputBuilder->HasError();
	}

	/** Build a build output, which makes this builder subsequently unusable. */
	inline FBuildOutput Build()
	{
		ON_SCOPE_EXIT { OutputBuilder = nullptr; };
		return OutputBuilder->Build();
	}

private:
	friend FBuildOutputBuilder Private::CreateBuildOutputBuilder(Private::IBuildOutputBuilderInternal* OutputBuilder);

	/** Construct a build output builder. Use IBuild::CreateOutput(). */
	inline explicit FBuildOutputBuilder(Private::IBuildOutputBuilderInternal* InOutputBuilder)
		: OutputBuilder(InOutputBuilder)
	{
	}

	TUniquePtr<Private::IBuildOutputBuilderInternal> OutputBuilder;
};

/**
 * A build output that can be null.
 *
 * @see FBuildOutput
 */
class FOptionalBuildOutput : private FBuildOutput
{
public:
	inline FOptionalBuildOutput() : FBuildOutput(nullptr) {}

	inline FOptionalBuildOutput(FBuildOutput&& InOutput) : FBuildOutput(MoveTemp(InOutput)) {}
	inline FOptionalBuildOutput(const FBuildOutput& InOutput) : FBuildOutput(InOutput) {}
	inline FOptionalBuildOutput& operator=(FBuildOutput&& InOutput) { FBuildOutput::operator=(MoveTemp(InOutput)); return *this; }
	inline FOptionalBuildOutput& operator=(const FBuildOutput& InOutput) { FBuildOutput::operator=(InOutput); return *this; }

	/** Returns the build output. The caller must check for null before using this accessor. */
	inline const FBuildOutput& Get() const & { return *this; }
	inline FBuildOutput Get() && { return MoveTemp(*this); }

	inline bool IsNull() const { return !IsValid(); }
	inline bool IsValid() const { return Output.IsValid(); }
	inline explicit operator bool() const { return IsValid(); }

	inline void Reset() { *this = FOptionalBuildOutput(); }
};

} // UE::DerivedData

#undef UE_API
