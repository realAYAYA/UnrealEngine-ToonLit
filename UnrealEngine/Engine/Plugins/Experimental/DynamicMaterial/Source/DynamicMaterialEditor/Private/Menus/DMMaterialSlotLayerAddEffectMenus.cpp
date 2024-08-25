// Copyright Epic Games, Inc. All Rights Reserved.

#include "Menus/DMMaterialSlotLayerAddEffectMenus.h"

#include "Components/DMMaterialEffect.h"
#include "Components/DMMaterialEffectFunction.h"
#include "Components/DMMaterialEffectStack.h"
#include "Components/DMMaterialLayer.h"
#include "DynamicMaterialEditorSettings.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Menus/DMMaterialSlotLayerAddEffectContext.h"
#include "Menus/DMMenuContext.h"
#include "ToolMenus.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "DMMaterialSlotLayerAddEffectMenus"

namespace UE::DynamicMaterialEditor::Private
{
	static const FName AddEffectMenuName(TEXT("MaterialDesigner.Slot.Layer.AddEffect"));
	static const FName AddEffectMenuSection(TEXT("AddEffect"));

	bool CanAddEffect(const FToolMenuContext& InContext, TSoftObjectPtr<UMaterialFunctionInterface> InMaterialFunctionPtr)
	{
		const UDMMaterialLayerObject* Layer = nullptr;

		if (UDMMenuContext* MenuContext = InContext.FindContext<UDMMenuContext>())
		{
			Layer = MenuContext->GetLayer();
		}
		else if (UDMMaterialSlotLayerAddEffectContext* SlotContext = InContext.FindContext<UDMMaterialSlotLayerAddEffectContext>())
		{
			Layer = SlotContext->GetLayer();
		}

		if (!Layer || !IsValid(Layer))
		{
			return false;
		}

		UDMMaterialEffectStack* EffectStack = Layer->GetEffectStack();

		if (!EffectStack)
		{
			return false;
		}

		UMaterialFunctionInterface* MaterialFunction = InMaterialFunctionPtr.LoadSynchronous();

		if (!MaterialFunction)
		{
			return false;
		}

		for (UDMMaterialEffect* Effect : EffectStack->GetEffects())
		{
			if (UDMMaterialEffectFunction* EffectFunction = Cast<UDMMaterialEffectFunction>(Effect))
			{
				if (EffectFunction->GetMaterialFunction() == MaterialFunction)
				{
					return false;
				}
			}
		}

		return true;
	}

	void AddEffect(const FToolMenuContext& InContext, TSoftObjectPtr<UMaterialFunctionInterface> InMaterialFunctionPtr)
	{
		const UDMMaterialLayerObject* Layer = nullptr;

		if (UDMMenuContext* MenuContext = InContext.FindContext<UDMMenuContext>())
		{
			Layer = MenuContext->GetLayer();
		}
		else if (UDMMaterialSlotLayerAddEffectContext* SlotContext = InContext.FindContext<UDMMaterialSlotLayerAddEffectContext>())
		{
			Layer = SlotContext->GetLayer();
		}

		if (!Layer || !IsValid(Layer))
		{
			return;
		}

		UDMMaterialEffectStack* EffectStack = Layer->GetEffectStack();

		if (!EffectStack)
		{
			return;
		}

		UMaterialFunctionInterface* MaterialFunction = InMaterialFunctionPtr.LoadSynchronous();

		if (!MaterialFunction)
		{
			return;
		}

		UDMMaterialEffectFunction* EffectFunction = UDMMaterialEffect::CreateEffect<UDMMaterialEffectFunction>(EffectStack);
		EffectFunction->SetMaterialFunction(MaterialFunction);

		// Some error applying it
		if (EffectFunction->GetMaterialFunction() != MaterialFunction)
		{
			return;
		}

		EffectStack->AddEffect(EffectFunction);
	}

