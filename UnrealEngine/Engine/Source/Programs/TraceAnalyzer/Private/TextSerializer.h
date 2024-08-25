// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Trace/Trace.h" // TraceLog, for UE::Trace::FEventRef8

#include "Io.h"

namespace UE
{
namespace TraceAnalyzer
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTextSerializer
{
public:
	FTextSerializer();
	virtual ~FTextSerializer();

	virtual void AppendChar(const ANSICHAR Value) = 0;
	virtual void Append(const ANSICHAR* Text, int32 Len) = 0;
	virtual void Append(const ANSICHAR* Text) = 0;
	virtual void Appendf(const char* Format, ...) = 0;
	virtual bool Commit() = 0;

	//////////////////////////////////////////////////
	// NEW_EVENT

	void BeginNewEventHeader()
	{
		if (bWriteEventHeader)
		{
#if !UE_TRACE_ANALYSIS_DEBUG
			// Add an empty line before a NEW_EVENT (if previous was an EVENT) to make it more visible.
			if (!bLastWasNewEvent)
			{
				AppendChar('\n');
			}
#endif
			bLastWasNewEvent = true;
			Append("\tNEW_EVENT : ");
		}
	}
	void EndNewEventHeader()
	{
		if (bWriteEventHeader)
		{
			AppendChar('\n');
		}
	}

	void BeginNewEventFields()
	{
	}
	void BeginField()
	{
		if (bWriteEventHeader)
		{
			Append("\t\tFIELD : ");
		}
		else
		{
			Append("\tFIELD : ");
		}
	}
	void EndField()
	{
		AppendChar('\n');
	}
	void EndNewEventFields()
	{
		// Add an extra empty line make each NEW_EVENT more visible.
		AppendChar('\n');
	}

	//////////////////////////////////////////////////
	// EVENT

	bool IsWriteEventHeaderEnabled() const { return bWriteEventHeader; }

	void BeginEvent(uint32 CtxThreadId)
	{
		if (bWriteEventHeader)
		{
			bLastWasNewEvent = false;
			if (CtxThreadId != (uint32)-1)
			{
				Appendf("\tEVENT [%u]", CtxThreadId);
			}
			else
			{
				Append("\tEVENT");
			}
		}
		else
		{
			AppendChar('\t');
		}
	}

	void WriteEventName(const char* LoggerName, const char* Name)
	{
		if (bWriteEventHeader)
		{
			Appendf(" %s.%s", LoggerName, Name);
		}
	}

	void BeginEventFields()
	{
		if (bWriteEventHeader)
		{
			Append(" : ");
		}
	}

	void NextEventField()
	{
		AppendChar(' ');
	}

	void EndEvent()
	{
		AppendChar('\n');
	}

	//////////////////////////////////////////////////
	// Array: [1 2 3...]

	void BeginArray()
	{
		AppendChar('[');
	}

	void NextArrayElement()
	{
		AppendChar(' ');
	}

	void EndArray()
	{
		AppendChar(']');
	}

	//////////////////////////////////////////////////
	// Values

	void WriteValueString(const char* Value)
	{
		AppendChar('\"');
		Append(Value);
		AppendChar('\"');
	}
	void WriteValueString(const char* Value, uint32 Len)
	{
		AppendChar('\"');
		Append(Value, Len);
		AppendChar('\"');
	}

	void WriteValueReference(const UE::Trace::FEventRef8& Value)  { Appendf("R(%u,%u)", Value.RefTypeId, uint32(Value.Id)); }
	void WriteValueReference(const UE::Trace::FEventRef16& Value) { Appendf("R(%u,%u)", Value.RefTypeId, uint32(Value.Id)); }
	void WriteValueReference(const UE::Trace::FEventRef32& Value) { Appendf("R(%u,%u)", Value.RefTypeId, Value.Id); }
	void WriteValueReference(const UE::Trace::FEventRef64& Value) { Appendf("R(%u,%llu)", Value.RefTypeId, Value.Id); }

	void WriteValueBool(bool Value)         { Append(Value ? "true" : "false"); }
	void WriteValueInteger(int64 Value)     { Appendf("%lli", Value); }
	void WriteValueIntegerHex(int64 Value)  { Appendf("0x%llX", uint64(Value)); }
	void WriteValueHex8(uint8 Value)        { Appendf("0x%X", uint32(Value)); }
	void WriteValueHex16(uint16 Value)      { Appendf("0x%X", uint32(Value)); }
	void WriteValueHex32(uint32 Value)      { Appendf("0x%X", Value); }
	void WriteValueHex64(uint64 Value)      { Appendf("0x%llX", Value); }
	void WriteValueFloat(float Value)       { Appendf("%f", Value); }
	void WriteValueDouble(double Value)     { Appendf("%f", Value); }
	void WriteValueTime(double Time)        { Appendf("%f", Time); }
	void WriteValueNull()                   { Append("null"); }

	void WriteValueBinary(const void* Data, uint32 Size);

	//////////////////////////////////////////////////
	// Key and Values

	void WriteKey(const ANSICHAR* Name)
	{
		Append(Name);
		AppendChar('=');
	}

	void WriteString(const ANSICHAR* Name, const ANSICHAR* Value)             { WriteKey(Name); WriteValueString(Value); }
	void WriteString(const ANSICHAR* Name, const ANSICHAR* Value, uint32 Len) { WriteKey(Name); WriteValueString(Value, Len); }
	void WriteBool(const ANSICHAR* Name, bool Value)                          { WriteKey(Name); WriteValueBool(Value); }
	void WriteInteger(const ANSICHAR* Name, int64 Value)                      { WriteKey(Name); WriteValueInteger(Value); }
	void WriteIntegerHex(const ANSICHAR* Name, int64 Value)                   { WriteKey(Name); WriteValueHex64(uint64(Value)); }
	void WriteFloat(const ANSICHAR* Name, float Value)                        { WriteKey(Name); WriteValueFloat(Value); }
	void WriteDouble(const ANSICHAR* Name, double Value)                      { WriteKey(Name); WriteValueDouble(Value); }
	void WriteNull(const ANSICHAR* Name)                                      { WriteKey(Name); WriteValueNull(); }
	void WriteBinary(const ANSICHAR* Name, const void* Data, uint32 Size)     { WriteKey(Name); WriteValueBinary(Data, Size); }

protected:
	bool bWriteEventHeader = true;
	bool bLastWasNewEvent = false;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FStdoutTextSerializer : public FTextSerializer
{
public:
	FStdoutTextSerializer();
	virtual ~FStdoutTextSerializer() {}
	
	virtual void AppendChar(const ANSICHAR Value) override;
	virtual void Append(const ANSICHAR* Text, int32 Len) override;
	virtual void Append(const ANSICHAR* Text) override;
	virtual void Appendf(const char* Format, ...) override;
	virtual bool Commit() override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFileTextSerializer : public FTextSerializer
{
public:
	FFileTextSerializer(FileHandle InHandle);
	virtual ~FFileTextSerializer();

	virtual void AppendChar(const ANSICHAR Value) override;
	virtual void Append(const ANSICHAR* Text, int32 Len) override;
	virtual void Append(const ANSICHAR* Text) override;
	virtual void Appendf(const char* Format, ...) override;
	virtual bool Commit() override;

private:
	void* GetPointer(uint32 RequiredSize);

private:
	FileHandle Handle = -1;
	static constexpr int BufferSize = 1024 * 1024;
	void* Buffer = nullptr;
	static constexpr int FormatBufferSize = 64 * 1024;
	void* FormatBuffer = nullptr;
	uint32 Used = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace TraceAnalyzer
} // namespace UE
