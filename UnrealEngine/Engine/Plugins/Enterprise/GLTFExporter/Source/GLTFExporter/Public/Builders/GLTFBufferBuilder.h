// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFJsonBuilder.h"

class GLTFEXPORTER_API FGLTFBufferBuilder : public FGLTFJsonBuilder
{
protected:

	FGLTFBufferBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions = nullptr);
	~FGLTFBufferBuilder();

	const FGLTFMemoryArchive* GetBufferData() const;

public:

	FGLTFJsonBufferView* AddBufferView(const void* RawData, uint64 ByteLength, EGLTFJsonBufferTarget BufferTarget = EGLTFJsonBufferTarget::None, uint8 DataAlignment = 4);

	template <class ElementType>
	FGLTFJsonBufferView* AddBufferView(const TArray<ElementType>& Array, EGLTFJsonBufferTarget BufferTarget = EGLTFJsonBufferTarget::None, uint8 DataAlignment = 4)
	{
		return AddBufferView(Array.GetData(), Array.Num() * sizeof(ElementType), BufferTarget, DataAlignment);
	}

private:

	void InitializeBuffer();

	FGLTFJsonBuffer* JsonBuffer;
	TSharedPtr<FGLTFMemoryArchive> BufferArchive;
};
