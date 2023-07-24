// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterColorGradingGenerator_ColorCorrectRegion.h"

#include "ClassIconFinder.h"
#include "ColorCorrectRegion.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailCustomization.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "PropertyHandle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DisplayClusterColorGrading"

TSharedRef<IDisplayClusterColorGradingDataModelGenerator> FDisplayClusterColorGradingGenerator_ColorCorrectRegion::MakeInstance()
{
	return MakeShareable(new FDisplayClusterColorGradingGenerator_ColorCorrectRegion());
}

class FColorCorrectRegionCustomization : public IDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TArray<FName> Categories;
		DetailBuilder.GetCategoryNames(Categories);

		for (const FName& Category : Categories)
		{
			if (Category != TEXT("Color Correction"))
			{
				DetailBuilder.HideCategory(Category);
			}
		}

		// TransformCommon is a custom category that doesn't get returned by GetCategoryNames that also needs to be hidden
		DetailBuilder.HideCategory(TEXT("TransformCommon"));

		IDetailCategoryBuilder& CCCategoryBuilder = DetailBuilder.EditCategory(TEXT("Color Correction"));

		TArray<TSharedRef<IPropertyHandle>> CCPropertyHandles;
		CCCategoryBuilder.GetDefaultProperties(CCPropertyHandles);

		IDetailCategoryBuilder& ColorGradingElementsCategory = DetailBuilder.EditCategory(FName("ColorGradingElements"));

		ColorGradingElementsCategory.AddProperty(DetailBuilder.GetProperty((GET_MEMBER_NAME_CHECKED(AColorCorrectionRegion, ColorGradingSettings.Global))));
		ColorGradingElementsCategory.AddProperty(DetailBuilder.GetProperty((GET_MEMBER_NAME_CHECKED(AColorCorrectionRegion, ColorGradingSettings.Shadows))));
		ColorGradingElementsCategory.AddProperty(DetailBuilder.GetProperty((GET_MEMBER_NAME_CHECKED(AColorCorrectionRegion, ColorGradingSettings.Midtones))));
		ColorGradingElementsCategory.AddProperty(DetailBuilder.GetProperty((GET_MEMBER_NAME_CHECKED(AColorCorrectionRegion, ColorGradingSettings.Highlights))));
	}
};

void FDisplayClusterColorGradingGenerator_ColorCorrectRegion::Initialize(const TSharedRef<class FDisplayClusterColorGradingDataModel>& ColorGradingDataModel, const TSharedRef<IPropertyRowGenerator>& PropertyRowGenerator)
{
	PropertyRowGenerator->RegisterInstancedCustomPropertyLayout(AColorCorrectRegion::StaticClass(), FOnGetDetailCustomizationInstance::CreateLambda([]
	{
		return MakeShared<FColorCorrectRegionCustomization>();
	}));
}

void FDisplayClusterColorGradingGenerator_ColorCorrectRegion::Destroy(const TSharedRef<class FDisplayClusterColorGradingDataModel>& ColorGradingDataModel, const TSharedRef<IPropertyRowGenerator>& PropertyRowGenerator)
{
	PropertyRowGenerator->UnregisterInstancedCustomPropertyLayout(AColorCorrectRegion::StaticClass());
}

