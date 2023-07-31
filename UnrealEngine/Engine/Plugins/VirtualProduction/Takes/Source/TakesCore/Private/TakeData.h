// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakesCoreLog.h"
#include "TakeMetaData.h"
#include "TakesCoreBlueprintLibrary.h"

#include "LevelSequence.h"
#include "MovieSceneToolsModule.h"
#include "MovieSceneSequence.h"
#include "Sections/MovieSceneSubSection.h"

#include "AssetRegistry/AssetRegistryModule.h"

class FTakesCoreTakeData : public IMovieSceneToolsTakeData
{
public:
	virtual ~FTakesCoreTakeData() { }

	virtual bool GatherTakes(const UMovieSceneSection* Section, TArray<FAssetData>& AssetData, uint32& OutCurrentTakeNumber)
	{
		const UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
		if (!SubSection)
		{
			return false;
		}

		ULevelSequence* Sequence = Cast<ULevelSequence>(SubSection->GetSequence());
		if (!Sequence)
		{
			return false;
		}

		UTakeMetaData* TakeMetaData = Sequence->FindMetaData<UTakeMetaData>();
		if (!TakeMetaData)
		{
			return false;
		}

		TArray<FAssetData> TakeDatas = UTakesCoreBlueprintLibrary::FindTakes(TakeMetaData->GetSlate());
		for (FAssetData TakeData : TakeDatas)
		{
			FAssetDataTagMapSharedView::FFindTagResult TakeNumberTag = TakeData.TagsAndValues.FindTag(UTakeMetaData::AssetRegistryTag_TakeNumber);

			int32 ThisTakeNumber = 0;
			if (TakeNumberTag.IsSet() && LexTryParseString(ThisTakeNumber, *TakeNumberTag.GetValue()))
			{
				AssetData.Add(TakeData);
			}
		}

		OutCurrentTakeNumber = TakeMetaData->GetTakeNumber();

		return true;
	}

	virtual bool GetTakeNumber(const UMovieSceneSection* Section, FAssetData AssetData, uint32& OutTakeNumber)
	{
		const UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
		if (!SubSection)
		{
			return false;
		}

		ULevelSequence* Sequence = Cast<ULevelSequence>(SubSection->GetSequence());
		if (!Sequence)
		{
			return false;
		}

		UTakeMetaData* TakeMetaData = Sequence->FindMetaData<UTakeMetaData>();
		if (!TakeMetaData)
		{
			return false;
		}

		TArray<FAssetData> TakeDatas = UTakesCoreBlueprintLibrary::FindTakes(TakeMetaData->GetSlate());
		for (FAssetData TakeData : TakeDatas)
		{
			if (TakeData == AssetData)
			{
				FAssetDataTagMapSharedView::FFindTagResult TakeNumberTag = TakeData.TagsAndValues.FindTag(UTakeMetaData::AssetRegistryTag_TakeNumber);

				int32 ThisTakeNumber = 0;
				if (TakeNumberTag.IsSet() && LexTryParseString(ThisTakeNumber, *TakeNumberTag.GetValue()))
				{
					OutTakeNumber = ThisTakeNumber;
					return true;
				}
			}
		}

		return false;
	}

	virtual bool SetTakeNumber(const UMovieSceneSection* Section, uint32 InTakeNumber)
	{
		const UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
		if (!SubSection)
		{
			return false;
		}

		ULevelSequence* Sequence = Cast<ULevelSequence>(SubSection->GetSequence());
		if (!Sequence)
		{
			return false;
		}

		UTakeMetaData* TakeMetaData = Sequence->FindMetaData<UTakeMetaData>();
		if (!TakeMetaData)
		{
			return false;
		}

		bool bWasLocked = TakeMetaData->IsLocked();
		TakeMetaData->Unlock();
		TakeMetaData->SetTakeNumber(InTakeNumber);
		if (bWasLocked)
		{
			TakeMetaData->Lock();
		}

		return true;
	}
};