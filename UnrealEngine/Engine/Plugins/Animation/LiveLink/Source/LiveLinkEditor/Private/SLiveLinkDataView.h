// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "LiveLinkTypes.h"
#include "Types/SlateEnums.h"

class IDetailsView;
class FLiveLinkClient;
class IStructureDetailsView;

class SLiveLinkDataView : public SCompoundWidget
{
private:
	using Super = SCompoundWidget;

public:
	SLATE_BEGIN_ARGS(SLiveLinkDataView){}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, FLiveLinkClient* InClient);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void SetSubjectKey(FLiveLinkSubjectKey InSubjectKey);
	FLiveLinkSubjectKey GetSubjectKey() const { return SubjectKey; }

	void SetRefreshDelay(double DelaySeconds) { UpdateDelay = DelaySeconds; }
	double GetRefreshDelay() const { return UpdateDelay; }

private:
	enum class EDetailType : uint32
	{
		Property,
		StaticData,
		FrameData,
	};

	void OnPropertyChanged(const FPropertyChangedEvent& InEvent);
	int32 GetDetailWidgetIndex() const;
	void OnSelectDetailWidget(EDetailType InDetailType);
	bool IsSelectedDetailWidget(EDetailType InDetailType) const { return InDetailType == DetailType; }
	bool CanEditRefreshDelay() const { return DetailType != EDetailType::Property; }
	void SetRefreshDelayInternal(double InDelaySeconds, ETextCommit::Type) { SetRefreshDelay(InDelaySeconds); }

private:
	FLiveLinkClient* Client;
	FLiveLinkSubjectKey SubjectKey;
	double LastUpdateSeconds;
	double UpdateDelay;
	EDetailType DetailType;

	TSharedPtr<IStructureDetailsView> StructureDetailsView;
	TSharedPtr<IDetailsView> SettingsDetailsView;
};
