// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterEditorData.h"

#include "EdGraph/EdGraph.h"
#include "NiagaraStackEditorData.h"
#include "ScopedTransaction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraEmitterEditorData)

const FName UNiagaraEmitterEditorData::PrivateMemberNames::SummarySections = GET_MEMBER_NAME_CHECKED(UNiagaraEmitterEditorData, SummarySections);

UNiagaraEmitterEditorData::UNiagaraEmitterEditorData(const FObjectInitializer& ObjectInitializer)
{
	StackEditorData = ObjectInitializer.CreateDefaultSubobject<UNiagaraStackEditorData>(this, TEXT("StackEditorData"));

	if (StackEditorData != nullptr)
	{
		StackEditorData->OnPersistentDataChanged().AddUObject(this, &UNiagaraEmitterEditorData::StackEditorDataChanged);
	}
	
	PlaybackRangeMin = 0;
	PlaybackRangeMax = 10;
}

void UNiagaraEmitterEditorData::PostLoad()
{
	Super::PostLoad();
	if (StackEditorData == nullptr)
	{
		StackEditorData = NewObject<UNiagaraStackEditorData>(this, TEXT("StackEditorData"), RF_Transactional);
		StackEditorData->OnPersistentDataChanged().AddUObject(this, &UNiagaraEmitterEditorData::StackEditorDataChanged);
	}
	StackEditorData->ConditionalPostLoad();
}

#if WITH_EDITORONLY_DATA
void UNiagaraEmitterEditorData::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(UEdGraph::StaticClass()));
}
#endif

UNiagaraStackEditorData& UNiagaraEmitterEditorData::GetStackEditorData() const
{
	return *StackEditorData;
}

TRange<float> UNiagaraEmitterEditorData::GetPlaybackRange() const
{
	return TRange<float>(PlaybackRangeMin, PlaybackRangeMax);
}

void UNiagaraEmitterEditorData::SetPlaybackRange(TRange<float> InPlaybackRange)
{
	PlaybackRangeMin = InPlaybackRange.GetLowerBoundValue();
	PlaybackRangeMax = InPlaybackRange.GetUpperBoundValue();

	OnPersistentDataChanged().Broadcast();
}

void UNiagaraEmitterEditorData::StackEditorDataChanged()
{
	OnPersistentDataChanged().Broadcast();
}



const TMap<FFunctionInputSummaryViewKey, FFunctionInputSummaryViewMetadata>& UNiagaraEmitterEditorData::GetSummaryViewMetaDataMap() const
{
	return SummaryViewFunctionInputMetadata;
}

TOptional<FFunctionInputSummaryViewMetadata> UNiagaraEmitterEditorData::GetSummaryViewMetaData(const FFunctionInputSummaryViewKey& Key) const
{
	return SummaryViewFunctionInputMetadata.Contains(Key)? SummaryViewFunctionInputMetadata[Key] : TOptional<FFunctionInputSummaryViewMetadata>();
}

void UNiagaraEmitterEditorData::SetSummaryViewMetaData(const FFunctionInputSummaryViewKey& Key, TOptional<FFunctionInputSummaryViewMetadata> NewMetadata)
{	
	if (NewMetadata.IsSet() == false)
	{
		SummaryViewFunctionInputMetadata.Remove(Key);
	}
	else
	{
		SummaryViewFunctionInputMetadata.FindOrAdd(Key) = NewMetadata.GetValue();
	}
	
	OnPersistentDataChanged().Broadcast();
	OnSummaryViewStateChangedDelegate.Broadcast();
}

void UNiagaraEmitterEditorData::SetShowSummaryView(bool bInShouldShowSummaryView)
{
	bShowSummaryView = bInShouldShowSummaryView;
	
	OnPersistentDataChanged().Broadcast();
	OnSummaryViewStateChangedDelegate.Broadcast();
}

void UNiagaraEmitterEditorData::ToggleShowSummaryView()
{
	FScopedTransaction ScopedTransaction(NSLOCTEXT("NiagaraEmitter", "EmitterModuleShowSummaryChanged", "Emitter summary view enabled/disabled."));
	Modify();

	SetShowSummaryView(!bShowSummaryView);
}

FSimpleMulticastDelegate& UNiagaraEmitterEditorData::OnSummaryViewStateChanged()
{
	return OnSummaryViewStateChangedDelegate;
}

const TArray<FNiagaraStackSection>& UNiagaraEmitterEditorData::GetSummarySections() const
{
	return SummarySections;
}

void UNiagaraEmitterEditorData::SetSummarySections(const TArray<FNiagaraStackSection>& InSummarySections)
{
	SummarySections = InSummarySections;
}
