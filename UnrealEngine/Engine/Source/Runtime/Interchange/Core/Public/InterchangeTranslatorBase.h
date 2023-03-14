// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "InterchangeResult.h"
#include "InterchangeResultsContainer.h"
#include "InterchangeSourceData.h"
#include "Misc/AssertionMacros.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "InterchangeTranslatorBase.generated.h"

class UInterchangeBaseNodeContainer;

UENUM(BlueprintType)
enum class EInterchangeTranslatorType : uint8
{
	Invalid = 0,
	Assets = 1 << 1,
	Actors = 1 << 2,
	Scenes = Assets | Actors
};

UENUM(BlueprintType)
enum class EInterchangeTranslatorAssetType : uint8
{
	None = 0x0,
	Textures = 0x1 << 0,
	Materials = 0x1 << 1,
	Meshes = 0x1 << 2,
	Animations = 0x1 << 3,
};
ENUM_CLASS_FLAGS(EInterchangeTranslatorAssetType);

UCLASS(BlueprintType, Blueprintable, Abstract)
class INTERCHANGECORE_API UInterchangeTranslatorBase : public UObject
{
	GENERATED_BODY()
public:

	/** return true if the translator can translate the given source data. */
	virtual bool CanImportSourceData(const UInterchangeSourceData* InSourceData) const;

	/** Specifies the capabilities of a translator. */
	virtual EInterchangeTranslatorType GetTranslatorType() const { return EInterchangeTranslatorType::Assets; }

	bool DoesSupportAssetType(EInterchangeTranslatorAssetType AssetType) const { return EnumHasAllFlags(GetSupportedAssetTypes(), AssetType); }
	virtual EInterchangeTranslatorAssetType GetSupportedAssetTypes() const PURE_VIRTUAL(UInterchangeTranslatorBase::GetSupportedAssetTypes, return EInterchangeTranslatorAssetType::None;);

	/** List of formats supported by the translator. Each entry is of the form "ext;Description" where ext is the file extension. */
	virtual TArray<FString> GetSupportedFormats() const PURE_VIRTUAL(UInterchangeTranslatorBase::GetSupportedExtensions, return TArray<FString>(););

	/**
	 * Translate the associated source data into node(s) that are hold in the specified nodes container.
	 *
	 * @param BaseNodeContainer - The nodes container where to put the translated source data result.
	 * @return true if the translator can translate the source data, false otherwise.
	 */
	virtual bool Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
	{
		return false;
	}

	/**
	 * Release source is called when we do not need anymore the translator source and also early in the cancel process.
	 * This is where out of process translator will send the stop command.
	 * A good example of why its useful to release the source is when the dispatcher delete the worker, the join on the
	 * thread will be very short and will not block the main thread. If the worker stop command was send before the completion task.
	 */
	virtual void ReleaseSource()
	{
		return;
	}

	/**
	 * This function is call when the import is done (FImportAsyncHelper::CleanUp) and we are cleaning the data.
	 * Use it to free resource that need to be release before the next garbage collector pass.
	 */
	virtual void ImportFinish()
	{
		return;
	}

	/**
	 * This function is used to add the given message object directly into the results for this operation.
	 */
	template <typename T>
	T* AddMessage() const
	{
		ensure(Results != nullptr);
		ensure(SourceData != nullptr);
		T* Item = Results->Add<T>();
		Item->SourceAssetName = SourceData->GetFilename();
		return Item;
	}


	void AddMessage(UInterchangeResult* Item) const
	{
		ensure(Results != nullptr);
		ensure(SourceData != nullptr);
		Results->Add(Item);
		Item->SourceAssetName = SourceData->GetFilename();
	}
	

	void SetResultsContainer(UInterchangeResultsContainer* InResults)
	{
		Results = InResults;
	}

	/**
	 * Get the associated source data for this translator.
	 */
	const UInterchangeSourceData* GetSourceData() const
	{
		return SourceData;
	}


	UPROPERTY()
	TObjectPtr<UInterchangeResultsContainer> Results;

	UPROPERTY()
	TObjectPtr<const UInterchangeSourceData> SourceData;
};
