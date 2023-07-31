// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "HAL/Runnable.h"
#include "Internationalization/Text.h"
#include "MuCO/CustomizableObject.h"
#include "MuR/Model.h"
#include "MuT/Node.h"
#include "Templates/SharedPointer.h"

class ITargetPlatform;


class FCustomizableObjectCompileRunnable : public FRunnable
{
public:

	struct FErrorAttachedData
	{
		TArray<float> UnassignedUVs;
	};

	struct FError
	{
		FText Message;
		TSharedPtr<FErrorAttachedData> AttachedData;
		const void* Context;

		FError(const FText& InMessage, const void* InContext) : Message(InMessage), Context(InContext) {}
		FError(const FText& InMessage, const TSharedPtr<FErrorAttachedData>& InAttachedData, const void* InContext) 
			: Message(InMessage), AttachedData(InAttachedData), Context(InContext) {}
	};

private:

	mu::NodePtr MutableRoot;
	TArray<FError> ArrayWarning;
	TArray<FError> ArrayError;

public:

	FCustomizableObjectCompileRunnable(mu::NodePtr Root, bool bInDisableTextureLayout);

	// FRunnable interface
	uint32 Run() override;

	// Own interface

	//
	bool IsCompleted() const;

	//
	const TArray<FError>& GetArrayError() const;
	
	//
	const TArray<FError>& GetArrayWarning() const;

public:

	mu::ModelPtr Model;

	// Texture packing strategy
	bool bDisableTextureLayout;

	FCompilationOptions Options;

	FString ErrorMsg;

	// Whether the thread has finished running
	bool bThreadCompleted;

	// If Mutable compile is disabled, to immediately finish the Run() method
	bool MutableIsDisabled;
};


class FCustomizableObjectSaveDDRunnable : public FRunnable
{
public:

	FCustomizableObjectSaveDDRunnable(class UCustomizableObject* CustomizableObject, const FCompilationOptions& Options);

	// FRunnable interface
	uint32 Run() override;

	// Bytes where the model is stored
	TArray64<uint8>& GetModelBytes();

	// Bytes where the streamed data files are stored
	TArray64<uint8>& GetBulkBytes();

	//
	bool IsCompleted() const;


	const ITargetPlatform* GetTargetPlatform() const;

	//
	FCompilationOptions Options;

private:

	MutableCompiledDataStreamHeader CustomizableObjectHeader;

	// Paths used to save files to disk
	FString FolderPath;
	FString CompildeDataFullFileName;
	FString StreamableDataFullFileName;

	mu::ModelPtr Model;

	// Bytes where the model is stored
	TArray64<uint8> Bytes;

	// Bytes where the streamed data files are stored
	TArray64<uint8> BulkDataBytes;

	bool bIsCooking = false;

	// Whether the thread has finished running
	bool bThreadCompleted = false;
};