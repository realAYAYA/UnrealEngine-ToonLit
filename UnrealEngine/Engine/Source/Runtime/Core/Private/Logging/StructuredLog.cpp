// Copyright Epic Games, Inc. All Rights Reserved.

#include "Logging/StructuredLog.h"

#include "Algo/Accumulate.h"
#include "Algo/AllOf.h"
#include "Algo/NoneOf.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "CoreGlobals.h"
#include "HAL/PlatformMath.h"
#include "HAL/PlatformTime.h"
#include "Internationalization/Internationalization.h"
#include "Logging/LogTrace.h"
#include "Logging/StructuredLogFormat.h"
#include "Misc/AsciiSet.h"
#include "Misc/DateTime.h"
#include "Misc/FeedbackContext.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeExit.h"
#include "Misc/StringBuilder.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryValue.h"
#include "Serialization/VarInt.h"

void VARARGS StaticFailDebug (const TCHAR* Error, const ANSICHAR* Expression, const ANSICHAR* File, int32 Line, bool bIsEnsure, void* ProgramCounter, const TCHAR* DescriptionFormat, ...);
void         StaticFailDebugV(const TCHAR* Error, const ANSICHAR* Expression, const ANSICHAR* File, int32 Line, bool bIsEnsure, void* ProgramCounter, const TCHAR* DescriptionFormat, va_list DescriptionArgs);

