// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "DMDefs.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Model/IDMMaterialBuildStateInterface.h"
#include "Model/IDMMaterialBuildUtilsInterface.h"
#include "RenderUtils.h"
#endif

#if WITH_EDITOR
namespace UE::DynamicMaterial::Private
{
	bool HasAlpha(UTexture* InTexture)
	{
		if (!IsValid(InTexture))
		{
			return false;
		}

		if (InTexture->CompressionNoAlpha)
		{
			return false;
		}

		EPixelFormatChannelFlags ValidTextureChannels = EPixelFormatChannelFlags::None;

		if (UTexture2D* Texture2D = Cast<UTexture2D>(InTexture))
		{
			ValidTextureChannels = GetPixelFormatValidChannels(Texture2D->GetPixelFormat());
		}
		else if (UTextureCube* TextureCube = Cast<UTextureCube>(InTexture))
		{
			ValidTextureChannels = GetPixelFormatValidChannels(TextureCube->GetPixelFormat());
		}
		else if (UVolumeTexture* VolumeTexture = Cast<UVolumeTexture>(InTexture))
		{
			ValidTextureChannels = GetPixelFormatValidChannels(VolumeTexture->GetPixelFormat());
		}
		else
		{
			return false;
		}

		return EnumHasAnyFlags(ValidTextureChannels, EPixelFormatChannelFlags::A);
	}
}

FDMGetDefaultRGBTexture UDMMaterialValueTexture::GetDefaultRGBTexture;
#endif

UDMMaterialValueTexture::UDMMaterialValueTexture()
	: UDMMaterialValue(EDMValueType::VT_Texture)
#if WITH_EDITORONLY_DATA
	, OldValue(nullptr)
#endif
{
#if WITH_EDITOR
	ResetDefaultValue();
	Value = DefaultValue;
#else
	Value = nullptr;
#endif
}

#if WITH_EDITOR
void UDMMaterialValueTexture::GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const
{
	if (!IsComponentValid())
	{
		return;
	}

	if (InBuildState->HasValue(this))
	{
		return;
	}

	UMaterialExpressionTextureObjectParameter* NewExpression = InBuildState->GetBuildUtils().CreateExpressionParameter<UMaterialExpressionTextureObjectParameter>(GetMaterialParameterName(), UE_DM_NodeComment_Default, Value);
	check(NewExpression);

	InBuildState->AddValueExpressions(this, {NewExpression});
}
 
UDMMaterialValueTexture* UDMMaterialValueTexture::CreateMaterialValueTexture(UObject* Outer, UTexture* InTexture)
{
	check(InTexture);
 
	UDMMaterialValueTexture* TextureValue = NewObject<UDMMaterialValueTexture>(Outer, NAME_None, RF_Transactional);
	TextureValue->SetValue(InTexture);
	return TextureValue;
}
 
void UDMMaterialValueTexture::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
 
	if (!IsComponentValid())
	{
		return;
	}

	if (PropertyAboutToChange->GetFName() == ValueName)
	{
		OldValue = GetValue();
	}
}

bool UDMMaterialValueTexture::IsDefaultValue() const
{
	return Value == DefaultValue;
}

void UDMMaterialValueTexture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Skip parent class because we need to do extra logic.
	Super::Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!IsComponentValid())
	{
		return;
	}

	FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();

	if (MemberPropertyName == NAME_None)
	{
		return;
	}

	for (const FName& EditableProperty : EditableProperties)
	{
		if (EditableProperty == MemberPropertyName)
		{
			if (EditableProperty == ValueName)
			{
				const TextureCompressionSettings CurrentCompression = OldValue ? OldValue->CompressionSettings.GetValue() : TextureCompressionSettings::TC_MAX;
				const TextureCompressionSettings NewCompression = Value ? Value->CompressionSettings.GetValue() : TextureCompressionSettings::TC_MAX;

				OnValueUpdated(CurrentCompression != NewCompression);
				return;
			}

			OnValueUpdated(/* bForceStructureUpdate */ true);
			return;
		}
	}
}
#endif

void UDMMaterialValueTexture::PostLoad()
{
	Super::PostLoad();

	Type = EDMValueType::VT_Texture;
}

#if WITH_EDITOR
void UDMMaterialValueTexture::ApplyDefaultValue()
{
	SetValue(DefaultValue);
}

void UDMMaterialValueTexture::ResetDefaultValue()
{
	DefaultValue = nullptr;

	if (GetDefaultRGBTexture.IsBound())
	{
		DefaultValue = GetDefaultRGBTexture.Execute();
	}
}

void UDMMaterialValueTexture::SetDefaultValue(UTexture* InDefaultValue)
{
	DefaultValue = InDefaultValue;
}

bool UDMMaterialValueTexture::HasAlpha() const
{
	return UE::DynamicMaterial::Private::HasAlpha(Value);
}
#endif

void UDMMaterialValueTexture::SetValue(UTexture* InValue)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (Value == InValue)
	{
		return;
	}

	const TextureCompressionSettings CurrentCompression = Value ? Value->CompressionSettings.GetValue() : TextureCompressionSettings::TC_MAX;
	const TextureCompressionSettings NewCompression = InValue ? InValue->CompressionSettings.GetValue() : TextureCompressionSettings::TC_MAX;

	Value = InValue;

	OnValueUpdated(CurrentCompression != NewCompression);
}

void UDMMaterialValueTexture::SetMIDParameter(UMaterialInstanceDynamic* InMID) const
{
	if (!IsComponentValid())
	{
		return;
	}

	check(InMID);

	InMID->SetTextureParameterValue(GetMaterialParameterName(), Value);
}
