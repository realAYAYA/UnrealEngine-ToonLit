// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_ChooserPlayer.h"

#include "EditorCategoryUtils.h"
#include "DetailLayoutBuilder.h"
#include "Animation/AnimAttributes.h"
#include "Animation/AnimPoseSearchProvider.h"
#include "Animation/AnimRootMotionProvider.h"
#include "ChooserPropertyAccess.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_ChooserPlayer"

void UAnimGraphNode_ChooserPlayer::UpdateContextData()
{
	if (Node.ChooserContextDefinition.IsEmpty())
	{
		Node.ChooserContextDefinition.SetNum(2);
		Node.ChooserContextDefinition[0].InitializeAs(FContextObjectTypeClass::StaticStruct());
		FContextObjectTypeClass& ClassEntry = Node.ChooserContextDefinition[0].GetMutable<FContextObjectTypeClass>();
		ClassEntry.Class = GetBlueprint()->GeneratedClass;
		ClassEntry.Direction = EContextObjectDirection::ReadWrite;
		Node.ChooserContextDefinition[1].InitializeAs(FContextObjectTypeStruct::StaticStruct());
		FContextObjectTypeStruct& StructEntry = Node.ChooserContextDefinition[1].GetMutable<FContextObjectTypeStruct>();
		StructEntry.Struct = FChooserPlayerSettings::StaticStruct();
		ClassEntry.Direction = EContextObjectDirection::ReadWrite;
	}
}

void UAnimGraphNode_ChooserPlayer::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();
	UpdateContextData();
}

FLinearColor UAnimGraphNode_ChooserPlayer::GetNodeTitleColor() const
{
	return FLinearColor(0.10f, 0.60f, 0.12f);
}

FText UAnimGraphNode_ChooserPlayer::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Selects Animation Assets using a Chooser, and plays them with an underlying BlendStack.");
}

FText UAnimGraphNode_ChooserPlayer::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Chooser Player");
}

FText UAnimGraphNode_ChooserPlayer::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Animation|Sequences");
}

void UAnimGraphNode_ChooserPlayer::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Super::CustomizeDetails(DetailBuilder);

	if (!UE::Anim::IPoseSearchProvider::IsAvailable())
	{
		DetailBuilder.HideCategory(TEXT("PoseMatching"));
	}
}

void UAnimGraphNode_ChooserPlayer::GetOutputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	OutAttributes.Add(UE::Anim::FAttributes::Curves);
	OutAttributes.Add(UE::Anim::FAttributes::Attributes);

	if (UE::Anim::IAnimRootMotionProvider::Get())
	{
		OutAttributes.Add(UE::Anim::IAnimRootMotionProvider::AttributeName);
	}
}

#undef LOCTEXT_NAMESPACE
