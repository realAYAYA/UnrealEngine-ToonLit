// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMMaterialListExtensionWidget.h"
#include "AssetToolsModule.h"
#include "Components/PrimitiveComponent.h"
#include "DMObjectMaterialProperty.h"
#include "DMWorldSubsystem.h"
#include "DetailLayoutBuilder.h"
#include "Engine/World.h"
#include "IAssetTools.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "MaterialList.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialListExtensionWidget"

void SDMMaterialListExtensionWidget::Construct(const FArguments& InArgs, const TSharedRef<FMaterialItemView>& InMaterialItemView, 
	UPrimitiveComponent* InCurrentComponent, IDetailLayoutBuilder& InDetailBuilder)
{
	MaterialItemViewWeak = InMaterialItemView;
	CurrentComponentWeak = InCurrentComponent;

	if (!CurrentComponentWeak.IsValid())
	{
		return;
	}

	// @formatter:off
	ChildSlot
	[
		SNew(SBox)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		[
			SNew(SButton)
			.OnClicked(this, &SDMMaterialListExtensionWidget::OnButtonClicked)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SDMMaterialListExtensionWidget::GetButtonText)
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
	];
	// @formatter:on
}

UObject* SDMMaterialListExtensionWidget::GetAsset() const
{
	UPrimitiveComponent* CurrentComponent = CurrentComponentWeak.Get();
	if (!ensure(CurrentComponent))
	{
		return nullptr;
	}

	const TSharedPtr<FMaterialItemView> MaterialItemView = MaterialItemViewWeak.Pin();
	if (!ensure(MaterialItemView.IsValid()))
	{
		return nullptr;
	}

	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(MaterialItemView->GetMaterialListItem().Material.Get());
	if (!MaterialInterface)
	{
		return nullptr;
	}

	return MaterialInterface;
}

UDynamicMaterialInstance* SDMMaterialListExtensionWidget::GetDynamicMaterialInstance() const
{
	return Cast<UDynamicMaterialInstance>(GetAsset());
}

void SDMMaterialListExtensionWidget::SetAsset(UObject* NewAsset)
{
	UPrimitiveComponent* CurrentComponent = CurrentComponentWeak.Get();
	if (!ensure(CurrentComponent))
	{
		return;
	}

	const TSharedPtr<FMaterialItemView> MaterialItemView = MaterialItemViewWeak.Pin();
	if (!ensure(MaterialItemView.IsValid()))
	{
		return;
	}

	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(NewAsset);
	if (!MaterialInterface)
	{
		return;
	}

	MaterialItemView->ReplaceMaterial(MaterialInterface);
}

void SDMMaterialListExtensionWidget::SetDynamicMaterialInstance(UDynamicMaterialInstance* NewInstance)
{
	if (MaterialItemViewWeak.IsValid())
	{
		if (UPrimitiveComponent* CurrentComponent = CurrentComponentWeak.Get())
		{
			if (UWorld* World = CurrentComponent->GetWorld())
			{
				if (UDMWorldSubsystem* WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
				{
					if (WorldSubsystem->GetInvokeTabDelegate().IsBound())
					{
						if (WorldSubsystem->GetMaterialValueSetterDelegate().IsBound())
						{
							const FMaterialListItem& ListItem = MaterialItemViewWeak.Pin()->GetMaterialListItem();
							const FDMObjectMaterialProperty MaterialProperty(CurrentComponent, ListItem.SlotIndex);
							WorldSubsystem->GetMaterialValueSetterDelegate().Execute(MaterialProperty, NewInstance);
						}

						return;
					}
				}
			}
		}
	}

	SetAsset(NewInstance);
}

FText SDMMaterialListExtensionWidget::GetButtonText() const
{
	if (GetDynamicMaterialInstance())
	{
		return LOCTEXT("OpenMaterialDesignerModel", "Edit with Material Designer");
	}

	return LOCTEXT("CreateMaterialDesignerModel", "Create with Material Designer");
}

FReply SDMMaterialListExtensionWidget::OnButtonClicked()
{
	if (GetDynamicMaterialInstance())
	{
		return OpenDynamicMaterialInstanceTab();
	}

	return CreateDynamicMaterialInstance();
}

FReply SDMMaterialListExtensionWidget::CreateDynamicMaterialInstance()
{
	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// We already have an instance, so we don't need to create one
	if (Instance)
	{
		return FReply::Unhandled();
	}

	FName InstanceName = MakeUniqueObjectName(CurrentComponentWeak.Get(), UDynamicMaterialInstance::StaticClass(), "DynamicMaterialInstance");

	UDynamicMaterialInstanceFactory* DynamicMaterialInstanceFactory = NewObject<UDynamicMaterialInstanceFactory>();
	check(DynamicMaterialInstanceFactory);

	UDynamicMaterialInstance* NewInstance = Cast<UDynamicMaterialInstance>(DynamicMaterialInstanceFactory->FactoryCreateNew(
		UDynamicMaterialInstance::StaticClass(),
		CurrentComponentWeak.Get(),
		InstanceName,
		RF_NoFlags,
		nullptr,
		GWarn
	));

	SetDynamicMaterialInstance(NewInstance);

	return OpenDynamicMaterialInstanceTab();
}

FReply SDMMaterialListExtensionWidget::ClearDynamicMaterialInstance()
{
	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// We don't have an instance, so we don't need to clear it (or any other asset in its place)
	if (!Instance)
	{
		return FReply::Unhandled();
	}

	SetDynamicMaterialInstance(nullptr);

	return FReply::Handled();
}

FReply SDMMaterialListExtensionWidget::OpenDynamicMaterialInstanceTab()
{
	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// We don't have an instance, so we can't open it
	if (!Instance)
	{
		return FReply::Unhandled();
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.OpenEditorForAssets({Instance});

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
