// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/SDMSourceBlendType.h"
#include "Components/MaterialStageBlends/DMMSBAdd.h"
#include "Components/MaterialStageBlends/DMMSBColor.h"
#include "Components/MaterialStageBlends/DMMSBColorBurn.h"
#include "Components/MaterialStageBlends/DMMSBColorDodge.h"
#include "Components/MaterialStageBlends/DMMSBDarken.h"
#include "Components/MaterialStageBlends/DMMSBDarkenColor.h"
#include "Components/MaterialStageBlends/DMMSBDifference.h"
#include "Components/MaterialStageBlends/DMMSBDivide.h"
#include "Components/MaterialStageBlends/DMMSBExclusion.h"
#include "Components/MaterialStageBlends/DMMSBHardLight.h"
#include "Components/MaterialStageBlends/DMMSBHardMix.h"
#include "Components/MaterialStageBlends/DMMSBHue.h"
#include "Components/MaterialStageBlends/DMMSBLighten.h"
#include "Components/MaterialStageBlends/DMMSBLightenColor.h"
#include "Components/MaterialStageBlends/DMMSBLinearBurn.h"
#include "Components/MaterialStageBlends/DMMSBLinearDodge.h"
#include "Components/MaterialStageBlends/DMMSBLinearLight.h"
#include "Components/MaterialStageBlends/DMMSBLuminosity.h"
#include "Components/MaterialStageBlends/DMMSBMultiply.h"
#include "Components/MaterialStageBlends/DMMSBNormal.h"
#include "Components/MaterialStageBlends/DMMSBOverlay.h"
#include "Components/MaterialStageBlends/DMMSBPinLight.h"
#include "Components/MaterialStageBlends/DMMSBSaturation.h"
#include "Components/MaterialStageBlends/DMMSBScreen.h"
#include "Components/MaterialStageBlends/DMMSBSoftLight.h"
#include "Components/MaterialStageBlends/DMMSBSubtract.h"
#include "Components/MaterialStageBlends/DMMSBVividLight.h"
#include "DMEDefs.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorStyle.h"
#include "ToolMenu.h"
#include "ToolMenuDelegates.h"
#include "ToolMenus.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMSourceBlendType"

namespace UE::DynamicMaterialEditor::Private
{
	static const FName SourceBlendMenuName = "SourceBlendMenu";

	struct FDMBlendCategory
	{
		FText Name;
		TArray<UClass*> Classes;
	};

	const TArray<FDMBlendCategory>& SupportedBlendCategories()
	{
		static TArray<FDMBlendCategory> OutBlendCategories;

		if (OutBlendCategories.IsEmpty() == false)
		{
			return OutBlendCategories;
		}

		OutBlendCategories.Append(
			{
				{
					LOCTEXT("BlendNormal", "Normal Blends"),
					{
						UDMMaterialStageBlendNormal::StaticClass()
					}
				},
				{
					LOCTEXT("BlendDarken", "Darken Blends"),
					{
						UDMMaterialStageBlendDarken::StaticClass(),
						UDMMaterialStageBlendMultiply::StaticClass(),
						UDMMaterialStageBlendColorBurn::StaticClass(),
						UDMMaterialStageBlendLinearBurn::StaticClass(),
						UDMMaterialStageBlendDarkenColor::StaticClass()
					}
				},
				{
					LOCTEXT("BlendLighten", "Lighten Blends"),
					{
						UDMMaterialStageBlendAdd::StaticClass(),
						UDMMaterialStageBlendLighten::StaticClass(),
						UDMMaterialStageBlendScreen::StaticClass(),
						UDMMaterialStageBlendColorDodge::StaticClass(),
						UDMMaterialStageBlendLinearDodge::StaticClass(),
						UDMMaterialStageBlendLightenColor::StaticClass()
					}
				},
				{
					LOCTEXT("BlendContrast", "Contrast Blends"),
					{
						UDMMaterialStageBlendOverlay::StaticClass(),
						UDMMaterialStageBlendSoftLight::StaticClass(),
						UDMMaterialStageBlendHardLight::StaticClass(),
						UDMMaterialStageBlendVividLight::StaticClass(),
						UDMMaterialStageBlendLinearLight::StaticClass(),
						UDMMaterialStageBlendPinLight::StaticClass(),
						UDMMaterialStageBlendHardMix::StaticClass()
					}
				},
				{
					LOCTEXT("BlendInversion", "Inversion Blends"),
					{
						UDMMaterialStageBlendDifference::StaticClass(),
						UDMMaterialStageBlendExclusion::StaticClass(),
						UDMMaterialStageBlendSubtract::StaticClass(),
						UDMMaterialStageBlendDivide::StaticClass()
					}
				},
				{
					LOCTEXT("BlendHSL", "HSL Blends"),
					{
						UDMMaterialStageBlendColor::StaticClass(),
						UDMMaterialStageBlendHue::StaticClass(),
						UDMMaterialStageBlendSaturation::StaticClass(),
						UDMMaterialStageBlendLuminosity::StaticClass()
					}
				}
			});
		return OutBlendCategories;
	};
}

