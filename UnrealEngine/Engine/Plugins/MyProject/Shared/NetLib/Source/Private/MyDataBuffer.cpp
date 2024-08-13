#include "MyDataBuffer.h"


FMyDataBuffer::FMyDataBuffer(const uint32 InitialSize)
	: Data(InitialSize), ReaderIndex(0), WriterIndex(0)
{
	check(ReadableBytes() == 0);
	check(WritableBytes() == InitialSize);
	check(PrependableBytes() == 0);
}

FMyDataBuffer::FMyDataBuffer(const char* InData, const uint32 Len)
	: Data(Len), ReaderIndex(0), WriterIndex(0)
{
	Append(InData, Len);
}

FMyDataBuffer::FMyDataBuffer(const void* InData, const uint32 Len)
	: Data(Len), ReaderIndex(0), WriterIndex(0)
{
	Append(InData, Len);
}

FMyDataBuffer::~FMyDataBuffer()
{
}

void FMyDataBuffer::Swap(FMyDataBuffer& Rhs)
{
	Data.swap(Rhs.Data);
	std::swap(ReaderIndex, Rhs.ReaderIndex);
	std::swap(WriterIndex, Rhs.WriterIndex);
}

uint32 FMyDataBuffer::ReadableBytes() const
{
	return WriterIndex - ReaderIndex;
}

uint32 FMyDataBuffer::WritableBytes() const
{
	return Data.size() - WriterIndex;
}

uint32 FMyDataBuffer::PrependableBytes() const
{
	return ReaderIndex;
}

const char* FMyDataBuffer::Peek() const
{
	return Begin() + ReaderIndex;
}

void FMyDataBuffer::Retrieve(const uint32 InLen)
{
	check(InLen <= ReadableBytes());
	if (InLen < ReadableBytes())
	{
		ReaderIndex += InLen;
	}
	else
	{
		RetrieveAll();
	}
}

void FMyDataBuffer::RetrieveUntil(const char* EndPtr)
{
	check(Peek() <= EndPtr);
	check(EndPtr <= BeginWrite());
	Retrieve(EndPtr - Peek());
}

void FMyDataBuffer::RetrieveInt64()
{
	Retrieve(sizeof(int64));
}

void FMyDataBuffer::RetrieveInt32()
{
	Retrieve(sizeof(int32_t));
}

void FMyDataBuffer::RetrieveInt16()
{
	Retrieve(sizeof(int16_t));
}

void FMyDataBuffer::RetrieveInt8()
{
	Retrieve(sizeof(int8_t));
}

void FMyDataBuffer::RetrieveAll()
{
	ReaderIndex = 0;
	WriterIndex = 0;
}

std::string FMyDataBuffer::RetrieveAllAsString()
{
	return RetrieveAsString(ReadableBytes());
}

std::string FMyDataBuffer::RetrieveAsString(const uint32 Len)
{
	check(Len <= ReadableBytes());
	std::string Result(Peek(), Len);
	Retrieve(Len);
	return Result;
}

void FMyDataBuffer::Append(const char* InData, const uint32 Len)
{
	EnsureWritableBytes(Len);
	std::copy(InData, InData + Len, BeginWrite());
	HasWritten(Len);
}

void FMyDataBuffer::Append(const void* InData, const uint32 Len)
{
	Append(static_cast<const char*>(InData), Len);
}

void FMyDataBuffer::EnsureWritableBytes(const uint32 Len)
{
	if (WritableBytes() < Len)
	{
		MakeSpace(Len);
	}
	check(WritableBytes() >= Len);
}

char* FMyDataBuffer::BeginWrite()
{
	return Begin() + WriterIndex;
}

const char* FMyDataBuffer::BeginWrite() const
{
	return Begin() + WriterIndex;
}

void FMyDataBuffer::HasWritten(const uint32 Len)
{
	check(Len <= WritableBytes());
	WriterIndex += Len;
}

void FMyDataBuffer::UnWrite(const uint32 Len)
{
	check(Len <= ReadableBytes());
	WriterIndex -= Len;
}

void FMyDataBuffer::AppendInt64(const int64 InVal)
{
	Append(&InVal, sizeof InVal);
}

void FMyDataBuffer::AppendInt32(const int32 InVal)
{
	Append(&InVal, sizeof InVal);
}

void FMyDataBuffer::AppendInt16(const int16 InVal)
{
	Append(&InVal, sizeof InVal);
}

void FMyDataBuffer::AppendInt8(const int8 InVal)
{
	Append(&InVal, sizeof InVal);
}

int64 FMyDataBuffer::ReadInt64()
{
	const int64 Result = PeekInt64();
	RetrieveInt64();
	return Result;
}

int32 FMyDataBuffer::ReadInt32()
{
	const int32 Result = PeekInt32();
	RetrieveInt32();
	return Result;
}

int16 FMyDataBuffer::ReadInt16()
{
	const int16 Result = PeekInt16();
	RetrieveInt16();
	return Result;
}

int8 FMyDataBuffer::ReadInt8()
{
	const int8 Result = PeekInt8();
	RetrieveInt8();
	return Result;
}

int64 FMyDataBuffer::PeekInt64() const
{
	check(ReadableBytes() >= sizeof(int64));
	int64 Val = 0;
	::memcpy(&Val, Peek(), sizeof Val);
	return Val;
}

int32 FMyDataBuffer::PeekInt32() const
{
	check(ReadableBytes() >= sizeof(int32_t));
	int32 Val = 0;
	::memcpy(&Val, Peek(), sizeof Val);
	return Val;
}

int16 FMyDataBuffer::PeekInt16() const
{
	check(ReadableBytes() >= sizeof(int16_t));
	int16 Val = 0;
	::memcpy(&Val, Peek(), sizeof Val);
	return Val;
}

int8 FMyDataBuffer::PeekInt8() const
{
	check(ReadableBytes() >= sizeof(int8_t));
	int8 Val = *Peek();
	return Val;
}


void FMyDataBuffer::PrependInt64(const int64 InVal)
{
	Prepend(&InVal, sizeof InVal);
}

void FMyDataBuffer::PrependInt32(const int32 InVal)
{
	Prepend(&InVal, sizeof InVal);
}

void FMyDataBuffer::PrependInt16(const int16 InVal)
{
	Prepend(&InVal, sizeof InVal);
}

void FMyDataBuffer::PrependInt8(const int8 InVal)
{
	Prepend(&InVal, sizeof InVal);
}

void FMyDataBuffer::Prepend(const void* InData, const uint32 Len)
{
	check(Len <= PrependableBytes());
	ReaderIndex -= Len;
	const char* d = static_cast<const char*>(InData);
	std::copy(d, d + Len, Begin() + ReaderIndex);
}

char* FMyDataBuffer::Begin()
{
	return &*Data.begin();
}

const char* FMyDataBuffer::Begin() const
{
	return &*Data.begin();
}

void FMyDataBuffer::MakeSpace(const uint32 Len)
{
	if (WritableBytes() + PrependableBytes() < Len)
	{
		constexpr uint32 BaseBytes = 1024;
		Data.resize(((WriterIndex + Len) / BaseBytes + 1) * BaseBytes);
	}
	else
	{
		const uint32 Readable = ReadableBytes();
		FMemory::Memmove(Begin(), Begin() + ReaderIndex, Readable);
	
		ReaderIndex = 0;
		WriterIndex = ReaderIndex + Readable;
		check(Readable == ReadableBytes());
	}
}
