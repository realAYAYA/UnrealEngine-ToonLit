// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_AnimationAsset.h"
#include "Animation/AnimStreamable.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

class FAssetTypeActions_AnimStreamable : public FAssetTypeActions_AnimationAsset
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimStreamable", "Streamable Animation"); }
	virtual FColor GetTypeColor() const override { return FColor(181,230,29); }
	virtual UClass* GetSupportedClass() const override { return UAnimStreamable::StaticClass(); }
	virtual bool CanFilter() override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override {}
};

/*void FAssetTypeActions_AnimStreamable::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto Sequences = GetTypedWeakObjectPtrs<UAnimStreamable>(InObjects);

	auto Logger = [Sequences]()
	{
		uint32 TotalCompressedData = 0;
		uint32 TotalZeroChunks = 0;
		uint32 TotalMaxStreaming = 0;

		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		check(TPM)
		ITargetPlatform* Platform = TPM->GetRunningTargetPlatform();
		check(Platform);

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("EVENT Streaming Memory Breakdown:"));
		for (TWeakObjectPtr<UAnimStreamable> Streamable : Sequences)
		{
			const FStreamableAnimPlatformData& StreamingData = Streamable->GetStreamingAnimPlatformData(Platform);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\tAnim : %s Chunks:%i ChunkTime:%.3f\n"), *Streamable->GetName(), StreamingData.Chunks.Num(), Streamable->GetChunkSizeSeconds(Platform));

			int32 MaxConsecutiveChunks = 0;
			for (int32 ChunkIndex = 0; ChunkIndex < StreamingData.Chunks.Num(); ++ChunkIndex)
			{
				const int32 NextChunkIndex = (++MaxConsecutiveChunks) % StreamingData.Chunks.Num();
				const int32 ChunkSize = StreamingData.Chunks[ChunkIndex].GetMemorySize();
				
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\t\tChunk %i ) %i Bytes\n"), ChunkIndex, ChunkSize);

				if (ChunkIndex > 0)
				{
					const int32 NextChunkSize = StreamingData.Chunks[NextChunkIndex].GetMemorySize();
					const int32 ConsecutiveSize = ChunkSize + NextChunkSize;
					MaxConsecutiveChunks = FMath::Max(MaxConsecutiveChunks, ConsecutiveSize);
				}
				else
				{
					TotalZeroChunks += ChunkSize;
				}
				TotalCompressedData += ChunkSize;
			}

			TotalMaxStreaming += MaxConsecutiveChunks;
		}
		TotalMaxStreaming += TotalZeroChunks;

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Total Compressed Data: %.2f Kb\n"), (double)TotalCompressedData / 1000.0);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Total Zero Chunks Size: %.2f Kb\n"), (double)TotalZeroChunks / 1000.0);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Max Streaming Size: %.2f Kb\n"), (double)TotalMaxStreaming / 1000.0);
	};

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AnimStreamable", "AnimSequence_ReimportWithNewSource", "LOGGER"),
		NSLOCTEXT("AnimStreamable", "AnimSequence_ReimportWithNewSourceTooltip", "Reimport the selected sequence(s) from a new source file."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.AssetActions.ReimportAnim"),
		FUIAction(FExecuteAction::CreateLambda(Logger))
	);
}*/