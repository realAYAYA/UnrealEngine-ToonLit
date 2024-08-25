// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handling/AvaMaskParametricMaterialHandle.h"

#include "AvaMaskUtilities.h"
#include "AvaShapeParametricMaterial.h"
#include "Engine/Texture.h"
#include "Handling/AvaParametricMaterialHandle.h"
#include "Materials/MaterialInstanceDynamic.h"

FAvaMaskParametricMaterialHandle::FAvaMaskParametricMaterialHandle(const FStructView& InParametricMaterial)
	: FAvaParametricMaterialHandle(InParametricMaterial)
{
	if (FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
	{
		if (UMaterialInstanceDynamic* MID = ParametricMtl->GetMaterial())
		{
			LastShadingModel = MID->GetShadingModels();
			LastBlendMode = MID->GetBlendMode();
			LastMaterialInstance = MID;
		}

		if (!ParametricMtl->OnMaterialParameterChanged().IsBoundToObject(this))
		{
			MaterialChangedHandle = ParametricMtl->OnMaterialParameterChanged().AddRaw(this, &FAvaMaskParametricMaterialHandle::OnMaterialChanged);
		}
	}
}

FAvaMaskParametricMaterialHandle::~FAvaMaskParametricMaterialHandle()
{
	// @note: if this doesn't cover all situations, the handle needs to be aware of the struct's outer to check validity
	if (!GIsGarbageCollecting)
	{
		if (FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
		{
			ParametricMtl->OnMaterialParameterChanged().RemoveAll(this);
		}
	}
}

bool FAvaMaskParametricMaterialHandle::GetMaskParameters(FAvaMask2DMaterialParameters& OutParameters)
{
	if (const UMaterialInstanceDynamic* MaterialInstance = WeakMaterialInstance.Get())
	{
		OutParameters.SetFromMaterial(
			MaterialInstance
			, UE::AvaMask::Internal::TextureParameterInfo
			, UE::AvaMask::Internal::ChannelParameterInfo
			, UE::AvaMask::Internal::InvertParameterInfo
			, UE::AvaMask::Internal::BaseOpacityParameterInfo
			, UE::AvaMask::Internal::PaddingParameterInfo
			, UE::AvaMask::Internal::FeatherParameterInfo);
		return true;
	}

	return false;
}

bool FAvaMaskParametricMaterialHandle::SetMaskParameters(
	UTexture* InTexture
	, float InBaseOpacity
	, EGeometryMaskColorChannel InChannel
	, bool bInInverted
	, const FVector2f& InPadding
	, bool bInApplyFeathering
	, float InOuterFeathering
	, float InInnerFeathering)
{
	if (UMaterialInstanceDynamic* MaterialInstance = GetMaterialInstance())
	{
		FLinearColor PaddingValueV = FLinearColor(FMath::Max(0, InPadding.X), FMath::Max(0, InPadding.Y), 0, 0);
		FLinearColor FeatherValueV = FLinearColor(bInApplyFeathering ? 1.0f : 0.0f, FMath::Max(0, InOuterFeathering), FMath::Max(0, InInnerFeathering), FMath::Max(0, FMath::Max(InOuterFeathering, InInnerFeathering)));
		
		UE_LOG(LogAvaMask, VeryVerbose, TEXT("SetParameters:\nTexture:%s\nChannel:%s\nFeather:%s\n"),
			InTexture ? *InTexture->GetName() : TEXT("(None)"),
			*UE::AvaMask::Internal::MaskChannelEnumToVector[InChannel].ToString(),
			*FeatherValueV.ToString());

		UTexture* DefaultTexture = nullptr;
		MaterialInstance->GetTextureParameterDefaultValue(UE::AvaMask::Internal::TextureParameterInfo, DefaultTexture);

		InTexture = InTexture ? InTexture : DefaultTexture;

		MaterialInstance->SetTextureParameterValueByInfo(UE::AvaMask::Internal::TextureParameterInfo, InTexture);
		MaterialInstance->SetVectorParameterValueByInfo(UE::AvaMask::Internal::ChannelParameterInfo, UE::AvaMask::Internal::MaskChannelEnumToVector[InChannel]);
		MaterialInstance->SetScalarParameterValueByInfo(UE::AvaMask::Internal::InvertParameterInfo, bInInverted ? 1.0f : 0.0f);
		MaterialInstance->SetScalarParameterValueByInfo(UE::AvaMask::Internal::BaseOpacityParameterInfo, FMath::Clamp(InBaseOpacity, 0.0f, 1.0f));
		MaterialInstance->SetVectorParameterValueByInfo(UE::AvaMask::Internal::PaddingParameterInfo, PaddingValueV);
		MaterialInstance->SetVectorParameterValueByInfo(UE::AvaMask::Internal::FeatherParameterInfo, FeatherValueV);

		return true;
	}

	return false;
}

const EBlendMode FAvaMaskParametricMaterialHandle::GetBlendMode()
{
	if (const FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
	{
		return ParametricMtl->GetUseTranslucentMaterial() ? EBlendMode::BLEND_Translucent : EBlendMode::BLEND_Opaque;
	}
	
	if (const UMaterialInterface* MaterialInstance = GetMaterialInstance())
	{
		return MaterialInstance->GetBlendMode();
	}
	
	if (const UMaterialInterface* Material = GetMaterial())
	{
		return Material->GetBlendMode();
	}

	return EBlendMode::BLEND_Opaque;
}

void FAvaMaskParametricMaterialHandle::SetBlendMode(const EBlendMode InBlendMode)
{
	const EBlendMode TargetBlendMode = UE::AvaMask::Internal::GetTargetBlendMode( GetBlendMode(), InBlendMode);
	if (FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
	{
		ParametricMtl->SetUseAutoTranslucency(false); // Disable so that user changes won't override the mask
		ParametricMtl->SetUseTranslucentMaterial(TargetBlendMode != EBlendMode::BLEND_Opaque);
		return;
	}
	
	if (UMaterialInstanceDynamic* MaterialInstance = GetMaterialInstance())
	{
		MaterialInstance->BlendMode = TargetBlendMode;
		MaterialInstance->BasePropertyOverrides.BlendMode = TargetBlendMode;
	}
}

void FAvaMaskParametricMaterialHandle::SetChildMaterial(UMaterialInstanceDynamic* InChildMaterial)
{
	ChildMaterial = InChildMaterial;
}

bool FAvaMaskParametricMaterialHandle::SaveOriginalState(const FStructView& InHandleData)
{
	// @note: this assumes the current material is the original
	if (const FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
	{
		ensureAlways(IsValid());
		
		if (FHandleData* MaterialHandleData = InHandleData.GetPtr<FHandleData>())
		{
			MaterialHandleData->OriginalBlendMode = GetBlendMode();

			const UMaterialInterface* Material = ParametricMtl->GetMaterial();

			// Can happen when Parametric material not yet initialized - but initialization is accounted for elsewhere in the Handle
			if (!Material)
			{
				Material = ParametricMtl->GetDefaultMaterial();
			}

			MaterialHandleData->OriginalMaskMaterialParameters.SetFromMaterial(
				Material
				, UE::AvaMask::Internal::TextureParameterInfo
				, UE::AvaMask::Internal::ChannelParameterInfo
				, UE::AvaMask::Internal::InvertParameterInfo
				, UE::AvaMask::Internal::BaseOpacityParameterInfo
				, UE::AvaMask::Internal::PaddingParameterInfo
				, UE::AvaMask::Internal::FeatherParameterInfo);

			MaterialHandleData->OriginalMaterialStyle = ParametricMtl->GetStyle();
			MaterialHandleData->OriginalBlendMode = GetBlendMode();

			return true;
		}
	}
	
	return false;
}

bool FAvaMaskParametricMaterialHandle::ApplyOriginalState(
	const FStructView& InHandleData
	, TUniqueFunction<void(UMaterialInterface*)>&& InMaterialSetter)
{
	if (FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
	{
		if (const FHandleData* MaterialHandleData = InHandleData.GetPtr<const FHandleData>())
		{
			SetBlendMode(MaterialHandleData->OriginalBlendMode);
			
			SetMaskParameters(
				MaterialHandleData->OriginalMaskMaterialParameters.Texture
				, MaterialHandleData->OriginalMaskMaterialParameters.BaseOpacity
				, MaterialHandleData->OriginalMaskMaterialParameters.Channel
				, MaterialHandleData->OriginalMaskMaterialParameters.bInvert
				, MaterialHandleData->OriginalMaskMaterialParameters.Padding
				, MaterialHandleData->OriginalMaskMaterialParameters.bApplyFeathering
				, MaterialHandleData->OriginalMaskMaterialParameters.OuterFeatherRadius
				, MaterialHandleData->OriginalMaskMaterialParameters.InnerFeatherRadius);

			ParametricMtl->SetStyle(MaterialHandleData->OriginalMaterialStyle);
			ParametricMtl->SetUseAutoTranslucency(true);
			ParametricMtl->SetUseTranslucentMaterial(MaterialHandleData->OriginalBlendMode == EBlendMode::BLEND_Translucent);

			InMaterialSetter(ParametricMtl->GetMaterial());

			return true;
		}
	}

	return false;
}

bool FAvaMaskParametricMaterialHandle::ApplyModifiedState(
	const FAvaMask2DSubjectParameters& InModifiedParameters
	, const FStructView& InHandleData
	, TUniqueFunction<void(UMaterialInterface*)>&& InMaterialSetter)
{
	if (FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
	{
		if (InHandleData.GetPtr<const FHandleData>())
		{
			SetBlendMode(InModifiedParameters.MaterialParameters.BlendMode);

			SetMaskParameters(
				InModifiedParameters.MaterialParameters.Texture
				, InModifiedParameters.MaterialParameters.BaseOpacity
				, InModifiedParameters.MaterialParameters.Channel
				, InModifiedParameters.MaterialParameters.bInvert
				, InModifiedParameters.MaterialParameters.Padding
				, InModifiedParameters.MaterialParameters.bApplyFeathering
				, InModifiedParameters.MaterialParameters.OuterFeatherRadius
				, InModifiedParameters.MaterialParameters.InnerFeatherRadius);

			InMaterialSetter(ParametricMtl->GetMaterial());

			return true;
		}
	}

	return false;
}

bool FAvaMaskParametricMaterialHandle::IsValid() const
{
	return FAvaParametricMaterialHandle::IsValid();
}

bool FAvaMaskParametricMaterialHandle::IsSupported(
	const UStruct* InStruct
	, const TVariant<UObject*, FStructView>& InInstance
	, FName InTag)
{
	return InTag == UE::AvaMask::Internal::HandleTag
		&& FAvaParametricMaterialHandle::IsSupported(InStruct, InInstance, InTag);
}

UMaterialInstanceDynamic* FAvaMaskParametricMaterialHandle::GetMaterialInstance()
{
	return FAvaParametricMaterialHandle::GetMaterialInstance();
}

void FAvaMaskParametricMaterialHandle::OnMaterialChanged()
{
	if (const FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
	{
		if (UMaterialInstanceDynamic* MaterialInstance = ParametricMtl->GetMaterial())
		{
			const FMaterialShadingModelField ShadingModel = MaterialInstance->GetShadingModels();
			const EBlendMode BlendMode = MaterialInstance->GetBlendMode();

			if (UMaterialInstance* LastMaterial = LastMaterialInstance.Get())
			{
				MaterialInstance->CopyInterpParameters(LastMaterial);	
			}

			LastShadingModel = ShadingModel;
			LastBlendMode = BlendMode;
			LastMaterialInstance = MaterialInstance;
		}

		if (UMaterialInstanceDynamic* ChildMtl = ChildMaterial.Get())
		{
			// Write the changed params to the Child Material
			ParametricMtl->SetMaterialParameterValues(ChildMtl);
		}
	}
}