TArray<TStrongObjectPtr<UClass>> SDMSourceBlendType::SupportedBlendClasses = {};
TMap<FName, FDMBlendNameClass> SDMSourceBlendType::BlendMap = {};

void SDMSourceBlendType::Construct(const FArguments& InArgs)
{
	SelectedItem = InArgs._SelectedItem;
	OnSelectedItemChanged = InArgs._OnSelectedItemChanged;

	EnsureBlendMap();
	EnsureMenuRegistered();

	FName InitialSourceBlendTypeName = NAME_None;

	if (TSubclassOf<UDMMaterialStageBlend> Blend = SelectedItem.Get())
	{
		InitialSourceBlendTypeName = SelectedItem.Get()->GetFName();
	}

	const FComboBoxStyle& ComboBoxStyle = FAppStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	const FComboButtonStyle& ComboButtonStyle = ComboBoxStyle.ComboButtonStyle;
	const FButtonStyle& ButtonStyle = ComboButtonStyle.ButtonStyle;
	
	ChildSlot
	[
		SNew(SComboButton)
		.ComboButtonStyle(&ComboButtonStyle)
		.ButtonStyle(&ButtonStyle)
		.ContentPadding(ComboBoxStyle.ContentPadding)
		.IsFocusable(true)
		.ForegroundColor(FSlateColor::UseStyle())
		.ToolTipText(LOCTEXT("SourceBlendMode", "Source Blend Mode"))
		.OnGetMenuContent(this, &SDMSourceBlendType::MakeSourceBlendMenuWidget)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SDMSourceBlendType::GetSelectedItemText)
		]
	];
}

void SDMSourceBlendType::EnsureBlendMap()
{
	if (SupportedBlendClasses.IsEmpty() == false)
	{
		return;
	}

	SupportedBlendClasses = UDMMaterialStageBlend::GetAvailableBlends();

	for (const TStrongObjectPtr<UClass>& BlendClass : SupportedBlendClasses)
	{
		UDMMaterialStageBlend* StageBlendCDO = Cast<UDMMaterialStageBlend>(BlendClass->GetDefaultObject());

		if (ensure(IsValid(StageBlendCDO)))
		{
			const FName BlendClassName = BlendClass->GetFName();
			const FText BlendClassText = StageBlendCDO->GetDescription();
			const TSubclassOf<UDMMaterialStageBlend> BlendClassObject = TSubclassOf<UDMMaterialStageBlend>(BlendClass.Get());

			if (BlendClassName != NAME_None && !BlendClassText.IsEmpty())
			{
				BlendMap.Emplace(BlendClassName, {BlendClassText, BlendClassObject});
			}
		}
	}
}

void SDMSourceBlendType::EnsureMenuRegistered()
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (UToolMenus::Get()->IsMenuRegistered(SourceBlendMenuName))
	{
		return;
	}

	UToolMenu* NewMenu = UToolMenus::Get()->RegisterMenu(SourceBlendMenuName, NAME_None, EMultiBoxType::Menu, false);
	if (!IsValid(NewMenu))
	{
		return;
	}

	NewMenu->bToolBarForceSmallIcons = true;
	NewMenu->bShouldCloseWindowAfterMenuSelection = true;
	NewMenu->bCloseSelfOnly = true;

	NewMenu->AddDynamicSection("PopulateToolBar", FNewToolMenuDelegate::CreateStatic(&SDMSourceBlendType::MakeSourceBlendMenu));
}

TSharedRef<SWidget> SDMSourceBlendType::OnGenerateWidget(const FName InItem)
{
	EnsureBlendMap();

	if (!InItem.IsValid() || !BlendMap.Contains(InItem))
	{
		return SNullWidget::NullWidget;
	}

	return 
		SNew(STextBlock)
		.Text(BlendMap[InItem].BlendName);
}

void SDMSourceBlendType::OnSelectionChanged(const FName InNewItem, const ESelectInfo::Type InSelectInfoType)
{
	if (!BlendMap.Contains(InNewItem))
	{
		return;
	}

	TSubclassOf<UDMMaterialStageBlend> SelectedClass = BlendMap[InNewItem].BlendClass;

	if (!SelectedClass.Get())
	{
		return;
	}

	OnSelectedItemChanged.ExecuteIfBound(SelectedClass);
}

