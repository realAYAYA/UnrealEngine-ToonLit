// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"
#include "Misc/OutputDevice.h"
#include "OutputLogCreationParams.h"
#include "Templates/SharedPointer.h"
#include "Textures/SlateIcon.h"
#include "UObject/NameTypes.h"
#include "Views/DashboardViewFactory.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "AudioInsights"


namespace UE::Audio::Insights
{
	class FLogDashboardViewFactory : public IDashboardViewFactory
	{
	public:
		FLogDashboardViewFactory();

		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual TSharedRef<SWidget> MakeWidget() override;

	private:
		struct FLogCategoryCollector : public FOutputDevice
		{
			FLogCategoryCollector();
			~FLogCategoryCollector();

			FDefaultCategorySelectionMap GetCollectedCategories() const;
			void RunAsync();

			virtual bool IsMemoryOnly() const override;
			virtual void Serialize(const TCHAR* InMsg, ELogVerbosity::Type Verbosity, const FName& InCategory) override;

		private:
			mutable FCriticalSection CollectionCritSec;
			FDefaultCategorySelectionMap CollectedCategories;
		};

		FLogCategoryCollector CategoryCollector;
	};
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
