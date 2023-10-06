// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertyTrackView.h"

#include "GameplayProvider.h"
#include "PropertyHelpers.h"
#include "PropertyTrack.h"
#include "VariantTreeNode.h"

void SPropertyTrackView::GetVariantsAtFrame(const TraceServices::FFrame& InFrame,TArray<TSharedRef<FVariantTreeNode>>& OutVariants) const
{
	if (TSharedPtr<RewindDebugger::FPropertyTrack> PropertyTrack = Track.Pin())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		const TSharedRef<FVariantTreeNode> Header = OutVariants.Add_GetRef(FVariantTreeNode::MakeHeader(NSLOCTEXT("SPropertyTrackView", "Properties", "Properties"), INDEX_NONE));
	
		if (const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName))
		{
			PropertyTrack->ReadObjectPropertyValueAtFrame(InFrame, *GameplayProvider, [GameplayProvider, &PropertyTrack, &Header](const FObjectPropertyValue& InValue, uint32 InValueIndex, const FObjectPropertiesMessage& InMessage)
			{
				GameplayProvider->ReadObjectPropertiesStorage(PropertyTrack->GetObjectId(), InMessage, [GameplayProvider, InValueIndex, &PropertyTrack, &Header](const TConstArrayView<FObjectPropertyValue> & InStorage)
				{
					Header->AddChild(FObjectPropertyHelpers::GetVariantNodeFromProperty(InValueIndex, *GameplayProvider, InStorage));
				});
			});
		}
		else
		{
			Header->AddChild(FVariantTreeNode::MakeString(FText::FromName(PropertyTrack->GetPropertyName()), PropertyTrack->GetObjectProperty()->Property.Value));
		}
	}
}

FName SPropertyTrackView::GetName() const
{
	return Track.IsValid() ? Track.Pin()->GetName() : FName(TEXT("SPropertyTrackView"));
}

void SPropertyTrackView::SetTrack(const TWeakPtr<RewindDebugger::FPropertyTrack>& InTrack)
{
	Track = InTrack;
}
