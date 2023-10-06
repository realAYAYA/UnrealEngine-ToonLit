// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSynchronizationSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TimeSynchronizationSource)

#if WITH_EDITOR
#include "Widgets/SNullWidget.h"
#include "Widgets/SWidget.h"
#endif

UTimeSynchronizationSource::UTimeSynchronizationSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUseForSynchronization(true)
{

}

#if WITH_EDITOR
TSharedRef<SWidget> UTimeSynchronizationSource::GetVisualWidget() const 
{ 
	return SNullWidget::NullWidget; 
};
#endif
