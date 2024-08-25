// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavAgentSelectorCustomization.h"

#include "AI/Navigation/NavigationTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailWidgetRow.h"
#include "Engine/Engine.h"
#include "Fonts/SlateFontInfo.h"
#include "HAL/PlatformCrt.h"
#include "IDetailChildrenBuilder.h"
#include "Internationalization/Internationalization.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/Attribute.h"
#include "NavigationSystem.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Templates/SubclassOf.h"
#include "Types/SlateEnums.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FNavAgentSelectorCustomization"

TSharedRef<IPropertyTypeCustomization> FNavAgentSelectorCustomization::MakeInstance()
{
	return MakeShareable(new FNavAgentSelectorCustomization);
}

void FNavAgentSelectorCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructHandle = StructPropertyHandle;
	OnAgentStateChanged();

	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(400.0f)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(this, &FNavAgentSelectorCustomization::GetSupportedDesc)
		.Font(StructCustomizationUtils.GetRegularFont())
	];
}

void FNavAgentSelectorCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	FString AgentPrefix("bSupportsAgent");
	const UNavigationSystemV1* NavSysCDO = (*GEngine->NavigationSystemClass != nullptr && GEngine->NavigationSystemClass->IsChildOf(UNavigationSystemV1::StaticClass()))
		? GetDefault<UNavigationSystemV1>(GEngine->NavigationSystemClass)
		: GetDefault<UNavigationSystemV1>();

	if (NavSysCDO == nullptr)
	{
		return;
	}

	const int32 NumAgents = FMath::Min(NavSysCDO->GetSupportedAgents().Num(), 16);

	for (uint32 Idx = 0; Idx < NumChildren; Idx++)
	{
		TSharedPtr<IPropertyHandle> PropHandle = StructPropertyHandle->GetChildHandle(Idx);
		if (PropHandle->GetProperty() && PropHandle->GetProperty()->GetName().StartsWith(AgentPrefix))
		{
			PropHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FNavAgentSelectorCustomization::OnAgentStateChanged));

			int32 AgentIdx = -1;
			TTypeFromString<int32>::FromString(AgentIdx, *(PropHandle->GetProperty()->GetName().Mid(AgentPrefix.Len()) ));

			if (AgentIdx >= 0 && AgentIdx < NumAgents)
			{
				FText PropName = FText::FromName(NavSysCDO->GetSupportedAgents()[AgentIdx].Name);
				StructBuilder.AddCustomRow(PropName)
					.NameContent()
					[
						SNew(STextBlock)
						.Text(PropName)
						.Font(StructCustomizationUtils.GetRegularFont())
					]
					.ValueContent()
					[
						PropHandle->CreatePropertyValueWidget()
					];
			}

			continue;
		}

		StructBuilder.AddProperty(PropHandle.ToSharedRef());
	}
}

void FNavAgentSelectorCustomization::OnAgentStateChanged()
{
	const UNavigationSystemV1* NavSysCDO = (*GEngine->NavigationSystemClass != nullptr && GEngine->NavigationSystemClass->IsChildOf(UNavigationSystemV1::StaticClass()))
		? GetDefault<UNavigationSystemV1>(GEngine->NavigationSystemClass)
		: GetDefault<UNavigationSystemV1>();

	if (NavSysCDO == nullptr)
	{
		return;
	}

	const int32 NumAgents = FMath::Min(NavSysCDO->GetSupportedAgents().Num(), 16);

	uint32 NumChildren = 0;
	StructHandle->GetNumChildren(NumChildren);

	int32 NumSupported = 0;
	int32 FirstSupportedIdx = -1;

	FString AgentPrefix("bSupportsAgent");
	for (uint32 Idx = 0; Idx < NumChildren; Idx++)
	{
		TSharedPtr<IPropertyHandle> PropHandle = StructHandle->GetChildHandle(Idx);
		if (PropHandle->GetProperty() && PropHandle->GetProperty()->GetName().StartsWith(AgentPrefix))
		{
			bool bSupportsAgent = false;
			FPropertyAccess::Result Result = PropHandle->GetValue(bSupportsAgent);
			if (Result == FPropertyAccess::Success && bSupportsAgent)
			{
				int32 AgentIdx = -1;
				TTypeFromString<int32>::FromString(AgentIdx, *(PropHandle->GetProperty()->GetName().Mid(AgentPrefix.Len())));

				if (AgentIdx >= 0 && AgentIdx < NumAgents)
				{
					NumSupported++;
					if (FirstSupportedIdx < 0)
					{
						FirstSupportedIdx = AgentIdx;
					}
				}
			}
		}
	}

	if (NumSupported == NumAgents)
	{
		SupportedDesc = LOCTEXT("AllAgents", "all");
	}
	else if (NumSupported == 0)
	{
		SupportedDesc = LOCTEXT("NoAgents", "none");
	}
	else if (NumSupported == 1)
	{
		SupportedDesc = FText::FromName(NavSysCDO->GetSupportedAgents()[FirstSupportedIdx].Name);
	}
	else
	{
		SupportedDesc = FText::Format(FText::FromString("{0}, ..."), FText::FromName(NavSysCDO->GetSupportedAgents()[FirstSupportedIdx].Name));
	}
}

FText FNavAgentSelectorCustomization::GetSupportedDesc() const
{
	return SupportedDesc;
}

#undef LOCTEXT_NAMESPACE
