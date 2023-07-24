// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEModelDataFactory.h"
#include "NNECoreModelData.h"

UNNEModelDataFactory::UNNEModelDataFactory(const FObjectInitializer& ObjectInitializer) : UFactory(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	SupportedClass = UNNEModelData::StaticClass();
	ImportPriority = DefaultImportPriority;
	Formats.Add("onnx;Open Neural Network Exchange Format");
}

UObject* UNNEModelDataFactory::FactoryCreateBinary(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR * Type, const uint8 *& Buffer, const uint8 * BufferEnd, FFeedbackContext* Warn)
{
	if (!Type || !Buffer || !BufferEnd || BufferEnd - Buffer <= 0)
	{
		return nullptr;
	}

	UNNEModelData* Result = NewObject<UNNEModelData>(InParent, Class, Name, Flags);
	TConstArrayView<uint8> BufferView = MakeArrayView(Buffer, BufferEnd-Buffer);
	Result->Init(Type, BufferView);

	return Result;
}

bool UNNEModelDataFactory::FactoryCanImport(const FString & Filename)
{
	return Filename.EndsWith(FString("onnx"));
}