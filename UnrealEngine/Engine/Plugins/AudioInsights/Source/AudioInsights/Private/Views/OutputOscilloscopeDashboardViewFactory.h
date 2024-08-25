// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"
#include "Views/DashboardViewFactory.h"

class USoundSubmix;

namespace UE::Audio::Insights
{
	class FAudioOscilloscopeAnalyzer;

	class FOutputOscilloscopeDashboardViewFactory : public IDashboardViewFactory, public TSharedFromThis<FOutputOscilloscopeDashboardViewFactory>
	{
	public:
		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual TSharedRef<SWidget> MakeWidget() override;

	private:
		void HandleOnActiveAudioDeviceChanged();
		void HandleOnSubmixSelectionChanged(const TWeakObjectPtr<USoundSubmix> InSoundSubmix);

		TSharedPtr<FAudioOscilloscopeAnalyzer> OutputOscilloscopeAnalyzer;
		TObjectPtr<USoundSubmix> MainSubmix;
	};
} // namespace UE::Audio::Insights
