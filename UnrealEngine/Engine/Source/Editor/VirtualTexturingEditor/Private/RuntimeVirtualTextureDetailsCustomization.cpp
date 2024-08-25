// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTextureDetailsCustomization.h"

#include "AssetToolsModule.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Factories/Texture2dFactoryNew.h"
#include "RuntimeVirtualTextureBuildStreamingMips.h"
#include "RuntimeVirtualTextureSetBounds.h"
#include "ScopedTransaction.h"
#include "SResetToDefaultMenu.h"
#include "VirtualTextureBuilderFactory.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/VirtualTextureBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VirtualTexturingEditorModule"

FRuntimeVirtualTextureDetailsCustomization::FRuntimeVirtualTextureDetailsCustomization()
	: VirtualTexture(nullptr)
{
}

TSharedRef<IDetailCustomization> FRuntimeVirtualTextureDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FRuntimeVirtualTextureDetailsCustomization);
}

namespace
{
	// Helper for adding text containing real values to the properties that are edited as power (or multiple) of 2
	void AddTextToProperty(IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& CategoryBuilder, FName const& PropertyName, TSharedPtr<STextBlock>& TextBlock)
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = DetailBuilder.GetProperty(PropertyName);
		DetailBuilder.HideProperty(PropertyHandle);

		TSharedPtr<SResetToDefaultMenu> ResetToDefaultMenu;

		CategoryBuilder.AddCustomRow(PropertyHandle->GetPropertyDisplayName())
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(4.0f)
			[
				SNew(SWrapBox)
				.UseAllottedSize(true)

				+ SWrapBox::Slot()
				.Padding(FMargin(0.0f, 2.0f, 2.0f, 0.0f))
				[
					SAssignNew(TextBlock, STextBlock)
				]
			]

			+ SHorizontalBox::Slot()
			[
				PropertyHandle->CreatePropertyValueWidget()
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f)
			[
				// Would be better to use SResetToDefaultPropertyEditor here but that is private in the PropertyEditor lib
				SAssignNew(ResetToDefaultMenu, SResetToDefaultMenu)
			]
		];

		ResetToDefaultMenu->AddProperty(PropertyHandle.ToSharedRef());
	}
}

void FRuntimeVirtualTextureDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get and store the linked URuntimeVirtualTexture
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 1)
	{
		return;
	}
	VirtualTexture = Cast<URuntimeVirtualTexture>(ObjectsBeingCustomized[0].Get());
	if (VirtualTexture == nullptr)
	{
		return;
	}

	// Set UIMax dependent on adaptive page table setting
	FString MaxTileCountString = FString::Printf(TEXT("%d"), URuntimeVirtualTexture::GetMaxTileCountLog2(VirtualTexture->GetAdaptivePageTable()));
	DetailBuilder.GetProperty(FName(TEXT("TileCount")))->SetInstanceMetaData("UIMax", MaxTileCountString);

	// Add size helpers
	IDetailCategoryBuilder& SizeCategory = DetailBuilder.EditCategory("Size", FText::GetEmpty());
	AddTextToProperty(DetailBuilder, SizeCategory, "TileCount", TileCountText);
	AddTextToProperty(DetailBuilder, SizeCategory, "TileSize", TileSizeText);
	AddTextToProperty(DetailBuilder, SizeCategory, "TileBorderSize", TileBorderSizeText);

	// Add details block
	IDetailCategoryBuilder& DetailsCategory = DetailBuilder.EditCategory("Details", FText::GetEmpty(), ECategoryPriority::Important);
	static const FText CustomRowSizeText = LOCTEXT("Details_RowFilter_Size", "Virtual Size");
	DetailsCategory.AddCustomRow(CustomRowSizeText)
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("Details_Size", "Virtual Texture Size"))
		.ToolTipText(LOCTEXT("Details_Size_Tooltip", "Virtual resolution derived from Size properties."))
	]
	.ValueContent()
	[
		SAssignNew(SizeText, STextBlock)
	];
	static const FText CustomRowPageTableSizeText = LOCTEXT("Details_RowFilter_PageTableSize", "Page Table Size");
	DetailsCategory.AddCustomRow(CustomRowPageTableSizeText)
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("Details_PageTableSize", "Page Table Size"))
		.ToolTipText(LOCTEXT("Details_PageTableSize_Tooltip", "Final page table size. This can vary according to the adaptive page table setting."))
	]
	.ValueContent()
	[
		SAssignNew(PageTableSizeText, STextBlock)
	];

	// Cache detail builder to refresh view updates
	CachedDetailBuilder = &DetailBuilder;

	// Add refresh callback for all properties 
	DetailBuilder.GetProperty(FName(TEXT("TileCount")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshTextDetails));
	DetailBuilder.GetProperty(FName(TEXT("TileSize")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshTextDetails));
	DetailBuilder.GetProperty(FName(TEXT("TileBorderSize")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshTextDetails));
	DetailBuilder.GetProperty(FName(TEXT("bAdaptive")))->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FRuntimeVirtualTextureDetailsCustomization::RefreshDetailsView));

	// Initialize text blocks
	RefreshTextDetails();
}

