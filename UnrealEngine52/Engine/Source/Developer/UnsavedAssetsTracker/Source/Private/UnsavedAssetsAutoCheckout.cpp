// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsavedAssetsAutoCheckout.h"

#include "ISourceControlModule.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "Containers/Ticker.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "SourceControlOperations.h"
#include "UnsavedAssetsTrackerModule.h"

FUnsavedAssetsAutoCheckout::FUnsavedAssetsAutoCheckout(FUnsavedAssetsTrackerModule* Module)
	: bProcessCheckoutBatchPending(false)
{
	Module->OnUnsavedAssetAdded.AddRaw(this, &FUnsavedAssetsAutoCheckout::OnAsyncCheckout);
	AsyncCheckoutComplete.BindRaw(this, &FUnsavedAssetsAutoCheckout::OnAsyncCheckoutComplete);
}

FUnsavedAssetsAutoCheckout::~FUnsavedAssetsAutoCheckout()
{
}

void FUnsavedAssetsAutoCheckout::OnAsyncCheckout(const FString& AbsoluteAssetFilepath)
{
	// Check if auto checkout is enabled.
	const UEditorLoadingSavingSettings* Settings = GetDefault<UEditorLoadingSavingSettings>();
	if (!Settings->GetAutomaticallyCheckoutOnAssetModification())
	{
		return;
	}

	// Add to FilesToCheckout batch.
	// Why are we batching? Because moving a large prefab (1000+ source files) would result in
	// an equal amount of FCheckOut operation which didn't scale and locked the editor due to
	// worker thread exhaustion.
	CheckoutBatch.Add(AbsoluteAssetFilepath);

	// If no batch process tick is pending, queue one.
	if (!bProcessCheckoutBatchPending)
	{
		bProcessCheckoutBatchPending = true;

		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateSP(this, &FUnsavedAssetsAutoCheckout::OnProcessCheckoutBatch)
		);
	}
}

void FUnsavedAssetsAutoCheckout::OnAsyncCheckoutComplete(const FSourceControlOperationRef& CheckOutOperation, ECommandResult::Type Result)
{
	const TArray<FString> FilesInBatch = OperationToPaths.FindAndRemoveChecked(&CheckOutOperation.Get());

	if (Result == ECommandResult::Succeeded)
	{
		for (const FString& File : FilesInBatch)
		{
			FUnsavedAssetsTrackerModule::Get().PostUnsavedAssetAutoCheckout.Broadcast(File, CheckOutOperation);
		}
	}

	if (Result == ECommandResult::Failed)
	{
		for (const FString& File : FilesInBatch)
		{
			FUnsavedAssetsTrackerModule::Get().PostUnsavedAssetAutoCheckoutFailure.Broadcast(File, CheckOutOperation);
		}
	}
}

bool FUnsavedAssetsAutoCheckout::OnProcessCheckoutBatch(float)
{
	TArray<FString> FilesToCheckout;
	FilesToCheckout.Reserve(CheckoutBatch.Num());

	for (const FString& File : CheckoutBatch)
	{
		// Why are we checking for file exists?
		// The OnUnsavedAssetAdded delegate triggers when dragging an asset in the viewport as well,
		// before it's even placed in the scene or saved to disk. There's no point in attempting
		// to do an FCheckOut for them.
		if (!FPaths::FileExists(File))
		{
			continue;
		}

		// Why are we checking if the package is still dirty?
		// Some packages become temporarily dirty, for example during world recreation.
		// By the time we try to check them out they may no longer be dirty.
		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(File, PackageName))
		{
			if (UPackage* Package = FindPackage(nullptr, *PackageName))
			{
				if (!Package->IsDirty())
				{
					continue;
				}
			}
		}

		FilesToCheckout.Add(File);
	}
	CheckoutBatch.Empty();

	if (FilesToCheckout.Num() > 0)
	{
		AsyncCheckout(FilesToCheckout);
	}

	bProcessCheckoutBatchPending = false;
	return false; // One shot.
}

void FUnsavedAssetsAutoCheckout::AsyncCheckout(const TArray<FString>& FilesToCheckout)
{
	ensure(FilesToCheckout.Num() > 0);

	FSourceControlOperationRef CheckOutOperation = ISourceControlOperation::Create<FCheckOut>();

	OperationToPaths.Add(&CheckOutOperation.Get(), FilesToCheckout);

	for (const FString& File : FilesToCheckout)
	{
		FUnsavedAssetsTrackerModule::Get().PreUnsavedAssetAutoCheckout.Broadcast(File, CheckOutOperation);
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	SourceControlProvider.Execute(CheckOutOperation, FilesToCheckout, EConcurrency::Asynchronous, AsyncCheckoutComplete);
}