// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Concepts/Insertable.h"
#include "Containers/StringFwd.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/OptionalFwd.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Templates/IsArrayOrRefOfType.h"
#include "Templates/Models.h"
#include "Templates/UnrealTypeTraits.h"

#include <atomic>
#include <type_traits>

class FTextFormat;

#define UE_API CORE_API

#if !NO_LOGGING

/**
 * Records a structured log event if this category is active at this level of verbosity.
 *
 * Supports either positional or named parameters, but not a mix of these styles.
 *
 * Positional: The field values must exactly match the fields referenced by Format.
 * UE_LOGFMT(LogCore, Warning, "Loading '{Name}' failed with error {Error}", Package->GetName(), ErrorCode);
 *
 * Named: The fields must contain every field referenced by Format. Order is irrelevant and extra fields are permitted.
 * UE_LOGFMT(LogCore, Warning, "Loading '{Name}' failed with error {Error}",
 *     ("Name", Package->GetName()), ("Error", ErrorCode), ("Flags", LoadFlags));
 *
 * Field names must match "[A-Za-z0-9_]+" and must be unique within this log event.
 * Field values will be serialized using SerializeForLog or operator<<(FCbWriter&, FieldType).
 *
 * @param CategoryName   Name of a log category declared by DECLARE_LOG_CATEGORY_*.
 * @param Verbosity      Name of a log verbosity level from ELogVerbosity.
 * @param Format         Format string in the style of FLogTemplate.
 * @param Fields[0-16]   Zero to sixteen fields or field values.
 */
#define UE_LOGFMT(CategoryName, Verbosity, Format, ...) UE_PRIVATE_LOGFMT_CALL(UE_LOGFMT_EX, (CategoryName, Verbosity, Format UE_PRIVATE_LOGFMT_FIELDS(__VA_ARGS__)))

/**
 * Records a structured log event if this category is active at this level of verbosity.
 *
 * This has the same functionality as UE_LOGFMT but removes the limit on field count.
 *
 * Positional: Values must be wrapped in UE_LOGFMT_VALUE.
 * UE_LOGFMT_EX(LogCore, Warning, "Loading '{Name}' failed with error {Error}",
 *     UE_LOGFMT_VALUE(Package->GetName()), UE_LOGFMT_VALUE(ErrorCode));
 *
 * Named: Fields must be wrapped in UE_LOGFMT_FIELD.
 * UE_LOGFMT_EX(LogCore, Warning, "Loading '{Name}' failed with error {Error}",
 *     UE_LOGFMT_FIELD("Name", Package->GetName()), UE_LOGFMT_FIELD("Error", ErrorCode), UE_LOGFMT_FIELD("Flags", LoadFlags));
 */
#define UE_LOGFMT_EX(CategoryName, Verbosity, Format, ...) UE_PRIVATE_LOGFMT(CategoryName, Verbosity, Format, ##__VA_ARGS__)

/**
 * Records a localized structured log event if this category is active at this level of verbosity.
 *
 * Example:
 * UE_LOGFMT_LOC(LogCore, Warning, "LoadingFailed", "Loading '{Name}' failed with error {Error}",
 *     ("Name", Package->GetName()), ("Error", ErrorCode), ("Flags", LoadFlags));
 *
 * Field names must match "[A-Za-z0-9_]+" and must be unique within this log event.
 * Field values will be serialized using SerializeForLog or operator<<(FCbWriter&, FieldType).
 * The fields must contain every field referenced by Format. Order is irrelevant and extra fields are permitted.
 *
 * @param CategoryName   Name of a log category declared by DECLARE_LOG_CATEGORY_*.
 * @param Verbosity      Name of a log verbosity level from ELogVerbosity.
 * @param Namespace      Namespace for the format FText, or LOCTEXT_NAMESPACE for the non-NS macro.
 * @param Key            Key for the format FText that is unique within the namespace.
 * @param Format         Format string in the style of FTextFormat.
 * @param Fields[0-16]   Zero to sixteen fields in the format ("Name", Value).
 */
