// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEditorDataBase.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraStackSection.h"
#include "NiagaraEmitterEditorData.generated.h"

class UNiagaraStackEditorData;


/** Editor only UI data for emitters. */
UCLASS(MinimalAPI)
class UNiagaraEmitterEditorData : public UNiagaraEditorDataBase
{
	GENERATED_BODY()

public: 
	struct PrivateMemberNames
	{
		static const FName SummarySections;
	};

public:
	UNiagaraEmitterEditorData(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	NIAGARAEDITOR_API static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

	NIAGARAEDITOR_API UNiagaraStackEditorData& GetStackEditorData() const;

	TRange<float> GetPlaybackRange() const;

	void SetPlaybackRange(TRange<float> InPlaybackRange);

	NIAGARAEDITOR_API FSimpleMulticastDelegate& OnSummaryViewStateChanged();
	
	NIAGARAEDITOR_API bool ShouldShowSummaryView() const { return bShowSummaryView; }
	NIAGARAEDITOR_API void SetShowSummaryView(bool bInShouldShowSummaryView);
	NIAGARAEDITOR_API void ToggleShowSummaryView();
	
	NIAGARAEDITOR_API const TMap<FFunctionInputSummaryViewKey, FFunctionInputSummaryViewMetadata>& GetSummaryViewMetaDataMap() const;
	NIAGARAEDITOR_API TOptional<FFunctionInputSummaryViewMetadata> GetSummaryViewMetaData(const FFunctionInputSummaryViewKey& Key) const;
	NIAGARAEDITOR_API void SetSummaryViewMetaData(const FFunctionInputSummaryViewKey& Key, TOptional<FFunctionInputSummaryViewMetadata> NewMetadata);

	const TArray<FNiagaraStackSection>& GetSummarySections() const;
	void SetSummarySections(const TArray<FNiagaraStackSection>& InSummarySections);

private:
	UPROPERTY(Instanced)
	TObjectPtr<UNiagaraStackEditorData> StackEditorData;

	UPROPERTY()
	float PlaybackRangeMin;

	UPROPERTY()
	float PlaybackRangeMax;

	UPROPERTY()
	uint32 bShowSummaryView : 1;

	/** Stores metadata for filtering function inputs when in Filtered/Simple view. */
	UPROPERTY()
	TMap<FFunctionInputSummaryViewKey, FFunctionInputSummaryViewMetadata> SummaryViewFunctionInputMetadata;

	UPROPERTY(EditAnywhere, Category=Summary)
	TArray<FNiagaraStackSection> SummarySections;

	FSimpleMulticastDelegate OnSummaryViewStateChangedDelegate;
	
	void StackEditorDataChanged();	
};


