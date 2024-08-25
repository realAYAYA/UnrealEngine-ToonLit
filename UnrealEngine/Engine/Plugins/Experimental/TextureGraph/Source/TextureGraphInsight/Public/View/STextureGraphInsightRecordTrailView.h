// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

//#include "SlateBasics.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"

#include "Model/TextureGraphInsightSession.h"


class TEXTUREGRAPHINSIGHT_API STextureGraphInsightRecordTrailView : public SCompoundWidget
{
	TSharedPtr< SBreadcrumbTrail<RecordID> > _trail;
public:
	SLATE_BEGIN_ARGS(STextureGraphInsightRecordTrailView) {}
		SLATE_ARGUMENT(RecordID, recordID)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	void Refresh(RecordID rid);

	static RecordID FindRootRecord(RecordID rid);

	void OnCrumbClicked(const RecordID& blob);
};