#define UE_LOGFMT_LOC(CategoryName, Verbosity, Key, Format, ...) \
	UE_LOGFMT_NSLOC(CategoryName, Verbosity, LOCTEXT_NAMESPACE, Key, Format, ##__VA_ARGS__)
#define UE_LOGFMT_NSLOC(CategoryName, Verbosity, Namespace, Key, Format, ...) \
	UE_PRIVATE_LOGFMT_CALL(UE_LOGFMT_NSLOC_EX, (CategoryName, Verbosity, Namespace, Key, Format UE_PRIVATE_LOGFMT_FIELDS(__VA_ARGS__)))

/**
 * Records a localized structured log event if this category is active at this level of verbosity.
 *
 * Example:
 * UE_LOGFMT_LOC_EX(LogCore, Warning, "LoadingFailed", "Loading '{PackageName}' failed with error {Error}",
 *     UE_LOGFMT_FIELD("Name", Package->GetName()), UE_LOGFMT_FIELD("Error", ErrorCode), UE_LOGFMT_FIELD("Flags", LoadFlags));
 *
 * Same as UE_LOGFMT_LOC and works for any number of fields.
 * Fields must be written as UE_LOGFMT_FIELD("Name", Value).
 */
#define UE_LOGFMT_LOC_EX(CategoryName, Verbosity, Key, Format, ...) \
	UE_LOGFMT_NSLOC_EX(CategoryName, Verbosity, LOCTEXT_NAMESPACE, Key, Format, ##__VA_ARGS__)
#define UE_LOGFMT_NSLOC_EX(CategoryName, Verbosity, Namespace, Key, Format, ...) \
	UE_PRIVATE_LOGFMT_LOC(CategoryName, Verbosity, Namespace, Key, Format, ##__VA_ARGS__)

/** Expands to a named structured log field. UE_LOGFMT_FIELD("Name", Value) */
#define UE_LOGFMT_FIELD(Name, Value) UE_PRIVATE_LOGFMT_FIELD((Name, Value))

/** Expands to a structured log value. UE_LOGFMT_VALUE(Value) */
#define UE_LOGFMT_VALUE(Value) Value

#else // #if !NO_LOGGING

#define UE_LOGFMT(...)
#define UE_LOGFMT_EX(...)

#define UE_LOGFMT_LOC(...)
#define UE_LOGFMT_NSLOC(...)
#define UE_LOGFMT_LOC_EX(...)
#define UE_LOGFMT_NSLOC_EX(...)

#define UE_LOGFMT_FIELD(Name, Value)
#define UE_LOGFMT_VALUE(Value)

#endif

struct FDateTime;

namespace UE
{

/** Template format is "Text with {Fields} embedded {Like}{This}. {{Double to escape.}}" */
class FLogTemplate;

/**
 * Time that a log event occurred.
 */
class FLogTime
{
public:
	UE_API static FLogTime Now();

	constexpr FLogTime() = default;

	/** Returns the UTC time. 0 ticks when the time was not set. */
	UE_API FDateTime GetUtcTime() const;

private:
	/** Ticks from FDateTime::UtcNow() */
	int64 UtcTicks = 0;
};

/**
 * Record of a log event.
 */
class FLogRecord
{
public:
	/** The optional name of the category for the log record. None when omitted. */
	const FName& GetCategory() const { return Category; }
	void SetCategory(const FName& InCategory) { Category = InCategory; }

	/** The verbosity level of the log record. Must be a valid level with no flags or special values. */
	ELogVerbosity::Type GetVerbosity() const { return Verbosity; }
	void SetVerbosity(ELogVerbosity::Type InVerbosity) { Verbosity = InVerbosity; }

	/** The time at which the log record was created. */
	const FLogTime& GetTime() const { return Time; }
	void SetTime(const FLogTime& InTime) { Time = InTime; }

	/** The format string that serves as the message for the log record. Example: TEXT("FieldName is {FieldName}") */
	const TCHAR* GetFormat() const { return Format; }
	void SetFormat(const TCHAR* InFormat) { Format = InFormat; }

	/** The optional template for the format string. */
	const FLogTemplate* GetTemplate() const { return Template; }
	void SetTemplate(const FLogTemplate* InTemplate) { Template = InTemplate; }

	/** The fields referenced by the format string, along with optional additional fields. */
	const FCbObject& GetFields() const { return Fields; }
	void SetFields(FCbObject&& InFields) { Fields = MoveTemp(InFields); }

	/** The optional source file path for the code that created the log record. Null when omitted. */
	const ANSICHAR* GetFile() const { return File; }
	void SetFile(const ANSICHAR* InFile) { File = InFile; }

	/** The optional source line number for the code that created the log record. 0 when omitted. */
	int32 GetLine() const { return Line; }
	void SetLine(int32 InLine) { Line = InLine; }

	/** The namespace of the localized text. Null when non-localized. */
	const TCHAR* GetTextNamespace() const { return TextNamespace; }
	void SetTextNamespace(const TCHAR* InTextNamespace) { TextNamespace = InTextNamespace; }

	/** The key of the localized text. Null when non-localized. */
	const TCHAR* GetTextKey() const { return TextKey; }
	void SetTextKey(const TCHAR* InTextKey) { TextKey = InTextKey; }

	/** Formats the message using the format, template, and fields. */
	UE_API void FormatMessageTo(FUtf8StringBuilderBase& Out) const;
	UE_API void FormatMessageTo(FWideStringBuilderBase& Out) const;

private:
	const TCHAR* Format = nullptr;
	const ANSICHAR* File = nullptr;
	int32 Line = 0;
	FName Category;
	ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
	FLogTime Time;
	FCbObject Fields;
	const FLogTemplate* Template = nullptr;
	const TCHAR* TextNamespace = nullptr;
	const TCHAR* TextKey = nullptr;
};

/**
 * Serializes the value to be used in a log message.
 *
 * Overload this when the log behavior needs to differ from general serialization to compact binary.
 */
template <typename ValueType UE_REQUIRES(TModels_V<CInsertable<FCbWriter&>, ValueType>)>
inline void SerializeForLog(FCbWriter& Writer, ValueType&& Value)
{
	Writer << (ValueType&&)Value;
}

/** Describes a type that can be serialized for use in a log message. */
struct CSerializableForLog
{
	template <typename ValueType>
	auto Requires(FCbWriter& Writer, ValueType&& Value) -> decltype(
		SerializeForLog(Writer, (ValueType&&)Value)
	);
};

/** Wrapper to support calling SerializeForLog with ADL from within an overload of SerializeForLog. */
template <typename ValueType UE_REQUIRES(TModels_V<CSerializableForLog, ValueType>)>
inline void CallSerializeForLog(FCbWriter& Writer, ValueType&& Value)
{
	SerializeForLog(Writer, (ValueType&&)Value);
}

} // UE

template <typename ValueType UE_REQUIRES(TModels_V<UE::CSerializableForLog, ValueType>)>
inline void SerializeForLog(FCbWriter& Writer, const TOptional<ValueType>& Optional)
{
	Writer.BeginArray();
	if (Optional)
	{
		UE::CallSerializeForLog(Writer, Optional.GetValue());
	}
	Writer.EndArray();
}

namespace UE::Logging::Private
{

/** Data about a static log that is created on-demand. */
struct FStaticLogDynamicData
{
	std::atomic<FLogTemplate*> Template;
};

/** Data about a static log that is constant for every occurrence. */
struct FStaticLogRecord
{
	const TCHAR* Format = nullptr;
	const ANSICHAR* File = nullptr;
	int32 Line = 0;
	ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
	FStaticLogDynamicData& DynamicData;

	// Workaround for https://developercommunity.visualstudio.com/t/Incorrect-warning-C4700-with-unrelated-s/10285950
	constexpr FStaticLogRecord(
		const TCHAR* InFormat,
		const ANSICHAR* InFile,
		int32 InLine,
		ELogVerbosity::Type InVerbosity,
		FStaticLogDynamicData& InDynamicData)
		: Format(InFormat)
		, File(InFile)
		, Line(InLine)
		, Verbosity(InVerbosity)
		, DynamicData(InDynamicData)
	{
	}
};

/** Data about a static localized log that is constant for every occurrence. */
struct FStaticLocalizedLogRecord
{
	const TCHAR* TextNamespace = nullptr;
	const TCHAR* TextKey = nullptr;
	const TCHAR* Format = nullptr;
	const ANSICHAR* File = nullptr;
	int32 Line = 0;
	ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
	FStaticLogDynamicData& DynamicData;

	// Workaround for https://developercommunity.visualstudio.com/t/Incorrect-warning-C4700-with-unrelated-s/10285950
	constexpr FStaticLocalizedLogRecord(
		const TCHAR* InTextNamespace,
		const TCHAR* InTextKey,
		const TCHAR* InFormat,
		const ANSICHAR* InFile,
		int32 InLine,
		ELogVerbosity::Type InVerbosity,
		FStaticLogDynamicData& InDynamicData)
		: TextNamespace(InTextNamespace)
		, TextKey(InTextKey)
		, Format(InFormat)
		, File(InFile)
		, Line(InLine)
		, Verbosity(InVerbosity)
		, DynamicData(InDynamicData)
	{
	}
};

struct FLogField
{
	using FWriteFn = void (FCbWriter& Writer, const void* Value);

	const ANSICHAR* Name;
	const void* Value;
	FWriteFn* WriteValue;

	template <typename ValueType>
	static void Write(FCbWriter& Writer, const void* Value)
	{
		SerializeForLog(Writer, *(const ValueType*)Value);
	}
};

} // UE::Logging::Private

#if !NO_LOGGING

namespace UE::Logging::Private
{

UE_API void LogWithNoFields(const FLogCategoryBase& Category, const FStaticLogRecord& Log);
UE_API void LogWithFieldArray(const FLogCategoryBase& Category, const FStaticLogRecord& Log, const FLogField* Fields, int32 FieldCount);

UE_API void FatalLogWithNoFields(const FLogCategoryBase& Category, const FStaticLogRecord& Log);
UE_API void FatalLogWithFieldArray(const FLogCategoryBase& Category, const FStaticLogRecord& Log, const FLogField* Fields, int32 FieldCount);

UE_API void LogWithNoFields(const FLogCategoryBase& Category, const FStaticLocalizedLogRecord& Log);
UE_API void LogWithFieldArray(const FLogCategoryBase& Category, const FStaticLocalizedLogRecord& Log, const FLogField* Fields, int32 FieldCount);

UE_API void FatalLogWithNoFields(const FLogCategoryBase& Category, const FStaticLocalizedLogRecord& Log);
UE_API void FatalLogWithFieldArray(const FLogCategoryBase& Category, const FStaticLocalizedLogRecord& Log, const FLogField* Fields, int32 FieldCount);

/** Wrapper to identify field names interleaved with field values. */
template <typename NameType>
struct TLogFieldName
{
	NameType Name;
};

/** Verify that the name is likely a string literal and forward it on. */
template <typename NameType>
inline constexpr TLogFieldName<NameType> CheckFieldName(NameType&& Name)
{
	static_assert(TIsArrayOrRefOfType<NameType, ANSICHAR>::Value, "Name must be an ANSICHAR string literal.");
	return {(NameType&&)Name};
}

/** Create log fields from values optionally preceded by names. */
struct FLogFieldCreator
{
	template <typename T> inline constexpr static int32 ValueCount = 1;
	template <typename T> inline constexpr static int32 ValueCount<TLogFieldName<T>> = 0;

	template <typename... FieldArgTypes>
	inline constexpr static int32 GetCount()
	{
		return (ValueCount<FieldArgTypes> + ...);
	}

	inline static void Create(FLogField* Fields)
	{
	}

	template <typename ValueType, typename... FieldArgTypes, std::enable_if_t<ValueCount<ValueType>>* = nullptr>
	inline static void Create(FLogField* Fields, const ValueType& Value, FieldArgTypes&&... FieldArgs)
	{
		new(Fields) FLogField{nullptr, &Value, FLogField::Write<std::remove_reference_t<ValueType>>};
		Create(Fields + 1, (FieldArgTypes&&)FieldArgs...);
	}

	template <typename NameType, typename ValueType, typename... FieldArgTypes>
	inline static void Create(FLogField* Fields, TLogFieldName<NameType> Name, const ValueType& Value, FieldArgTypes&&... FieldArgs)
	{
		new(Fields) FLogField{Name.Name, &Value, FLogField::Write<std::remove_reference_t<ValueType>>};
		Create(Fields + 1, (FieldArgTypes&&)FieldArgs...);
	}
};

template <typename T>
struct TFieldArgType
{
	using Type = typename TCallTraits<T>::ParamType;
};

template <typename NameType>
struct TFieldArgType<TLogFieldName<NameType>>
{
	using Type = TLogFieldName<NameType>;
};

/** Log with fields created from the arguments, which may be values or pairs of name/value. */
template <typename... FieldArgTypes, typename LogType>
FORCENOINLINE UE_DEBUG_SECTION void LogWithFields(const FLogCategoryBase& Category, const LogType& Log, typename TFieldArgType<FieldArgTypes>::Type... FieldArgs)
{
	constexpr int32 FieldCount = FLogFieldCreator::template GetCount<FieldArgTypes...>();
	static_assert(FieldCount > 0);
	FLogField Fields[FieldCount];
	FLogFieldCreator::Create(Fields, (FieldArgTypes&&)FieldArgs...);
	LogWithFieldArray(Category, Log, Fields, FieldCount);
}

/** Fatal log with fields created from the arguments, which may be values or pairs of name/value. */
template <typename... FieldArgTypes, typename LogType>
inline void FatalLogWithFields(const FLogCategoryBase& Category, const LogType& Log, typename TFieldArgType<FieldArgTypes>::Type... FieldArgs)
{
	constexpr int32 FieldCount = FLogFieldCreator::template GetCount<FieldArgTypes...>();
	static_assert(FieldCount > 0);
	FLogField Fields[FieldCount];
	FLogFieldCreator::Create(Fields, (FieldArgTypes&&)FieldArgs...);
	FatalLogWithFieldArray(Category, Log, Fields, FieldCount);
}

/** Log if the category is active at this level of verbosity. */
template <typename LogCategoryType, ELogVerbosity::Type Verbosity, typename LogRecordType, typename... FieldArgTypes>
inline void LogIfActive(const LogCategoryType& Category, const LogRecordType& Log, FieldArgTypes&&... FieldArgs)
{
	static_assert((Verbosity & ELogVerbosity::VerbosityMask) < ELogVerbosity::NumVerbosity && Verbosity > 0, "Verbosity must be constant and in range.");
	if constexpr ((Verbosity & ELogVerbosity::VerbosityMask) == ELogVerbosity::Fatal)
	{
		if constexpr (sizeof...(FieldArgTypes) == 0)
		{
			FatalLogWithNoFields(Category, Log);
		}
		else
		{
			FatalLogWithFields<FieldArgTypes...>(Category, Log, (FieldArgTypes&&)FieldArgs...);
		}
		CA_ASSUME(false);
	}
	else if constexpr (
		(Verbosity & ELogVerbosity::VerbosityMask) <= ELogVerbosity::COMPILED_IN_MINIMUM_VERBOSITY &&
		(Verbosity & ELogVerbosity::VerbosityMask) <= LogCategoryType::CompileTimeVerbosity)
	{
		if (!Category.IsSuppressed(Verbosity))
		{
			if constexpr (sizeof...(FieldArgTypes) == 0)
			{
				LogWithNoFields(Category, Log);
			}
			else
			{
				LogWithFields<FieldArgTypes...>(Category, Log, (FieldArgTypes&&)FieldArgs...);
			}
		}
	}
}

} // UE::Logging::Private

#define UE_PRIVATE_LOGFMT(CategoryName, Verbosity, Format, ...) \
	do \
	{ \
		static ::UE::Logging::Private::FStaticLogDynamicData LOG_Dynamic; \
		static constexpr ::UE::Logging::Private::FStaticLogRecord LOG_Static UE_PRIVATE_LOGFMT_AGGREGATE(TEXT(Format), __builtin_FILE(), __builtin_LINE(), ::ELogVerbosity::Verbosity, LOG_Dynamic); \
		::UE::Logging::Private::LogIfActive<FLogCategory##CategoryName, ::ELogVerbosity::Verbosity>(CategoryName, LOG_Static, ##__VA_ARGS__); \
	} \
	while (false)

#define UE_PRIVATE_LOGFMT_LOC(CategoryName, Verbosity, Namespace, Key, Format, ...) \
	do \
	{ \
		static ::UE::Logging::Private::FStaticLogDynamicData LOG_Dynamic; \
		static constexpr ::UE::Logging::Private::FStaticLocalizedLogRecord LOG_Static UE_PRIVATE_LOGFMT_AGGREGATE(TEXT(Namespace), TEXT(Key), TEXT(Format), __builtin_FILE(), __builtin_LINE(), ::ELogVerbosity::Verbosity, LOG_Dynamic); \
		::UE::Logging::Private::LogIfActive<FLogCategory##CategoryName, ::ELogVerbosity::Verbosity>(CategoryName, LOG_Static, ##__VA_ARGS__); \
	} \
	while (false)

// This macro avoids having non-parenthesized commas in the log macros.
// Such commas can cause issues when a log macro is wrapped by another macro.
#define UE_PRIVATE_LOGFMT_AGGREGATE(...) {__VA_ARGS__}

// This macro expands a field from either `(Name, Value)` or `Value`
// A `(Name, Value)` field is converted to `CheckFieldName(Name), Value`
// A `Value` field is passed through as `Value`
#define UE_PRIVATE_LOGFMT_FIELD(Field) UE_PRIVATE_LOGFMT_FIELD_EXPAND(UE_PRIVATE_LOGFMT_NAMED_FIELD Field)
// This macro is only called when the field was parenthesized.
#define UE_PRIVATE_LOGFMT_NAMED_FIELD(Name, ...) UE_PRIVATE_LOGFMT_NAMED_FIELD ::UE::Logging::Private::CheckFieldName(Name), __VA_ARGS__
// The next three macros remove UE_PRIVATE_LOGFMT_NAMED_FIELD from the expanded expression.
#define UE_PRIVATE_LOGFMT_FIELD_EXPAND(...) UE_PRIVATE_LOGFMT_FIELD_EXPAND_INNER(__VA_ARGS__)
#define UE_PRIVATE_LOGFMT_FIELD_EXPAND_INNER(...) UE_PRIVATE_LOGFMT_STRIP_ ## __VA_ARGS__
#define UE_PRIVATE_LOGFMT_STRIP_UE_PRIVATE_LOGFMT_NAMED_FIELD

// This macro expands `Arg1, Arg2` to `UE_PRIVATE_LOGFMT_FIELD(Arg1), UE_PRIVATE_LOGFMT_FIELD(Arg2), ...`
// This macro expands `("Name1", Arg1), ("Name2", Arg2)` to `UE_PRIVATE_LOGFMT_FIELD(("Name1", Arg1)), UE_PRIVATE_LOGFMT_FIELD(("Name2", Arg2)), ...
#define UE_PRIVATE_LOGFMT_FIELDS(...) UE_PRIVATE_LOGFMT_CALL(PREPROCESSOR_JOIN(UE_PRIVATE_LOGFMT_FIELDS_, UE_PRIVATE_LOGFMT_COUNT(__VA_ARGS__)), (__VA_ARGS__))

#define UE_PRIVATE_LOGFMT_FIELDS_0()
#define UE_PRIVATE_LOGFMT_FIELDS_1(A)                 , UE_PRIVATE_LOGFMT_FIELD(A)
#define UE_PRIVATE_LOGFMT_FIELDS_2(A,B)               , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B)
#define UE_PRIVATE_LOGFMT_FIELDS_3(A,B,C)             , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C)
#define UE_PRIVATE_LOGFMT_FIELDS_4(A,B,C,D)           , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D)
#define UE_PRIVATE_LOGFMT_FIELDS_5(A,B,C,D,E)         , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E)
#define UE_PRIVATE_LOGFMT_FIELDS_6(A,B,C,D,E,F)       , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F)
#define UE_PRIVATE_LOGFMT_FIELDS_7(A,B,C,D,E,F,G)     , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G)
#define UE_PRIVATE_LOGFMT_FIELDS_8(A,B,C,D,E,F,G,H)   , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G), UE_PRIVATE_LOGFMT_FIELD(H)
#define UE_PRIVATE_LOGFMT_FIELDS_9(A,B,C,D,E,F,G,H,I) , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G), UE_PRIVATE_LOGFMT_FIELD(H), UE_PRIVATE_LOGFMT_FIELD(I)

#define UE_PRIVATE_LOGFMT_FIELDS_10(A,B,C,D,E,F,G,H,I,J)             , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G), UE_PRIVATE_LOGFMT_FIELD(H), UE_PRIVATE_LOGFMT_FIELD(I), UE_PRIVATE_LOGFMT_FIELD(J)
#define UE_PRIVATE_LOGFMT_FIELDS_11(A,B,C,D,E,F,G,H,I,J,K)           , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G), UE_PRIVATE_LOGFMT_FIELD(H), UE_PRIVATE_LOGFMT_FIELD(I), UE_PRIVATE_LOGFMT_FIELD(J), UE_PRIVATE_LOGFMT_FIELD(K)
#define UE_PRIVATE_LOGFMT_FIELDS_12(A,B,C,D,E,F,G,H,I,J,K,L)         , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G), UE_PRIVATE_LOGFMT_FIELD(H), UE_PRIVATE_LOGFMT_FIELD(I), UE_PRIVATE_LOGFMT_FIELD(J), UE_PRIVATE_LOGFMT_FIELD(K), UE_PRIVATE_LOGFMT_FIELD(L)
#define UE_PRIVATE_LOGFMT_FIELDS_13(A,B,C,D,E,F,G,H,I,J,K,L,M)       , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G), UE_PRIVATE_LOGFMT_FIELD(H), UE_PRIVATE_LOGFMT_FIELD(I), UE_PRIVATE_LOGFMT_FIELD(J), UE_PRIVATE_LOGFMT_FIELD(K), UE_PRIVATE_LOGFMT_FIELD(L), UE_PRIVATE_LOGFMT_FIELD(M)
#define UE_PRIVATE_LOGFMT_FIELDS_14(A,B,C,D,E,F,G,H,I,J,K,L,M,N)     , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G), UE_PRIVATE_LOGFMT_FIELD(H), UE_PRIVATE_LOGFMT_FIELD(I), UE_PRIVATE_LOGFMT_FIELD(J), UE_PRIVATE_LOGFMT_FIELD(K), UE_PRIVATE_LOGFMT_FIELD(L), UE_PRIVATE_LOGFMT_FIELD(M), UE_PRIVATE_LOGFMT_FIELD(N)
#define UE_PRIVATE_LOGFMT_FIELDS_15(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O)   , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G), UE_PRIVATE_LOGFMT_FIELD(H), UE_PRIVATE_LOGFMT_FIELD(I), UE_PRIVATE_LOGFMT_FIELD(J), UE_PRIVATE_LOGFMT_FIELD(K), UE_PRIVATE_LOGFMT_FIELD(L), UE_PRIVATE_LOGFMT_FIELD(M), UE_PRIVATE_LOGFMT_FIELD(N), UE_PRIVATE_LOGFMT_FIELD(O)
#define UE_PRIVATE_LOGFMT_FIELDS_16(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P) , UE_PRIVATE_LOGFMT_FIELD(A), UE_PRIVATE_LOGFMT_FIELD(B), UE_PRIVATE_LOGFMT_FIELD(C), UE_PRIVATE_LOGFMT_FIELD(D), UE_PRIVATE_LOGFMT_FIELD(E), UE_PRIVATE_LOGFMT_FIELD(F), UE_PRIVATE_LOGFMT_FIELD(G), UE_PRIVATE_LOGFMT_FIELD(H), UE_PRIVATE_LOGFMT_FIELD(I), UE_PRIVATE_LOGFMT_FIELD(J), UE_PRIVATE_LOGFMT_FIELD(K), UE_PRIVATE_LOGFMT_FIELD(L), UE_PRIVATE_LOGFMT_FIELD(M), UE_PRIVATE_LOGFMT_FIELD(N), UE_PRIVATE_LOGFMT_FIELD(O), UE_PRIVATE_LOGFMT_FIELD(P)

#define UE_PRIVATE_LOGFMT_COUNT(...) UE_PRIVATE_LOGFMT_CALL(UE_PRIVATE_LOGFMT_COUNT_IMPL, (_, ##__VA_ARGS__, 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0))
#define UE_PRIVATE_LOGFMT_COUNT_IMPL(_, A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P, Count, ...) Count

#define UE_PRIVATE_LOGFMT_CALL(F, A) UE_PRIVATE_LOGFMT_EXPAND(F A)
#define UE_PRIVATE_LOGFMT_EXPAND(X) X

#endif // #if !NO_LOGGING

#undef UE_API
