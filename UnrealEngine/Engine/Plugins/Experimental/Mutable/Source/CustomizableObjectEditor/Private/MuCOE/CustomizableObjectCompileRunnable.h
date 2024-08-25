// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Runnable.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuR/Ptr.h"
#include "MuT/Node.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"

#include <atomic>

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

	/** */
	struct FReferenceResourceRequest
	{
		int32 ID = -1;
		TSharedPtr<mu::Ptr<mu::Image>> ResolvedImage;
		TSharedPtr<UE::Tasks::FTaskEvent> CompletionEvent;
	};
	TQueue<FReferenceResourceRequest, EQueueMode::Mpsc> PendingResourceReferenceRequests;

	mu::Ptr<mu::Image> LoadResourceReferenced(int32 ID);


public:

	FCustomizableObjectCompileRunnable(mu::Ptr<mu::Node> Root);

	// FRunnable interface
	uint32 Run() override;

	// Own interface

	//
	bool IsCompleted() const;

	//
	const TArray<FError>& GetArrayErrors() const;

	void Tick();

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model;

	FCompilationOptions Options;
	
	TArray<TSoftObjectPtr<UTexture>> ReferencedTextures;

	FString ErrorMsg;

	// Whether the thread has finished running
	std::atomic<bool> bThreadCompleted;
};


class FCustomizableObjectSaveDDRunnable : public FRunnable
{
public:

	FCustomizableObjectSaveDDRunnable(class UCustomizableObject* CustomizableObject, const FCompilationOptions& Options);

	// FRunnable interface
	uint32 Run() override;

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

	bool bIsCooking = false;

	// Whether the thread has finished running
	std::atomic<bool> bThreadCompleted = false;

public:

	// Bytes where the model is stored
	TArray64<uint8> Bytes;

	// Bytes where the streamed data files are stored
	TArray64<uint8> BulkDataBytes;

	// Bytes store streameable files coming form the CO itself.
	TArray64<uint8> MorphDataBytes;

};
