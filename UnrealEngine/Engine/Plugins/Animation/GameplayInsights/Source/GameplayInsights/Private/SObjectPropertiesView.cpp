// Copyright Epic Games, Inc. All Rights Reserved.

#include "SObjectPropertiesView.h"
#include "AnimationProvider.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "GameplayProvider.h"
#include "IRewindDebugger.h"
#include "Styling/SlateIconFinder.h"
#include "TraceServices/Model/Frames.h"
#include "VariantTreeNode.h"

#define LOCTEXT_NAMESPACE "SObjectPropertiesView"

void SObjectPropertiesView::GetVariantsAtFrame(const TraceServices::FFrame& InFrame, TArray<TSharedRef<FVariantTreeNode>>& OutVariants) const
{
	if (const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

		TSharedRef<FVariantTreeNode> Header = OutVariants.Add_GetRef(FVariantTreeNode::MakeHeader(LOCTEXT("Properties", "Properties"), INDEX_NONE));

		TArray<TSharedPtr<FVariantTreeNode>> PropertyVariants;

		GameplayProvider->ReadObjectPropertiesTimeline(ObjectId, [this, &InFrame, GameplayProvider, &Header, &PropertyVariants](const FGameplayProvider::ObjectPropertiesTimeline& InTimeline)
		{
			InTimeline.EnumerateEvents(InFrame.StartTime, InFrame.EndTime, [this, GameplayProvider, &Header, &PropertyVariants](double InStartTime, double InEndTime, uint32 InDepth, const FObjectPropertiesMessage& InMessage)
			{
				GameplayProvider->EnumerateObjectPropertyValues(ObjectId, InMessage, [GameplayProvider, &Header, &PropertyVariants](const FObjectPropertyValue& InValue)
				{
					const TCHAR* Key = GameplayProvider->GetPropertyName(InValue.KeyStringId);
					PropertyVariants.Add(FVariantTreeNode::MakeString(FText::FromString(Key), InValue.Value));

					// note assumes that order is parent->child in the properties array
					if(InValue.ParentId != INDEX_NONE)
					{
						PropertyVariants[InValue.ParentId]->AddChild(PropertyVariants.Last().ToSharedRef());
					}
					else
					{
						Header->AddChild(PropertyVariants.Last().ToSharedRef());
					}
				});
				return TraceServices::EEventEnumerate::Stop;
			});
		});
	}
}

static const FName ObjectPropertiesName("ObjectProperties");

FName SObjectPropertiesView::GetName() const
{
	return ObjectPropertiesName;
}

FName FObjectPropertiesViewCreator::GetTargetTypeName() const
{
	static FName TargetTypeName = "Object";
	return TargetTypeName;
}

FName FObjectPropertiesViewCreator::GetName() const
{
	return ObjectPropertiesName;
}

FText FObjectPropertiesViewCreator::GetTitle() const
{
	return LOCTEXT("Object Properties", "Properties");
}

FSlateIcon FObjectPropertiesViewCreator::GetIcon() const
{
	return FSlateIconFinder::FindIconForClass(UObject::StaticClass());
}

TSharedPtr<IRewindDebuggerView> FObjectPropertiesViewCreator::CreateDebugView(uint64 ObjectId, double CurrentTime, const TraceServices::IAnalysisSession& AnalysisSession) const
{
	return SNew(SObjectPropertiesView, ObjectId, CurrentTime, AnalysisSession);
}

bool FObjectPropertiesViewCreator::HasDebugInfo(uint64 ObjectId) const
{
	const TraceServices::IAnalysisSession* AnalysisSession = IRewindDebugger::Instance()->GetAnalysisSession();
	
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	bool bHasData = false;
	if (const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName))
	{
		GameplayProvider->ReadObjectPropertiesTimeline(ObjectId, [this, &bHasData, GameplayProvider](const FGameplayProvider::ObjectPropertiesTimeline& InTimeline)
		{
			bHasData = true;
		});
	}
	
	return bHasData;
}


#undef LOCTEXT_NAMESPACE
