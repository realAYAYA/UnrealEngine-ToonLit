#pragma once

#include <vector>
#include <string>


class FMyDataBuffer
{

public:
	
	FMyDataBuffer(uint32 InitialSize = 1024);
	FMyDataBuffer(const char* InData, uint32 Len);
	FMyDataBuffer(const void* InData, uint32 Len);
	~FMyDataBuffer();

	void Swap(FMyDataBuffer& Rhs);

	uint32 ReadableBytes() const;  // 可读字节数
	uint32 WritableBytes() const;  // 可写字节数
	uint32 PrependableBytes() const;  // 已读字节数(读索引位置)

	// 移动读索引
	void Retrieve(uint32 InLen);
	void RetrieveUntil(const char* EndPtr);
	void RetrieveInt64();
	void RetrieveInt32();
	void RetrieveInt16();
	void RetrieveInt8();
	void RetrieveAll();
	std::string RetrieveAllAsString();
	std::string RetrieveAsString(uint32 Len);

	void EnsureWritableBytes(uint32 Len);  // 确保能写入Len个字节

	char* BeginWrite();
	const char* BeginWrite() const;
	void HasWritten(uint32 Len);  // 移动写索引 + len

	void UnWrite(uint32 Len);  // 移动写索引，回滚写入

	// 获取数据 (移动读索引)
	int64 ReadInt64();
	int32 ReadInt32();
	int16 ReadInt16();
	int8 ReadInt8();

	// 读取数据 (不移动读索引)
	const char* Peek() const;
	int64 PeekInt64() const;
	int32 PeekInt32() const;
	int16 PeekInt16() const;
	int8 PeekInt8() const;

	// 追加 (移动写索引)
	void Append(const char* InData, uint32 Len);
	void Append(const void* InData, uint32 Len);
	void AppendInt64(int64 InVal);
	void AppendInt32(int32 InVal);
	void AppendInt16(int16 InVal);
	void AppendInt8(int8 InVal);

	// 覆盖 (移动读索引)
	void Prepend(const void* InData, uint32 Len);
	void PrependInt64(int64 InVal);
	void PrependInt32(int32 InVal);
	void PrependInt16(int16 InVal);
	void PrependInt8(int8 InVal);

private:

	char* Begin();
	const char* Begin() const;
	void MakeSpace(uint32 Len);
	
	
	std::vector<char> Data;
	uint32 ReaderIndex = 0;  // 读索引
	uint32 WriterIndex = 0;  // 写索引
};
