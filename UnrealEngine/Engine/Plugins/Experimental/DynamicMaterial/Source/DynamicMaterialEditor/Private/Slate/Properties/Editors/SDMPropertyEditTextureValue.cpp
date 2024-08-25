// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "SDMPropertyEditTextureValue.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "DMPrivate.h"
#include "Engine/Texture.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditTextureValue"

TSharedPtr<SWidget> SDMPropertyEditTextureValue::CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InTextureValue)
{
	return SNew(SDMPropertyEditTextureValue, Cast<UDMMaterialValueTexture>(InTextureValue))
		.ComponentEditWidget(InComponentEditWidget);
}
 
void SDMPropertyEditTextureValue::Construct(const FArguments& InArgs, UDMMaterialValueTexture* InTextureValue)
{
	ensure(IsValid(InTextureValue));

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyMaterialValue(InTextureValue)
	);
}
 
UDMMaterialValueTexture* SDMPropertyEditTextureValue::GetTextureValue() const
{
	return Cast<UDMMaterialValueTexture>(GetValue());
}
 
FString SDMPropertyEditTextureValue::GetTexturePath() const
{
	UDMMaterialValueTexture* TextureValue = GetTextureValue();

	if (!IsValid(TextureValue) || !TextureValue->IsComponentValid())
	{
		return "";
	}
 
	const UTexture* Texture = TextureValue->GetValue();
 
	if (Texture)
	{
		return Texture->GetPathName();
	}
 
	return "";
}
 
TSharedRef<SWidget> SDMPropertyEditTextureValue::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);
 
	UDMMaterialValueTexture* TextureValue = GetTextureValue();

	if (!IsValid(TextureValue) || !TextureValue->IsComponentValid())
	{
		return SNullWidget::NullWidget;
	}
 
	return CreateAssetPicker(
		UTexture::StaticClass(),
		TAttribute<FString>::CreateSP(this, &SDMPropertyEditTextureValue::GetTexturePath),
		FOnSetObject::CreateSP(this, &SDMPropertyEditTextureValue::OnTextureSelected)
	);
}
 
void SDMPropertyEditTextureValue::OnTextureSelected(const FAssetData& InAssetData)
{
	UDMMaterialValueTexture* TextureValue = GetTextureValue();

	if (IsValid(TextureValue) && TextureValue->IsComponentValid())
	{
		UObject* NewValue = InAssetData.GetAsset();
		UTexture* NewTexture = Cast<UTexture>(NewValue);

		// Not a UTexture
		if (NewValue && !NewTexture)
		{
			return;
		}

		if (TextureValue->GetValue() != NewTexture)
		{
			StartTransaction(LOCTEXT("SetValue", "Material Designer Value Set (Texture)"));
			TextureValue->SetValue(NewTexture);
			EndTransaction();
		}
	}
}

#undef LOCTEXT_NAMESPACE
