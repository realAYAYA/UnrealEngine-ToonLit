// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Runnable.h"
#include "MuCO/CustomizableObject.h"
#include "MuR/Ptr.h"
#include "MuT/Node.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"

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
		EMessageSeverity::Type Severity;
		ELoggerSpamBin SpamBin = ELoggerSpamBin::ShowAll;
		FText Message;
		TSharedPtr<FErrorAttachedData> AttachedData;
		const void* Context;

		FError(const EMessageSeverity::Type InSeverity, const FText& InMessage, const void* InContext, const ELoggerSpamBin InSpamBin = ELoggerSpamBin::ShowAll) : Severity(InSeverity), SpamBin(InSpamBin), Message(InMessage), Context(InContext) {}
		FError(const EMessageSeverity::Type InSeverity, const FText& InMessage, const TSharedPtr<FErrorAttachedData>& InAttachedData, const void* InContext, const ELoggerSpamBin InSpamBin = ELoggerSpamBin::ShowAll)
			: Severity(InSeverity), SpamBin(InSpamBin), Message(InMessage), AttachedData(InAttachedData), Context(InContext) {}
	};

private:

	mu::Ptr<mu::Node> MutableRoot;
	TArray<FError> ArrayErrors;

public:

	FCustomizableObjectCompileRunnable(mu::Ptr<mu::Node> Root);

	// FRunnable interface
	uint32 Run() override;

	// Own interface

	//
	bool IsCompleted() const;

	//
	const TArray<FError>& GetArrayErrors() const;

public:

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model;

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
	FString CompileDataFullFileName;
	FString StreamableDataFullFileName;

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model;

	// Bytes where the model is stored
	TArray64<uint8> Bytes;

	// Bytes where the streamed data files are stored
	TArray64<uint8> BulkDataBytes;

	bool bIsCooking = false;

	// Whether the thread has finished running
	bool bThreadCompleted = false;
};