namespace UE::Logging::Private
{

// Temporary override until performance and functionality are sufficient for this to be the default.
CORE_API bool GConvertBasicLogToLogRecord = false;

static constexpr FAsciiSet ValidLogFieldName("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_");

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct FLogTemplateOp
{
	enum EOpCode : int32 { OpEnd, OpSkip, OpText, OpName, OpIndex, OpLocText, OpCount };

	static constexpr int32 ValueShift = 3;
	static_assert(OpCount <= (1 << ValueShift));

	EOpCode Code = OpEnd;
	int32 Value = 0;

	inline int32 GetSkipSize() const { return Code == OpIndex ? 0 : Value; }

	static inline FLogTemplateOp Load(const uint8*& Data);
	static inline uint32 SaveSize(const FLogTemplateOp& Op) { return MeasureVarUInt(Encode(Op)); }
	static inline void Save(const FLogTemplateOp& Op, uint8*& Data);
	static inline uint64 Encode(const FLogTemplateOp& Op) { return uint64(Op.Code) | (uint64(Op.Value) << ValueShift); }
	static inline FLogTemplateOp Decode(uint64 Value) { return {EOpCode(Value & ((1 << ValueShift) - 1)), int32(Value >> ValueShift)}; }
};

inline FLogTemplateOp FLogTemplateOp::Load(const uint8*& Data)
{
	uint32 ByteCount = 0;
	ON_SCOPE_EXIT { Data += ByteCount; };
	return Decode(ReadVarUInt(Data, ByteCount));
}

inline void FLogTemplateOp::Save(const FLogTemplateOp& Op, uint8*& Data)
{
	Data += WriteVarUInt(Encode(Op), Data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename CharType>
struct TLogFieldValueConstants;

template <>
struct TLogFieldValueConstants<UTF8CHAR>
{
	static inline const FAnsiStringView Null = ANSITEXTVIEW("null");
	static inline const FAnsiStringView True = ANSITEXTVIEW("true");
	static inline const FAnsiStringView False = ANSITEXTVIEW("false");
};

template <>
struct TLogFieldValueConstants<WIDECHAR>
{
	static inline const FWideStringView Null = WIDETEXTVIEW("null");
	static inline const FWideStringView True = WIDETEXTVIEW("true");
	static inline const FWideStringView False = WIDETEXTVIEW("false");
};

template <typename CharType>
static void LogFieldValue(TStringBuilderBase<CharType>& Out, const FCbFieldView& Field)
{
	using FConstants = TLogFieldValueConstants<CharType>;
	switch (FCbValue Accessor = Field.GetValue(); Accessor.GetType())
	{
	case ECbFieldType::Null:
		Out.Append(FConstants::Null);
		break;
	case ECbFieldType::Object:
	case ECbFieldType::UniformObject:
		if (FCbFieldView TextField = Accessor.AsObjectView().FindViewIgnoreCase(ANSITEXTVIEW("$text")))
		{
			Out.Append(TextField.AsString());
			break;
		}
		[[fallthrough]];
	case ECbFieldType::Array:
	case ECbFieldType::UniformArray:
	case ECbFieldType::Binary:
		CompactBinaryToCompactJson(Field.RemoveName(), Out);
		break;
	case ECbFieldType::String:
		Out.Append(Accessor.AsString());
		break;
	case ECbFieldType::IntegerPositive:
		Out << Accessor.AsIntegerPositive();
		break;
	case ECbFieldType::IntegerNegative:
		Out << Accessor.AsIntegerNegative();
		break;
	case ECbFieldType::Float32:
	case ECbFieldType::Float64:
		CompactBinaryToCompactJson(Field.RemoveName(), Out);
		break;
	case ECbFieldType::BoolFalse:
		Out.Append(FConstants::False);
		break;
	case ECbFieldType::BoolTrue:
		Out.Append(FConstants::True);
		break;
	case ECbFieldType::ObjectAttachment:
	case ECbFieldType::BinaryAttachment:
		Out << Accessor.AsAttachment();
		break;
	case ECbFieldType::Hash:
		Out << Accessor.AsHash();
		break;
	case ECbFieldType::Uuid:
		Out << Accessor.AsUuid();
		break;
	case ECbFieldType::DateTime:
		Out << FDateTime(Accessor.AsDateTimeTicks()).ToIso8601();
		break;
	case ECbFieldType::TimeSpan:
	{
		const FTimespan Span(Accessor.AsTimeSpanTicks());
		if (Span.GetDays() == 0)
		{
			Out << Span.ToString(TEXT("%h:%m:%s.%n"));
		}
		else
		{
			Out << Span.ToString(TEXT("%d.%h:%m:%s.%n"));
		}
		break;
	}
	case ECbFieldType::ObjectId:
		Out << Accessor.AsObjectId();
		break;
	case ECbFieldType::CustomById:
	case ECbFieldType::CustomByName:
		CompactBinaryToCompactJson(Field.RemoveName(), Out);
		break;
	default:
		checkNoEntry();
		break;
	}
}

static void AddFieldValue(FFormatNamedArguments& Out, const FCbFieldView& Field)
{
	const FString FieldName(Field.GetName());

	switch (FCbValue Accessor = Field.GetValue(); Accessor.GetType())
	{
	case ECbFieldType::IntegerPositive:
		Out.Emplace(FieldName, Accessor.AsIntegerPositive());
		return;
	case ECbFieldType::IntegerNegative:
		Out.Emplace(FieldName, Accessor.AsIntegerNegative());
		return;
	case ECbFieldType::Float32:
		Out.Emplace(FieldName, Accessor.AsFloat32());
		return;
	case ECbFieldType::Float64:
		Out.Emplace(FieldName, Accessor.AsFloat64());
		return;
	default:
		break;
	}

	// Handle anything that falls through as text.
	TStringBuilder<128> Text;
	LogFieldValue(Text, Field);
	Out.Emplace(FieldName, FText::FromString(FString(Text)));
}

} // UE::Logging::Private

namespace UE
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FLogTemplate
{
	using FLogField = Logging::Private::FLogField;

public:
	static FLogTemplate* CreateStatic(const Logging::Private::FStaticLogRecord& Log, const FLogField* Fields, int32 FieldCount)
	{
		return Create(Log.Format, Fields, FieldCount);
	}

	static FLogTemplate* CreateStatic(const Logging::Private::FStaticLocalizedLogRecord& Log, const FLogField* Fields, int32 FieldCount)
	{
		return CreateLocalized(Log.TextNamespace, Log.TextKey, Log.Format, Fields, FieldCount);
	}

	static FLogTemplate* Create(const TCHAR* Format, const FLogField* Fields = nullptr, int32 FieldCount = 0);

	static FLogTemplate* CreateLocalized(
		const TCHAR* TextNamespace,
		const TCHAR* TextKey,
		const TCHAR* Format,
		const FLogField* Fields = nullptr,
		int32 FieldCount = 0);

	static void Destroy(FLogTemplate* Template);

	const TCHAR* GetFormat() const { return StaticFormat; }

	FLogTemplate* GetNext() { return Next; }
	void SetNext(FLogTemplate* Template) { Next = Template; }

	uint8* GetOpData() { return (uint8*)(this + 1); }
	const uint8* GetOpData() const { return (const uint8*)(this + 1); }

	template <typename CharType>
	void FormatTo(TStringBuilderBase<CharType>& Out, const FCbFieldViewIterator& Fields) const;

	FText FormatToText(const FCbFieldViewIterator& Fields) const;

private:
	template <typename CharType>
	void FormatLocalizedTo(TStringBuilderBase<CharType>& Out, const FCbFieldViewIterator& Fields) const;

	FText FormatLocalizedToText(const FCbFieldViewIterator& Fields) const;

	inline constexpr explicit FLogTemplate(const TCHAR* Format)
		: StaticFormat(Format)
	{
	}

	~FLogTemplate() = default;
	FLogTemplate(const FLogTemplate&) = delete;
	FLogTemplate& operator=(const FLogTemplate&) = delete;

	const TCHAR* StaticFormat = nullptr;
	FLogTemplate* Next = nullptr;
};

FLogTemplate* FLogTemplate::Create(const TCHAR* Format, const FLogField* Fields, const int32 FieldCount)
{
	using namespace Logging::Private;

	const TConstArrayView<FLogField> FieldsView(Fields, FieldCount);
	const bool bFindFields = !!Fields;
	const bool bPositional = !FieldCount || Algo::NoneOf(FieldsView, &FLogField::Name);
	checkf(bPositional || Algo::AllOf(FieldsView, &FLogField::Name),
		TEXT("Log fields must be entirely named or entirely anonymous. [[%s]]"), Format);
	checkf(bPositional || Algo::AllOf(FieldsView,
		[](const FLogField& Field) { return *Field.Name && *Field.Name != '_' && FAsciiSet::HasOnly(Field.Name, ValidLogFieldName); }),
		TEXT("Log field names must match \"[A-Za-z0-9][A-Za-z0-9_]*\" in [[%s]]."), Format);

	TArray<FLogTemplateOp, TInlineAllocator<16>> Ops;

	int32 FormatFieldCount = 0;
	int32 BracketSearchOffset = 0;
	for (const TCHAR* TextStart = Format;;)
	{
		constexpr FAsciiSet Brackets("{}");
		const TCHAR* const TextEnd = FAsciiSet::FindFirstOrEnd(TextStart + BracketSearchOffset, Brackets);
		BracketSearchOffset = 0;

		// Escaped "{{" or "}}"
		if ((TextEnd[0] == TEXT('{') && TextEnd[1] == TEXT('{')) ||
			(TextEnd[0] == TEXT('}') && TextEnd[1] == TEXT('}')))
		{
			// Only "{{" or "}}"
			if (TextStart == TextEnd)
			{
				Ops.Add({FLogTemplateOp::OpSkip, 1});
				TextStart = TextEnd + 1;
				BracketSearchOffset = 1;
			}
			// Text and "{{" or "}}"
			else
			{
				Ops.Add({FLogTemplateOp::OpText, UE_PTRDIFF_TO_INT32(1 + TextEnd - TextStart)});
				Ops.Add({FLogTemplateOp::OpSkip, 1});
				TextStart = TextEnd + 2;
			}
			continue;
		}

		// Text
		if (TextStart != TextEnd)
		{
			Ops.Add({FLogTemplateOp::OpText, UE_PTRDIFF_TO_INT32(TextEnd - TextStart)});
			TextStart = TextEnd;
		}

		// End
		if (!*TextEnd)
		{
			Ops.Add({FLogTemplateOp::OpEnd});
			break;
		}

		// Invalid '}'
		checkf(*TextStart == TEXT('{'), TEXT("Log format has an unexpected '}' character. Use '}}' to escape it. [[%s]]"), Format);

		// Field
		const TCHAR* const FieldStart = TextStart;
		const TCHAR* const FieldNameEnd = FAsciiSet::Skip(FieldStart + 1, ValidLogFieldName);
		checkf(*FieldNameEnd, TEXT("Log format has an unterminated field reference. Use '{{' to escape '{' if needed. [[%s]]"), Format);
		checkf(*FieldNameEnd == TEXT('}'), TEXT("Log format has invalid character '%c' in field name. "
			"Use '{{' to escape '{' if needed. Names must match \"[A-Za-z0-9][A-Za-z0-9_]*\". [[%s]]"), *FieldNameEnd, Format);
		const TCHAR* const FieldEnd = FieldNameEnd + 1;
		const int32 FieldLen = UE_PTRDIFF_TO_INT32(FieldEnd - FieldStart);
		const int32 FieldNameLen = FieldLen - 2;
		checkf(FieldStart[1] != TEXT('_'), TEXT("Log format uses reserved field name '%.*s' with leading '_'. "
			"Names must match \"[A-Za-z0-9][A-Za-z0-9_]*\". [[%s]]"), FieldNameLen, FieldStart + 1, Format);

		if (bFindFields && !bPositional)
		{
			bool bFoundField = false;
			for (int32 BaseFieldIndex = 0; BaseFieldIndex < FieldCount; ++BaseFieldIndex)
			{
				const int32 FieldIndex = (FormatFieldCount + BaseFieldIndex) % FieldCount;
				const ANSICHAR* FieldName = Fields[FieldIndex].Name;
				if (FPlatformString::Strncmp(FieldName, FieldStart + 1, FieldNameLen) == 0 && !FieldName[FieldNameLen])
				{
					Ops.Add({FLogTemplateOp::OpIndex, FieldIndex});
					bFoundField = true;
					break;
				}
			}
			checkf(bFoundField, TEXT("Log format requires field '%.*s' which was not provided. [[%s]]"),
				FieldNameLen, FieldStart + 1, Format);
		}

		Ops.Add({FLogTemplateOp::OpName, FieldLen});
		++FormatFieldCount;

		TextStart = FieldEnd;
	}

	checkf(!bFindFields || !bPositional || FormatFieldCount == FieldCount,
		TEXT("Log format requires %d fields and %d were provided. [[%s]]"), FormatFieldCount, FieldCount, Format);

	const uint32 TotalSize = sizeof(FLogTemplate) + Algo::TransformAccumulate(Ops, FLogTemplateOp::SaveSize, 0);
	FLogTemplate* const Template = new(FMemory::Malloc(TotalSize, alignof(FLogTemplate))) FLogTemplate(Format);
	uint8* Data = Template->GetOpData();
	for (const FLogTemplateOp& Op : Ops)
	{
		FLogTemplateOp::Save(Op, Data);
	}
	return Template;
}

FLogTemplate* FLogTemplate::CreateLocalized(const TCHAR* TextNamespace, const TCHAR* TextKey, const TCHAR* Format, const FLogField* Fields, const int32 FieldCount)
{
	using namespace Logging::Private;

	// Disallow escape sequences and argument modifiers.
	// These are inconsistent with non-localized format strings and are disallowed for now.
	constexpr FAsciiSet Invalid("`|");
	checkf(FAsciiSet::HasNone(Format, Invalid),
		TEXT("Log format does not currently allow escapes (`) or argument modifiers (|). [[%s]]"), Format);

	FTextFormat TextFormat(FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(Format, TextNamespace, TextKey));

	const bool bFindFields = !!Fields;
	if (bFindFields)
	{
		int32 FormatFieldCount = 0;
		TArray<FString> TextFormatFieldNames;
		TextFormat.GetFormatArgumentNames(TextFormatFieldNames);
		for (const FString& FormatName : TextFormatFieldNames)
		{
			bool bFoundField = false;
			const TCHAR* FormatNameStr = *FormatName;
			const int32 FormatNameLen = FormatName.Len();
			for (int32 BaseFieldIndex = 0; BaseFieldIndex < FieldCount; ++BaseFieldIndex)
			{
				const int32 FieldIndex = (FormatFieldCount + BaseFieldIndex) % FieldCount;
				const ANSICHAR* FieldName = Fields[FieldIndex].Name;
				if (FPlatformString::Strncmp(FieldName, FormatNameStr, FormatNameLen) == 0 && !FieldName[FormatNameLen])
				{
					bFoundField = true;
					break;
				}
			}
			checkf(bFoundField, TEXT("Log format requires field '%s' which was not provided. [[%s]]"), FormatNameStr, Format);
			++FormatFieldCount;
		}
	}

	const FLogTemplateOp Ops[]{{FLogTemplateOp::OpLocText}, {FLogTemplateOp::OpEnd}};
	const uint32 TotalSize = sizeof(FTextFormat) + sizeof(FLogTemplate) + Algo::TransformAccumulate(Ops, FLogTemplateOp::SaveSize, 0);
	FTextFormat* NewTextFormat = new(FMemory::Malloc(TotalSize, uint32(FPlatformMath::Max(alignof(FTextFormat), alignof(FLogTemplate))))) FTextFormat(MoveTemp(TextFormat));
	FLogTemplate* const Template = new(NewTextFormat + 1) FLogTemplate(Format); //-V752
	uint8* Data = Template->GetOpData();
	for (const FLogTemplateOp& Op : Ops)
	{
		FLogTemplateOp::Save(Op, Data);
	}
	return Template;
}

void FLogTemplate::Destroy(FLogTemplate* Template)
{
	using namespace Logging::Private;

	const uint8* NextOp = Template->GetOpData();
	if (FLogTemplateOp::Load(NextOp).Code == FLogTemplateOp::OpLocText)
	{
		FTextFormat* TextFormat = (FTextFormat*)Template - 1;
		TextFormat->~FTextFormat();
		FMemory::Free(TextFormat);
	}
	else
	{
		FMemory::Free(Template);
	}
}

template <typename CharType>
void FLogTemplate::FormatTo(TStringBuilderBase<CharType>& Out, const FCbFieldViewIterator& Fields) const
{
	using namespace Logging::Private;

	auto FindField = [&Fields, It = Fields, Index = 0, Format = StaticFormat](FAnsiStringView Name, int32 IndexHint = -1) mutable -> FCbFieldView&
	{
		if (IndexHint >= 0)
		{
			for (; Index < IndexHint && It; ++Index, ++It)
			{
			}
			if (IndexHint < Index)
			{
				It = Fields;
				for (Index = 0; Index < IndexHint && It; ++Index, ++It);
			}
			if (IndexHint == Index && Name.Equals(It.GetName()))
			{
				return It;
			}
		}
		const int32 PrevIndex = Index;
		for (; It; ++Index, ++It)
		{
			if (Name.Equals(It.GetName()))
			{
				return It;
			}
		}
		It = Fields;
		for (Index = 0; Index < PrevIndex && It; ++Index, ++It)
		{
			if (Name.Equals(It.GetName()))
			{
				return It;
			}
		}
		checkf(false, TEXT("Log format requires field '%.*hs' which was not provided. [[%s]]"), Name.Len(), Name.GetData(), Format);
		return It;
	};

	int32 FieldIndexHint = -1;
	const uint8* NextOp = GetOpData();
	const TCHAR* NextFormat = StaticFormat;
	for (;;)
	{
		const FLogTemplateOp Op = FLogTemplateOp::Load(NextOp);
		switch (Op.Code)
		{
		case FLogTemplateOp::OpLocText:
			return FormatLocalizedTo(Out, Fields);
		case FLogTemplateOp::OpEnd:
			return;
		case FLogTemplateOp::OpText:
			Out.Append(NextFormat, Op.Value);
			break;
		case FLogTemplateOp::OpIndex:
			FieldIndexHint = Op.Value;
			break;
		case FLogTemplateOp::OpName:
			const auto Name = StringCast<ANSICHAR>(NextFormat + 1, Op.Value - 2);
			LogFieldValue(Out, FindField(MakeStringView(Name.Get(), Name.Length()), FieldIndexHint));
			FieldIndexHint = -1;
			break;
		}
		NextFormat += Op.GetSkipSize();
	}
}

FText FLogTemplate::FormatToText(const FCbFieldViewIterator& Fields) const
{
	using namespace Logging::Private;

	const uint8* NextOp = GetOpData();
	if (FLogTemplateOp::Load(NextOp).Code == FLogTemplateOp::OpLocText)
	{
		return FormatLocalizedToText(Fields);
	}
	else
	{
		TStringBuilder<512> Builder;
		FormatTo(Builder, Fields);
		return FText::FromStringView(Builder);
	}
}

template <typename CharType>
FORCENOINLINE void FLogTemplate::FormatLocalizedTo(TStringBuilderBase<CharType>& Out, const FCbFieldViewIterator& Fields) const
{
	Out.Append(FormatLocalizedToText(Fields).ToString());
}

FText FLogTemplate::FormatLocalizedToText(const FCbFieldViewIterator& Fields) const
{
	using namespace Logging::Private;

	FFormatNamedArguments TextFormatArguments;
	for (const FCbFieldView& Field : Fields)
	{
		AddFieldValue(TextFormatArguments, Field);
	}

	const FTextFormat* TextFormat = (const FTextFormat*)this - 1;
	return FText::Format(*TextFormat, TextFormatArguments);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FLogTemplate* CreateLogTemplate(const TCHAR* Format)
{
	return FLogTemplate::Create(Format);
}

FLogTemplate* CreateLogTemplate(const TCHAR* TextNamespace, const TCHAR* TextKey, const TCHAR* Format)
{
	return FLogTemplate::CreateLocalized(TextNamespace, TextKey, Format);
}

void DestroyLogTemplate(FLogTemplate* Template)
{
	FLogTemplate::Destroy(Template);
}

void FormatLogTo(FUtf8StringBuilderBase& Out, const FLogTemplate* Template, const FCbFieldViewIterator& Fields)
{
	Template->FormatTo(Out, Fields);
}

void FormatLogTo(FWideStringBuilderBase& Out, const FLogTemplate* Template, const FCbFieldViewIterator& Fields)
{
	Template->FormatTo(Out, Fields);
}

FText FormatLogToText(const FLogTemplate* Template, const FCbFieldViewIterator& Fields)
{
	return Template->FormatToText(Fields);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FLogTime FLogTime::Now()
{
	FLogTime Time;
	Time.UtcTicks = FDateTime::UtcNow().GetTicks();
	return Time;
}

FDateTime FLogTime::GetUtcTime() const
{
	return FDateTime(UtcTicks);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename CharType>
FORCENOINLINE static void FormatDynamicRecordMessageTo(TStringBuilderBase<CharType>& Out, const FLogRecord& Record)
{
	const TCHAR* Format = Record.GetFormat();
	if (UNLIKELY(!Format))
	{
		return;
	}

	const TCHAR* TextNamespace = Record.GetTextNamespace();
	const TCHAR* TextKey = Record.GetTextKey();
	checkf(!TextNamespace == !TextKey,
		TEXT("Log record must have both or neither of the text namespace and text key. [[%s]]"), Format);

	FLogTemplate* LocalTemplate = TextKey ? FLogTemplate::CreateLocalized(TextNamespace, TextKey, Format) : FLogTemplate::Create(Format);
	LocalTemplate->FormatTo(Out, Record.GetFields().CreateViewIterator());
	FLogTemplate::Destroy(LocalTemplate);
}

template <typename CharType>
static void FormatRecordMessageTo(TStringBuilderBase<CharType>& Out, const FLogRecord& Record)
{
	const FLogTemplate* Template = Record.GetTemplate();
	if (LIKELY(Template))
	{
		return Template->FormatTo(Out, Record.GetFields().CreateViewIterator());
	}
	FormatDynamicRecordMessageTo(Out, Record);
}

void FLogRecord::FormatMessageTo(FUtf8StringBuilderBase& Out) const
{
	FormatRecordMessageTo(Out, *this);
}

void FLogRecord::FormatMessageTo(FWideStringBuilderBase& Out) const
{
	FormatRecordMessageTo(Out, *this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // UE

#if !NO_LOGGING

namespace UE::Logging::Private
{

class FLogTemplateFieldIterator
{
public:
	inline explicit FLogTemplateFieldIterator(const FLogTemplate& Template)
		: NextOp(Template.GetOpData())
		, NextFormat(Template.GetFormat())
	{
		++*this;
	}

	FLogTemplateFieldIterator& operator++();
	inline explicit operator bool() const { return !!NextOp; }
	inline const FStringView& GetName() const { return Name; }

private:
	FStringView Name;
	const uint8* NextOp = nullptr;
	const TCHAR* NextFormat = nullptr;
};

FLogTemplateFieldIterator& FLogTemplateFieldIterator::operator++()
{
	using namespace Logging::Private;

	while (NextOp)
	{
		FLogTemplateOp Op = FLogTemplateOp::Load(NextOp);
		if (Op.Code == FLogTemplateOp::OpName)
		{
			Name = FStringView(NextFormat + 1, Op.Value - 2);
			NextFormat += Op.GetSkipSize();
			return *this;
		}
		if (Op.Code == FLogTemplateOp::OpEnd)
		{
			break;
		}
		NextFormat += Op.GetSkipSize();
	}

	NextOp = nullptr;
	Name.Reset();
	return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct FStaticLogTemplateManager
{
	std::atomic<FLogTemplate*> Head = nullptr;

	~FStaticLogTemplateManager()
	{
		for (FLogTemplate* Template = Head.exchange(nullptr); Template;)
		{
			FLogTemplate* NextTemplate = Template->GetNext();
			FLogTemplate::Destroy(Template);
			Template = NextTemplate;
		}
	}
};

static FStaticLogTemplateManager GStaticLogTemplateManager;

// Tracing the log happens in its own function because that allows stack space for the message to
// be returned before calling into the output devices.
FORCENOINLINE static void LogToTrace(const void* LogPoint, const FLogRecord& Record)
{
#if LOGTRACE_ENABLED
	TStringBuilder<1024> Message;
	Record.FormatMessageTo(Message);
	FLogTrace::OutputLogMessage(LogPoint, *Message);
#endif
}

// Serializing log fields to compact binary happens in its own function because that allows stack
// space for the writer to be returned before calling into the output devices.
FORCENOINLINE static FCbObject SerializeLogFields(
	const FLogTemplate& Template,
	const FLogField* Fields,
	const int32 FieldCount)
{
	if (FieldCount == 0)
	{
		return FCbObject();
	}

	TCbWriter<1024> Writer;
	Writer.BeginObject();

	// Anonymous. Extract names from Template.
	if (!Fields->Name)
	{
		FLogTemplateFieldIterator It(Template);
		for (const FLogField* FieldsEnd = Fields + FieldCount; Fields != FieldsEnd; ++Fields, ++It)
		{
			check(It);
			const auto Name = StringCast<ANSICHAR>(It.GetName().GetData(), It.GetName().Len());
			Fields->WriteValue(Writer.SetName(MakeStringView(Name.Get(), Name.Length())), Fields->Value);
		}
		check(!It);
	}
	// Named
	else
	{
		for (const FLogField* FieldsEnd = Fields + FieldCount; Fields != FieldsEnd; ++Fields)
		{
			Fields->WriteValue(Writer.SetName(Fields->Name), Fields->Value);
		}
	}

	Writer.EndObject();
	return Writer.Save().AsObject();
}

template <typename StaticLogRecordType>
FORCENOINLINE static FLogTemplate& CreateLogTemplate(const FLogCategoryBase& Category, const StaticLogRecordType& Log, const FLogField* Fields, const int32 FieldCount)
{
#if LOGTRACE_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(LogChannel))
	{
		FLogTrace::OutputLogMessageSpec(&Log, &Category, Log.Verbosity, Log.File, Log.Line, TEXT("%s"));
	}
#endif

	for (FLogTemplate* Template = Log.DynamicData.Template.load(std::memory_order_acquire);;)
	{
		if (Template && Template->GetFormat() == Log.Format)
		{
			return *Template;
		}

		FLogTemplate* NewTemplate = FLogTemplate::CreateStatic(Log, Fields, FieldCount);
		if (LIKELY(Log.DynamicData.Template.compare_exchange_strong(Template, NewTemplate, std::memory_order_release, std::memory_order_acquire)))
		{
			// Register the template to destroy on exit.
			for (FLogTemplate* Head = GStaticLogTemplateManager.Head.load(std::memory_order_relaxed);;)
			{
				NewTemplate->SetNext(Head);
				if (LIKELY(GStaticLogTemplateManager.Head.compare_exchange_weak(Head, NewTemplate, std::memory_order_release, std::memory_order_relaxed)))
				{
					break;
				}
			}
			return *NewTemplate;
		}
		FLogTemplate::Destroy(NewTemplate);
	}
}

template <typename StaticLogRecordType>
inline static FLogTemplate& EnsureLogTemplate(const FLogCategoryBase& Category, const StaticLogRecordType& Log, const FLogField* Fields, const int32 FieldCount)
{
	// Format can change on a static log record due to Live Coding.
	if (FLogTemplate* Template = Log.DynamicData.Template.load(std::memory_order_acquire); LIKELY(Template && Template->GetFormat() == Log.Format))
	{
		return *Template;
	}
	return CreateLogTemplate(Category, Log, Fields, FieldCount);
}

template <typename StaticLogRecordType>
inline static FLogRecord CreateLogRecord(const FLogCategoryBase& Category, const StaticLogRecordType& Log, const FLogField* Fields, const int32 FieldCount)
{
	FLogTemplate& Template = EnsureLogTemplate(Category, Log, Fields, FieldCount);

	FLogRecord Record;
	Record.SetFormat(Log.Format);
	Record.SetTemplate(&Template);
	Record.SetFields(SerializeLogFields(Template, Fields, FieldCount));
	Record.SetFile(Log.File);
	Record.SetLine(Log.Line);
	Record.SetCategory(Category.GetCategoryName());
	Record.SetVerbosity(Log.Verbosity);
	Record.SetTime(FLogTime::Now());
	return Record;
}

template <typename StaticLogRecordType>
inline static void DispatchLogRecord(const StaticLogRecordType& Log, const FLogRecord& Record)
{
#if LOGTRACE_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(LogChannel))
	{
		LogToTrace(&Log, Record);
	}
#endif

	FOutputDevice* OutputDevice = nullptr;
	switch (Log.Verbosity)
	{
	case ELogVerbosity::Error:
	case ELogVerbosity::Warning:
	case ELogVerbosity::Display:
	case ELogVerbosity::SetColor:
		OutputDevice = GWarn;
		break;
	default:
		break;
	}
	(OutputDevice ? OutputDevice : GLog)->SerializeRecord(Record);
}

void LogWithFieldArray(const FLogCategoryBase& Category, const FStaticLogRecord& Log, const FLogField* Fields, const int32 FieldCount)
{
#if !NO_LOGGING
	DispatchLogRecord(Log, CreateLogRecord(Category, Log, Fields, FieldCount));
#endif
}

void LogWithNoFields(const FLogCategoryBase& Category, const FStaticLogRecord& Log)
{
#if !NO_LOGGING
	// A non-null field pointer enables field validation in FLogTemplate::Create.
	static constexpr FLogField EmptyField{};
	LogWithFieldArray(Category, Log, &EmptyField, 0);
#endif
}

void LogWithFieldArray(const FLogCategoryBase& Category, const FStaticLocalizedLogRecord& Log, const FLogField* Fields, const int32 FieldCount)
{
#if !NO_LOGGING
	FLogRecord Record = CreateLogRecord(Category, Log, Fields, FieldCount);
	Record.SetTextNamespace(Log.TextNamespace);
	Record.SetTextKey(Log.TextKey);
	DispatchLogRecord(Log, Record);
#endif
}

void LogWithNoFields(const FLogCategoryBase& Category, const FStaticLocalizedLogRecord& Log)
{
#if !NO_LOGGING
	LogWithFieldArray(Category, Log, nullptr, 0);
#endif
}

void FatalLogWithFieldArray(const FLogCategoryBase& Category, const FStaticLogRecord& Log, const FLogField* Fields, const int32 FieldCount)
{
#if !NO_LOGGING
	FLogRecord Record = CreateLogRecord(Category, Log, Fields, FieldCount);
	TStringBuilder<512> Message;
	Record.FormatMessageTo(Message);

	StaticFailDebug(TEXT("Fatal error:"), "", Log.File, Log.Line, /*bIsEnsure*/ false, PLATFORM_RETURN_ADDRESS(), TEXT("%s"), *Message);

	UE_DEBUG_BREAK_AND_PROMPT_FOR_REMOTE();
	FDebug::ProcessFatalError(PLATFORM_RETURN_ADDRESS());
#endif
}

void FatalLogWithNoFields(const FLogCategoryBase& Category, const FStaticLogRecord& Log)
{
#if !NO_LOGGING
	// A non-null field pointer enables field validation in FLogTemplate::Create.
	static constexpr FLogField EmptyField{};
	FatalLogWithFieldArray(Category, Log, &EmptyField, 0);
#endif
}

void FatalLogWithFieldArray(const FLogCategoryBase& Category, const FStaticLocalizedLogRecord& Log, const FLogField* Fields, const int32 FieldCount)
{
#if !NO_LOGGING
	FLogRecord Record = CreateLogRecord(Category, Log, Fields, FieldCount);
	TStringBuilder<512> Message;
	Record.FormatMessageTo(Message);

	StaticFailDebug(TEXT("Fatal error:"), "", Log.File, Log.Line, /*bIsEnsure*/ false, PLATFORM_RETURN_ADDRESS(), TEXT("%s"), *Message);

	UE_DEBUG_BREAK_AND_PROMPT_FOR_REMOTE();
	FDebug::ProcessFatalError(PLATFORM_RETURN_ADDRESS());
#endif
}

void FatalLogWithNoFields(const FLogCategoryBase& Category, const FStaticLocalizedLogRecord& Log)
{
#if !NO_LOGGING
	// A non-null field pointer enables field validation in FLogTemplate::Create.
	static constexpr FLogField EmptyField{};
	FatalLogWithFieldArray(Category, Log, &EmptyField, 0);
#endif
}

} // UE::Logging::Private

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace UE::Logging::Private
{

static constexpr const TCHAR* const GStaticBasicLogFormat = TEXT("{Message}");

class FStaticBasicLogTemplateManager
{
public:
	~FStaticBasicLogTemplateManager()
	{
		if (FLogTemplate* LocalTemplate = Template.exchange(nullptr))
		{
			FLogTemplate::Destroy(LocalTemplate);
		}
	}

	inline FLogTemplate& EnsureTemplate()
	{
		if (FLogTemplate* LocalTemplate = Template.load(std::memory_order_acquire); LIKELY(LocalTemplate))
		{
			return *LocalTemplate;
		}
		return CreateTemplate();
	}

private:
	FORCENOINLINE FLogTemplate& CreateTemplate()
	{
		FLogTemplate* NewTemplate = FLogTemplate::Create(GStaticBasicLogFormat);
		if (FLogTemplate* ExistingTemplate = nullptr;
			UNLIKELY(!Template.compare_exchange_strong(ExistingTemplate, NewTemplate, std::memory_order_release, std::memory_order_acquire)))
		{
			FLogTemplate::Destroy(NewTemplate);
			return *ExistingTemplate;
		}
		return *NewTemplate;
	}

	std::atomic<FLogTemplate*> Template = nullptr;
};

static FStaticBasicLogTemplateManager GStaticBasicLogTemplateManager;

FORCENOINLINE static void OutputBasicLogMessageSpec(const FLogCategoryBase& Category, const FStaticBasicLogRecord& Log)
{
#if LOGTRACE_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(LogChannel))
	{
		FLogTrace::OutputLogMessageSpec(&Log, &Category, Log.Verbosity, Log.File, Log.Line, TEXT("%s"));
	}
#endif
	Log.DynamicData.bInitialized.store(true, std::memory_order_release);
}

inline static void EnsureBasicLogMessageSpec(const FLogCategoryBase& Category, const FStaticBasicLogRecord& Log)
{
	if (!Log.DynamicData.bInitialized.load(std::memory_order_acquire))
	{
		OutputBasicLogMessageSpec(Category, Log);
	}
}

// Tracing the log happens in its own function because that allows stack space for the message to
// be returned before calling into the output devices.
FORCENOINLINE static void BasicLogToTrace(const void* LogPoint, const TCHAR* Format, va_list Args)
{
#if LOGTRACE_ENABLED
	TStringBuilder<1024> Message;
	Message.AppendV(Format, Args);
	FLogTrace::OutputLogMessage(LogPoint, *Message);
#endif
}

// Serializing the log to compact binary happens in its own function because that allows stack
// space for the writer to be returned before calling into the output devices.
FORCENOINLINE static FCbObject SerializeBasicLogMessage(const FStaticBasicLogRecord& Log, va_list Args)
{
	TStringBuilder<512> Message;
	Message.AppendV(Log.Format, Args);

#if LOGTRACE_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(LogChannel))
	{
		FLogTrace::OutputLogMessage(&Log, *Message);
	}
#endif

	TCbWriter<512> Writer;
	Writer.BeginObject();
	Writer.AddString(ANSITEXTVIEW("Message"), Message);
	Writer.EndObject();
	return Writer.Save().AsObject();
}

void BasicLog(const FLogCategoryBase& Category, const FStaticBasicLogRecord* Log, ...)
{
#if !NO_LOGGING
	EnsureBasicLogMessageSpec(Category, *Log);

	if (GConvertBasicLogToLogRecord)
	{
		va_list Args;
		va_start(Args, Log);
		FCbObject Fields = SerializeBasicLogMessage(*Log, Args);
		va_end(Args);

		FLogRecord Record;
		Record.SetFormat(GStaticBasicLogFormat);
		Record.SetTemplate(&GStaticBasicLogTemplateManager.EnsureTemplate());
		Record.SetFields(MoveTemp(Fields));
		Record.SetFile(Log->File);
		Record.SetLine(Log->Line);
		Record.SetCategory(Category.GetCategoryName());
		Record.SetVerbosity(Log->Verbosity);
		Record.SetTime(FLogTime::Now());

		FOutputDevice* OutputDevice = nullptr;
		switch (Log->Verbosity)
		{
		case ELogVerbosity::Error:
		case ELogVerbosity::Warning:
		case ELogVerbosity::Display:
		case ELogVerbosity::SetColor:
			OutputDevice = GWarn;
			break;
		default:
			break;
		}
		(OutputDevice ? OutputDevice : GLog)->SerializeRecord(Record);
	}
	else
	{
		va_list Args;
	#if LOGTRACE_ENABLED
		if (UE_TRACE_CHANNELEXPR_IS_ENABLED(LogChannel))
		{
			va_start(Args, Log);
			BasicLogToTrace(Log, Log->Format, Args);
			va_end(Args);
		}
	#endif
		va_start(Args, Log);
		FMsg::LogV(Log->File, Log->Line, Category.GetCategoryName(), Log->Verbosity, Log->Format, Args);
		va_end(Args);
	}
#endif
}

void BasicFatalLog(const FLogCategoryBase& Category, const FStaticBasicLogRecord* Log, ...)
{
#if !NO_LOGGING
	va_list Args;
	va_start(Args, Log);
	StaticFailDebugV(TEXT("Fatal error:"), "", Log->File, Log->Line, /*bIsEnsure*/ false, PLATFORM_RETURN_ADDRESS(), Log->Format, Args);
	va_end(Args);

	UE_DEBUG_BREAK_AND_PROMPT_FOR_REMOTE();
	FDebug::ProcessFatalError(PLATFORM_RETURN_ADDRESS());
#endif
}

} // UE::Logging::Private

#endif // !NO_LOGGING
