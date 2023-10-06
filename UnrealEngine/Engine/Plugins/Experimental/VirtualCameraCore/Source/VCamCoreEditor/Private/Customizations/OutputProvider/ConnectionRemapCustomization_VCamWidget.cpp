// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConnectionRemapCustomization_VCamWidget.h"

#include "UI/VCamWidget.h"

#include "Algo/AnyOf.h"
#include "Customization/IConnectionRemapUtils.h"
#include "DetailWidgetRow.h"
#include "LogVCamEditor.h"

#define LOCTEXT_NAMESPACE "FConnectionRemapCustomization_VCamWidget"

namespace UE::VCamCoreEditor::Private
{
	TSharedRef<IConnectionRemapCustomization> FConnectionRemapCustomization_VCamWidget::Make()
	{
		return MakeShared<FConnectionRemapCustomization_VCamWidget>();
	}

	bool FConnectionRemapCustomization_VCamWidget::CanGenerateGroup(const FShouldGenerateArgs& Args) const
	{
		return Args.CustomizedWidget->Connections.Num() > 0
			&& (!Args.DisplaySettings.bOnlyShowManuallyConfiguredConnections
				|| Algo::AnyOf(Args.CustomizedWidget->Connections, [](const TPair<FName, FVCamConnection>& Pair)
				{
					return Pair.Value.bManuallyConfigureConnection;
				}));
	}

	void FConnectionRemapCustomization_VCamWidget::Customize(const FConnectionRemapCustomizationArgs& Args)
	{
		Widget = Args.CustomizedWidget;
		
		for (const TPair<FName, FVCamConnection>& Connection : Args.CustomizedWidget->Connections)
		{
			if (Args.DisplaySettings.bOnlyShowManuallyConfiguredConnections && !Connection.Value.bManuallyConfigureConnection)
			{
				continue;
			}
			
			const FName ConnectionName = Connection.Key;
			Args.Utils->AddConnection(FAddConnectionArgs{
				Args.WidgetGroup,
				ConnectionName,
				Connection.Value,
				FOnTargetSettingsChanged::CreateSP(this, &FConnectionRemapCustomization_VCamWidget::OnTargetSettingsChanged, ConnectionName),
				Args.Utils->GetRegularFont()
			});
		}
	}

	void FConnectionRemapCustomization_VCamWidget::OnTargetSettingsChanged(const FVCamConnectionTargetSettings& NewConnectionTargetSettings, FName ConnectionName) const
	{
		if (!NewConnectionTargetSettings.HasValidSettings())
		{
			return;
		}
		
		EConnectionUpdateResult Result = EConnectionUpdateResult::NoConnectionsUpdated;
		
		if (Widget.IsValid())
		{
			const TMap<FName, FVCamConnectionTargetSettings> NewConnectionTargets {{ ConnectionName, NewConnectionTargetSettings }};
			constexpr bool bReinitializeOnSuccess = true;
			Widget->UpdateConnectionTargets(NewConnectionTargets, bReinitializeOnSuccess, Result);
		}
		
		UE_CLOG(Result == EConnectionUpdateResult::NoConnectionsUpdated, LogVCamEditor, Warning, TEXT("Failed to update connection %s"), *ConnectionName.ToString());
	}
}

#undef LOCTEXT_NAMESPACE