	void GenerateAddEffectSubMenu(UToolMenu* InMenu, int32 InCategoryIndex)
	{
		if (!InMenu)
		{
			return;
		}

		const UDynamicMaterialEditorSettings* Settings = GetDefault<UDynamicMaterialEditorSettings>();

		if (!Settings)
		{
			return;
		}

		const TArray<FDMMaterialEffectList>& EffectList = Settings->GetEffectList();

		if (!EffectList.IsValidIndex(InCategoryIndex))
		{
			return;
		}

		FToolMenuSection& Section = InMenu->AddSection("EffectList", LOCTEXT("EffectList", "Effect List"));

		for (const TSoftObjectPtr<UMaterialFunctionInterface>& Effect : EffectList[InCategoryIndex].Effects)
		{
			UMaterialFunctionInterface* MaterialFunction = Effect.LoadSynchronous();

			if (!MaterialFunction)
			{
				continue;
			}

			FToolUIAction Action;
			Action.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&AddEffect, Effect);
			Action.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanAddEffect, Effect);

			const FString& Description = MaterialFunction->GetUserExposedCaption();
			const FString ToolTip = MaterialFunction->GetDescription();

			Section.AddMenuEntry(
				FName(*Description),
				FText::FromString(Description),
				FText::FromString(ToolTip),
				FSlateIcon(),
				FToolUIActionChoice(Action)
			);
		}
	}

	void GenerateAddEffectMenu(UToolMenu* InMenu)
	{
		if (!InMenu)
		{
			return;
		}

		const UDynamicMaterialEditorSettings* Settings = GetDefault<UDynamicMaterialEditorSettings>();

		if (!Settings)
		{
			return;
		}

		FToolMenuSection& Section = InMenu->AddSection("AddEffect", LOCTEXT("AddEffect", "Add Effect"));

		const TArray<FDMMaterialEffectList>& EffectList = Settings->GetEffectList();

		for (int32 CategoryIndex = 0; CategoryIndex < EffectList.Num(); ++CategoryIndex)
		{
			Section.AddSubMenu(
				NAME_None,
				FText::FromString(EffectList[CategoryIndex].Name),
				FText::GetEmpty(),
				FNewToolMenuChoice(FNewToolMenuDelegate::CreateStatic(&GenerateAddEffectSubMenu, CategoryIndex))
			);
		}
	}

	void RegisterAddEffectMenu()
	{
		using namespace UE::DynamicMaterialEditor::Private;

		if (UToolMenus::Get()->IsMenuRegistered(AddEffectMenuName))
		{
			return;
		}

		UToolMenu* const Menu = UToolMenus::Get()->RegisterMenu(AddEffectMenuName);

		Menu->AddDynamicSection("AddEffectSection", FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* InMenu)
			{
				GenerateAddEffectMenu(InMenu);
			}));
	}
}

TSharedRef<SWidget> FDMMaterialSlotLayerAddEffectMenus::OpenAddEffectMenu(UDMMaterialLayerObject* InLayer)
{
	using namespace UE::DynamicMaterialEditor::Private;

	RegisterAddEffectMenu();

	UDMMaterialSlotLayerAddEffectContext* ContextObject = NewObject<UDMMaterialSlotLayerAddEffectContext>();
	ContextObject->SetLayer(InLayer);

	return UToolMenus::Get()->GenerateWidget(AddEffectMenuName, FToolMenuContext(ContextObject));
}

void FDMMaterialSlotLayerAddEffectMenus::AddEffectSubMenu(UToolMenu* InMenu, UDMMaterialLayerObject* InLayer)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (InMenu->ContainsSection(AddEffectMenuSection))
	{
		return;
	}

	FToolMenuSection& NewSection = InMenu->AddSection(
		AddEffectMenuSection,
		LOCTEXT("Effects", "Effects")
	);

	NewSection.AddSubMenu(
		NAME_None,
		LOCTEXT("AddEffect", "Add Effect"),
		FText::GetEmpty(),
		FNewToolMenuChoice(FNewToolMenuDelegate::CreateStatic(&GenerateAddEffectMenu))
	);
}

#undef LOCTEXT_NAMESPACE