FText SDMSourceBlendType::GetSelectedItemText() const
{
	if (TSubclassOf<UDMMaterialStageBlend> Blend = SelectedItem.Get())
	{
		const FName SelectedName = Blend->GetFName();

		if (BlendMap.Contains(SelectedName))
		{
			return BlendMap[SelectedName].BlendName;
		}
	}

	return FText::GetEmpty();

}

TSharedRef<SWidget> SDMSourceBlendType::MakeSourceBlendMenuWidget()
{
	using namespace UE::DynamicMaterialEditor::Private;

	UDMSourceBlendTypeContextObject* Context = NewObject<UDMSourceBlendTypeContextObject>();
	Context->SetBlendTypeWidget(SharedThis(this));

	return UToolMenus::Get()->GenerateWidget(SourceBlendMenuName, FToolMenuContext(Context));
}

void SDMSourceBlendType::OnBlendTypeSelected(UClass* InBlendClass)
{
	OnSelectedItemChanged.ExecuteIfBound(InBlendClass);
}

bool SDMSourceBlendType::CanSelectBlendType(UClass* InBlendClass)
{
	return SelectedItem.Get() != InBlendClass;
}

bool SDMSourceBlendType::InBlendTypeSelected(UClass* InBlendClass)
{
	return SelectedItem.Get() == InBlendClass;
}

void SDMSourceBlendType::MakeSourceBlendMenu(UToolMenu* InToolMenu)
{
	using namespace UE::DynamicMaterialEditor::Private;

	static const TArray<FDMBlendCategory> BlendCategories = SupportedBlendCategories();

	if (SupportedBlendClasses.IsEmpty())
	{
		return;
	}

	UDMSourceBlendTypeContextObject* ContextObject = InToolMenu->FindContext<UDMSourceBlendTypeContextObject>();

	if (!ContextObject)
	{
		return;
	}

	TSharedPtr<SDMSourceBlendType> SourceBlendWidget = ContextObject->GetBlendTypeWidget();

	if (!SourceBlendWidget.IsValid())
	{
		return;
	}

	auto AddClasstoSection = [&SourceBlendWidget](FToolMenuSection& InSection, const FText& InName, UClass* InBlendClass)
	{
		InSection.AddMenuEntry(
			NAME_None,
			InName,
			LOCTEXT("ChangeSourceBlendTooltip", "Change the source of this stage to a Material Blend."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(SourceBlendWidget.ToSharedRef(), &SDMSourceBlendType::OnBlendTypeSelected, InBlendClass),
				FCanExecuteAction::CreateSP(SourceBlendWidget.ToSharedRef(), &SDMSourceBlendType::CanSelectBlendType, InBlendClass),
				FIsActionChecked::CreateSP(SourceBlendWidget.ToSharedRef(), &SDMSourceBlendType::InBlendTypeSelected, InBlendClass),
				EUIActionRepeatMode::RepeatDisabled
			)
		);
	};

	for (const FDMBlendCategory& BlendCategory : BlendCategories)
	{
		FToolMenuSection& BlendCategorySection = InToolMenu->AddSection(FName(BlendCategory.Name.ToString()), BlendCategory.Name);

		for (UClass* BlendCategoryClass : BlendCategory.Classes)
		{
			UDMMaterialStageBlend* BlendCDO = Cast<UDMMaterialStageBlend>(BlendCategoryClass->GetDefaultObject());

			AddClasstoSection(BlendCategorySection, BlendCDO->GetDescription(), BlendCategoryClass);
		}
	}

	FToolMenuSection& UncategorizedSection = InToolMenu->AddSection(TEXT("OtherBlends"), LOCTEXT("OtherBlends", "Other Blends"));

	for (TStrongObjectPtr<UClass> BlendClass : SupportedBlendClasses)
	{
		UDMMaterialStageBlend* BlendCDO = Cast<UDMMaterialStageBlend>(BlendClass->GetDefaultObject());

		bool bInBlendCategories = false;

		for (const FDMBlendCategory& BlendCategory : BlendCategories)
		{
			for (UClass* BlendCategoryClass : BlendCategory.Classes)
			{
				if (BlendClass.Get() == BlendCategoryClass)
				{
					bInBlendCategories = true;
					break;
				}
			}
		}

		if (bInBlendCategories)
		{
			continue;
		}

		AddClasstoSection(UncategorizedSection, BlendCDO->GetDescription(), BlendClass.Get());
	}
}

#undef LOCTEXT_NAMESPACE