void FDisplayClusterColorGradingGenerator_ColorCorrectRegion::GenerateDataModel(IPropertyRowGenerator& PropertyRowGenerator, FDisplayClusterColorGradingDataModel& OutColorGradingDataModel)
{
	TArray<TWeakObjectPtr<AColorCorrectRegion>> SelectedCCRs;
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyRowGenerator.GetSelectedObjects();
	for (const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid() && SelectedObject->IsA<AColorCorrectRegion>())
		{
			TWeakObjectPtr<AColorCorrectRegion> SelectedRootActor = CastChecked<AColorCorrectRegion>(SelectedObject.Get());
			SelectedCCRs.Add(SelectedRootActor);
		}
	}

	if (!SelectedCCRs.Num())
	{
		return;
	}

	const TArray<TSharedRef<IDetailTreeNode>>& RootNodes = PropertyRowGenerator.GetRootTreeNodes();

	const TSharedRef<IDetailTreeNode>* ColorGradingElementsPtr = RootNodes.FindByPredicate([](const TSharedRef<IDetailTreeNode>& Node)
	{
		return Node->GetNodeName() == TEXT("ColorGradingElements");
	});

	if (ColorGradingElementsPtr)
	{
		const TSharedRef<IDetailTreeNode> ColorGradingElements = *ColorGradingElementsPtr;
		FDisplayClusterColorGradingDataModel::FColorGradingGroup ColorGradingGroup;

		ColorGradingGroup.DetailsViewCategories.Add(TEXT("Color Correction"));

		TArray<TSharedRef<IDetailTreeNode>> ColorGradingPropertyNodes;
		ColorGradingElements->GetChildren(ColorGradingPropertyNodes);

		for (const TSharedRef<IDetailTreeNode>& PropertyNode : ColorGradingPropertyNodes)
		{
			TSharedPtr<IPropertyHandle> PropertyHandle = PropertyNode->CreatePropertyHandle();

			FDisplayClusterColorGradingDataModel::FColorGradingElement ColorGradingElement = CreateColorGradingElement(PropertyNode, FText::FromName(PropertyNode->GetNodeName()));
			ColorGradingGroup.ColorGradingElements.Add(ColorGradingElement);
		}

		ColorGradingGroup.GroupHeaderWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0, 1, 6, 1))
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(16)
				.HeightOverride(16)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FClassIconFinder::FindIconForActor(SelectedCCRs[0]))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(FText::FromString(SelectedCCRs[0]->GetActorLabel()))
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			];

		OutColorGradingDataModel.ColorGradingGroups.Add(ColorGradingGroup);
	}
}

FDisplayClusterColorGradingDataModel::FColorGradingElement FDisplayClusterColorGradingGenerator_ColorCorrectRegion::CreateColorGradingElement(const TSharedRef<IDetailTreeNode>& GroupNode, FText ElementLabel)
{
	FDisplayClusterColorGradingDataModel::FColorGradingElement ColorGradingElement;
	ColorGradingElement.DisplayName = ElementLabel;

	TArray<TSharedRef<IDetailTreeNode>> ChildNodes;
	GroupNode->GetChildren(ChildNodes);

	for (const TSharedRef<IDetailTreeNode>& ChildNode : ChildNodes)
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = ChildNode->CreatePropertyHandle();
		if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
		{
			const FString ColorGradingModeString = PropertyHandle->GetProperty()->GetMetaData(TEXT("ColorGradingMode")).ToLower();

			if (!ColorGradingModeString.IsEmpty())
			{
				if (ColorGradingModeString.Compare(TEXT("saturation")) == 0)
				{
					ColorGradingElement.SaturationPropertyHandle = PropertyHandle;
				}
				else if (ColorGradingModeString.Compare(TEXT("contrast")) == 0)
				{
					ColorGradingElement.ContrastPropertyHandle = PropertyHandle;
				}
				else if (ColorGradingModeString.Compare(TEXT("gamma")) == 0)
				{
					ColorGradingElement.GammaPropertyHandle = PropertyHandle;
				}
				else if (ColorGradingModeString.Compare(TEXT("gain")) == 0)
				{
					ColorGradingElement.GainPropertyHandle = PropertyHandle;
				}
				else if (ColorGradingModeString.Compare(TEXT("offset")) == 0)
				{
					ColorGradingElement.OffsetPropertyHandle = PropertyHandle;
				}
			}
		}
	}

	return ColorGradingElement;
}

bool FDisplayClusterColorGradingGenerator_ColorCorrectRegion::FilterDetailsViewProperties(const TSharedRef<IDetailTreeNode>& InDetailTreeNode)
{
	if (InDetailTreeNode->GetNodeType() == EDetailNodeType::Category)
	{
		return InDetailTreeNode->GetNodeName() == TEXT("Color Correction");
	}
	else
	{
		return true;
	}
}

#undef LOCTEXT_NAMESPACE