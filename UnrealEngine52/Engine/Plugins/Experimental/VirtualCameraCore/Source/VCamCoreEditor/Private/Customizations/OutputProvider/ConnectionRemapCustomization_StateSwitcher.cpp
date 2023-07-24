// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConnectionRemapCustomization_StateSwitcher.h"

#include "DetailLayoutBuilder.h"
#include "Customization/IConnectionRemapCustomization.h"
#include "UI/VCamWidget.h"
#include "UI/Switcher/VCamStateSwitcherWidget.h"

#include "Algo/AnyOf.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "Customization/IConnectionRemapUtils.h"
#include "Util/SharedPropertyCustomizationUtils.h"

#define LOCTEXT_NAMESPACE "FConnectionRemapCustomization_StateSwitcher"

namespace UE::VCamCoreEditor::Private
{
	TSharedRef<IConnectionRemapCustomization> FConnectionRemapCustomization_StateSwitcher::Make()
	{
		return MakeShared<FConnectionRemapCustomization_StateSwitcher>();
	}

	bool FConnectionRemapCustomization_StateSwitcher::CanGenerateGroup(const FShouldGenerateArgs& Args) const
	{
		UVCamStateSwitcherWidget* StateSwitcherWidget = Cast<UVCamStateSwitcherWidget>(Args.CustomizedWidget);
		const bool bHasStateSwitcherProperties = ensure(StateSwitcherWidget)
			&& Algo::AnyOf(StateSwitcherWidget->GetStates(), [StateSwitcherWidget](FName State)
			{
				FVCamWidgetConnectionState StateInfo;
				return StateSwitcherWidget->GetStateInfo(State, StateInfo)
					&& StateInfo.WidgetConfigs.Num()
					&& Algo::AnyOf(StateInfo.WidgetConfigs, [](const FWidgetConnectionConfig& Config){ return Config.ConnectionTargets.Num() > 0; });
			});

		return bHasStateSwitcherProperties || Super::CanGenerateGroup(Args);
	}
	
	void FConnectionRemapCustomization_StateSwitcher::Customize(const FConnectionRemapCustomizationArgs& Args)
	{
		AddCurrentStateProperty(Args);
		Super::Customize(Args);
	}

	void FConnectionRemapCustomization_StateSwitcher::AddCurrentStateProperty(const FConnectionRemapCustomizationArgs& Args)
	{
		UVCamStateSwitcherWidget* StateSwitcherWidget = Cast<UVCamStateSwitcherWidget>(Args.CustomizedWidget);
		const TSharedPtr<IPropertyHandle> CurrentStatePropertyHandle = Args.Builder.AddObjectPropertyData({ StateSwitcherWidget }, UVCamStateSwitcherWidget::GetCurrentStatePropertyName());
		if (!ensure(CurrentStatePropertyHandle))
		{
			return;
		}

		IDetailPropertyRow& PropertyRow = Args.WidgetGroup.AddPropertyRow(CurrentStatePropertyHandle.ToSharedRef());
		StateSwitcher::CustomizeCurrentState(*StateSwitcherWidget, PropertyRow, CurrentStatePropertyHandle.ToSharedRef(), Args.Utils->GetRegularFont());
	}

}

#undef LOCTEXT_NAMESPACE