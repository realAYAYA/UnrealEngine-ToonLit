// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class UNearestNeighborModelSection;
namespace UE::NearestNeighborModel
{
	class SNearestNeighborModelSectionWidget : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SNearestNeighborModelSectionWidget) {}
			SLATE_ARGUMENT(UNearestNeighborModelSection*, Section)
		SLATE_END_ARGS()
		
		void Construct(const FArguments& InArgs);
	
	private:
		TObjectPtr<UNearestNeighborModelSection> Section;
		TSharedPtr<IDetailsView> DetailsView;
	};

	class FNearestNeighborModelSectionCustomization : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShared<FNearestNeighborModelSectionCustomization>();
		}

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	};
};
