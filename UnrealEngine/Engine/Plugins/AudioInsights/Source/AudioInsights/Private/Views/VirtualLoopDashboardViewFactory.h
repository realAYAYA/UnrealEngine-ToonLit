// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Views/TableDashboardViewFactory.h"


namespace UE::Audio::Insights
{
	class FVirtualLoopDashboardViewFactory : public FTraceObjectTableDashboardViewFactory
	{
	public:
		FVirtualLoopDashboardViewFactory();
		virtual ~FVirtualLoopDashboardViewFactory() = default;

		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override;

	protected:
		virtual void ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason Reason) override;
		virtual const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& GetColumns() const override;

		virtual void SortTable() override;

#if WITH_EDITOR
		virtual bool IsDebugDrawEnabled() const override;
		virtual void DebugDraw(float InElapsed, const IDashboardDataViewEntry& InEntry, ::Audio::FDeviceId DeviceId) const override;
#endif // WITH_EDITOR

	private:
		FSoundAttenuationVisualizer AttenuationVisualizer;
	};
} // namespace UE::Audio::Insights