void FRuntimeVirtualTextureDetailsCustomization::RefreshTextDetails()
{
	FNumberFormattingOptions SizeOptions;
	SizeOptions.UseGrouping = false;
	SizeOptions.MaximumFractionalDigits = 0;

 	TileCountText->SetText(FText::Format(LOCTEXT("Details_Number", "{0}"), FText::AsNumber(VirtualTexture->GetTileCount(), &SizeOptions)));
	TileSizeText->SetText(FText::Format(LOCTEXT("Details_Number", "{0}"), FText::AsNumber(VirtualTexture->GetTileSize(), &SizeOptions)));
 	TileBorderSizeText->SetText(FText::Format(LOCTEXT("Details_Number", "{0}"), FText::AsNumber(VirtualTexture->GetTileBorderSize(), &SizeOptions)));

	FString SizeUnits = TEXT("Texels");
	int32 Size = VirtualTexture->GetSize();
	int32 SizeLog2 = FMath::CeilLogTwo(Size);
	if (SizeLog2 >= 30)
	{
		Size = Size >> 30;
		SizeUnits = TEXT("GiTexels");
	}
	else if (SizeLog2 >= 20)
	{
		Size = Size >> 20;
		SizeUnits = TEXT("MiTexels");
	}
	else if (SizeLog2 >= 10)
	{
		Size = Size >> 10;
		SizeUnits = TEXT("KiTexels");
	}
	SizeText->SetText(FText::Format(LOCTEXT("Details_Number_Units", "{0} {1}"), FText::AsNumber(Size, &SizeOptions), FText::FromString(SizeUnits)));

	PageTableSizeText->SetText(FText::Format(LOCTEXT("Details_Number", "{0}"), FText::AsNumber(VirtualTexture->GetPageTableSize(), &SizeOptions)));
}

void FRuntimeVirtualTextureDetailsCustomization::RefreshDetailsView()
{
	if (CachedDetailBuilder != nullptr)
	{
		CachedDetailBuilder->ForceRefreshDetails();
	}
}


FRuntimeVirtualTextureComponentDetailsCustomization::FRuntimeVirtualTextureComponentDetailsCustomization()
{
}

TSharedRef<IDetailCustomization> FRuntimeVirtualTextureComponentDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FRuntimeVirtualTextureComponentDetailsCustomization);
}

void FRuntimeVirtualTextureComponentDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get and store the linked URuntimeVirtualTextureComponent.
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 1)
	{
		return;
	}
	RuntimeVirtualTextureComponent = Cast<URuntimeVirtualTextureComponent>(ObjectsBeingCustomized[0].Get());
	if (RuntimeVirtualTextureComponent == nullptr)
	{
		return;
	}

	// Apply custom widget for SetBounds.
	TSharedRef<IPropertyHandle> SetBoundsPropertyHandle = DetailBuilder.GetProperty(TEXT("bSetBoundsButton"));
	DetailBuilder.EditDefaultProperty(SetBoundsPropertyHandle)->CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("Button_SetBounds", "Set Bounds"))
		.ToolTipText(LOCTEXT("Button_SetBounds_Tooltip", "Set the rotation to match the Bounds Align Actor and expand bounds to include all primitives that write to this virtual texture."))
	]
	.ValueContent()
	.MinDesiredWidth(125.f)
	[
		SNew(SButton)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.ContentPadding(2)
		.Text(LOCTEXT("Button_SetBounds", "Set Bounds"))
		.OnClicked(this, &FRuntimeVirtualTextureComponentDetailsCustomization::SetBounds)
		.IsEnabled(this, &FRuntimeVirtualTextureComponentDetailsCustomization::IsSetBoundsEnabled)
	];

	// Apply custom widget for BuildStreamingMips.
	TSharedRef<IPropertyHandle> BuildStreamingMipsPropertyHandle = DetailBuilder.GetProperty(TEXT("bBuildStreamingMipsButton"));
	DetailBuilder.EditDefaultProperty(BuildStreamingMipsPropertyHandle)->CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("Button_BuildStreamingTexture", "Build Streaming Texture"))
		.ToolTipText(LOCTEXT("Button_Build_Tooltip", "Build the low mips as streaming virtual texture data"))
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(2)
			.Text(LOCTEXT("Button_Build", "Build"))
			.OnClicked(this, &FRuntimeVirtualTextureComponentDetailsCustomization::BuildStreamedMips)
			.IsEnabled(this, &FRuntimeVirtualTextureComponentDetailsCustomization::IsBuildStreamedMipsEnabled)
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Icons.Warning"))
			.Visibility(this, &FRuntimeVirtualTextureComponentDetailsCustomization::IsBuildWarningIconVisible)
			.ToolTipText(LOCTEXT("Warning_Build_Tooltip", "The settings have changed since the Streaming Texture was last rebuilt. Streaming mips are disabled."))
		]
	];
}

bool FRuntimeVirtualTextureComponentDetailsCustomization::IsSetBoundsEnabled() const
{
	return RuntimeVirtualTextureComponent->GetVirtualTexture() != nullptr;
}

FReply FRuntimeVirtualTextureComponentDetailsCustomization::SetBounds()
{
	if (RuntimeVirtualTextureComponent->GetVirtualTexture() != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("Transaction_SetBounds", "Set RuntimeVirtualTextureComponent Bounds"));
		RuntimeVirtualTexture::SetBounds(RuntimeVirtualTextureComponent);
		// Force update of editor view widget.
		GEditor->NoteSelectionChange(false);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

bool FRuntimeVirtualTextureComponentDetailsCustomization::IsBuildStreamedMipsEnabled() const
{
	return RuntimeVirtualTextureComponent->GetVirtualTexture() != nullptr;
}

EVisibility FRuntimeVirtualTextureComponentDetailsCustomization::IsBuildWarningIconVisible() const
{
	const bool bVisible = RuntimeVirtualTextureComponent->IsStreamingTextureInvalid();
	return bVisible ? EVisibility::Visible : EVisibility::Hidden;
}

FReply FRuntimeVirtualTextureComponentDetailsCustomization::BuildStreamedMips()
{
	// Create a new asset if none is already bound
	UVirtualTextureBuilder* CreatedTexture = nullptr;
	if (RuntimeVirtualTextureComponent->GetVirtualTexture() != nullptr && RuntimeVirtualTextureComponent->GetStreamingTexture() == nullptr)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

		const FString DefaultPath = FPackageName::GetLongPackagePath(RuntimeVirtualTextureComponent->GetVirtualTexture()->GetPathName());
		const FString DefaultName = FPackageName::GetShortName(RuntimeVirtualTextureComponent->GetVirtualTexture()->GetName() + TEXT("_SVT"));

		UFactory* Factory = NewObject<UVirtualTextureBuilderFactory>();
		UObject* Object = AssetToolsModule.Get().CreateAssetWithDialog(DefaultName, DefaultPath, UVirtualTextureBuilder::StaticClass(), Factory);
		CreatedTexture = Cast<UVirtualTextureBuilder>(Object);
	}

	// Build the texture contents
	bool bOK = false;
	if (RuntimeVirtualTextureComponent->GetStreamingTexture() != nullptr || CreatedTexture != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("Transaction_BuildDebugStreamingTexture", "Build Streaming Texture"));

		if (CreatedTexture != nullptr)
		{
			RuntimeVirtualTextureComponent->Modify();
			RuntimeVirtualTextureComponent->SetStreamingTexture(CreatedTexture);
		}

		RuntimeVirtualTextureComponent->GetStreamingTexture()->Modify();

		const FLinearColor FixedColor = RuntimeVirtualTextureComponent->GetStreamingMipsFixedColor();
		if (RuntimeVirtualTexture::BuildStreamedMips(RuntimeVirtualTextureComponent, FixedColor))
		{
			bOK = true;
		}
	}

	return bOK ? FReply::Handled() : FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
