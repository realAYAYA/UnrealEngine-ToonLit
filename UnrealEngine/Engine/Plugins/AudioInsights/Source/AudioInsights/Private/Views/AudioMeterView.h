// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

class UAudioBus;
class USoundSubmix;

namespace UE::Audio::Insights
{
	class FAudioMeterAnalyzer;
	class FAudioMeterSubmixAnalyzer;

	class FAudioMeterView : public TSharedFromThis<FAudioMeterView>
	{
	public:
		using FAudioAssetVariant = TVariant<TWeakObjectPtr<UAudioBus>, TWeakObjectPtr<USoundSubmix>>;
		using FAudioMeterVariant = TVariant<TSharedPtr<FAudioMeterAnalyzer>, TSharedPtr<FAudioMeterSubmixAnalyzer>>;

		FAudioMeterView(FAudioAssetVariant InAudioAssetVariant);
		virtual ~FAudioMeterView();

		TSharedRef<SWidget> GetWidget() const { return AudioMeterViewWidget; };

	private:
		FAudioMeterVariant MakeAudioMeterAnalyzerVariant(const FAudioAssetVariant InAudioAssetVariant);
		TSharedRef<STextBlock> MakeAudioAssetNameTextBlock(const FAudioAssetVariant InAudioAssetVariant);
		TSharedRef<SWidget> MakeWidget();

		TSharedRef<SWidget> GetAudioMeterAnalyzerWidget() const;

		void HandleOnActiveAudioDeviceChanged();

		FAudioAssetVariant AudioAssetVariant;
		FAudioMeterVariant AudioMeterAnalyzerVariant;

		TSharedRef<STextBlock> AudioAssetNameTextBlock;

		TSharedRef<SWidget> AudioMeterViewWidget;

		FDelegateHandle OnActiveAudioDeviceChangedHandle;
	};
} // namespace UE::Audio::Insights
