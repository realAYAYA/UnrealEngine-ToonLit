// Copyright Epic Games, Inc. All Rights Reserved.
#include "Animation/AnimCompressionDerivedData.h"
#include "Animation/AnimCompressionDerivedDataPublic.h"
#include "DerivedDataCacheInterface.h"
#include "Misc/ScopeExit.h"
#include "Stats/Stats.h"
#include "Animation/AnimSequence.h"
#include "AnimationUtils.h"
#include "Animation/AnimCompress.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Misc/CoreDelegates.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Animation/AnimationCompressionDerivedData.h"

#if WITH_EDITOR

DECLARE_CYCLE_STAT(TEXT("Anim Compression (Derived Data)"), STAT_AnimCompressionDerivedData, STATGROUP_Anim);

FDerivedDataAnimationCompression::FDerivedDataAnimationCompression(const TCHAR* InTypeName, const FString& InAssetDDCKey,
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TSharedPtr<FAnimCompressContext> InCompressContext)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	: TypeName(InTypeName)
	, AssetDDCKey(InAssetDDCKey)
	, CompressContext(InCompressContext)
{

}

FDerivedDataAnimationCompression::~FDerivedDataAnimationCompression()
{
}

const TCHAR* FDerivedDataAnimationCompression::GetVersionString() const
{
	return *UE::Anim::Compression::AnimationCompressionVersionString;
}

FString FDerivedDataAnimationCompression::GetDebugContextString() const
{
	check(DataToCompressPtr.IsValid());
	return DataToCompressPtr->FullName;
}

bool FDerivedDataAnimationCompression::Build( TArray<uint8>& OutDataArray )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDerivedDataAnimationCompression::Build);

	const double CompressionStartTime = FPlatformTime::Seconds();

	check(DataToCompressPtr.IsValid());
	FCompressibleAnimData& DataToCompress = *DataToCompressPtr.Get();
	FCompressedAnimSequence OutData;

	if (DataToCompress.IsCancelled())
	{
		return false;
	}

	// Update UsageStats only when not running on the game thread since that thread times this at a higher level.
	COOK_STAT(auto Timer = UE::Anim::AnimSequenceCookStats::UsageStats.TimeSyncWork());
	COOK_STAT(IsInGameThread() ? Timer.Cancel() : Timer.TrackCyclesOnly());

	SCOPE_CYCLE_COUNTER(STAT_AnimCompressionDerivedData);
	UE_LOG(LogAnimationCompression, Log, TEXT("Building Anim DDC data for %s"), *DataToCompress.FullName);

	FCompressibleAnimDataResult CompressionResult;

	bool bCompressionSuccessful = false;
	{
		DataToCompress.FetchData(DataToCompress.TargetPlatform);
		DataToCompress.Update(OutData);

		const bool bBoneCompressionOk = FAnimationUtils::CompressAnimBones(DataToCompress, CompressionResult);
		if (DataToCompress.IsCancelled())
		{
			return false;
		}
		const bool bCurveCompressionOk = FAnimationUtils::CompressAnimCurves(DataToCompress, OutData);
		
		bCompressionSuccessful = bBoneCompressionOk && bCurveCompressionOk;

#if DO_CHECK
		FString CompressionName = DataToCompress.BoneCompressionSettings->GetFullName();
		
		ensureMsgf(bCompressionSuccessful, TEXT("Anim Compression failed for Sequence '%s' with compression scheme '%s': compressed data empty\n\tAnimIndex: %i\n\tMaxAnim:%i"), 
											*DataToCompress.FullName,
											*CompressionName,
											PRAGMA_DISABLE_DEPRECATION_WARNINGS
											CompressContext.Get()->AnimIndex,
											CompressContext.Get()->MaxAnimations
											PRAGMA_ENABLE_DEPRECATION_WARNINGS
											);
#endif
	}

	bCompressionSuccessful = bCompressionSuccessful && !DataToCompress.IsCancelled();

	if (bCompressionSuccessful)
	{
		const double CompressionEndTime = FPlatformTime::Seconds();
		const double CompressionTime = CompressionEndTime - CompressionStartTime;

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		CompressContext->GatherPostCompressionStats(OutData, DataToCompress.BoneData, DataToCompress.AnimFName, CompressionTime, true);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		OutData.CompressedByteStream = MoveTemp(CompressionResult.CompressedByteStream);
		OutData.CompressedDataStructure = MoveTemp(CompressionResult.AnimData);
		OutData.BoneCompressionCodec = CompressionResult.Codec;

		FMemoryWriter Ar(OutDataArray, true);
		OutData.CompressedRawData = DataToCompress.RawAnimationData;
		OutData.OwnerName = DataToCompress.AnimFName;
		OutData.SerializeCompressedData(Ar, true, nullptr, nullptr, DataToCompress.BoneCompressionSettings, DataToCompress.CurveCompressionSettings); //Save out compressed
	}

	return bCompressionSuccessful;
}

#endif	//WITH_EDITOR
