// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/CEClonerGridLayout.h"

#include "Cloner/CEClonerComponent.h"
#include "NiagaraDataInterfaceTexture.h"
#include "NiagaraSystem.h"

void UCEClonerGridLayout::SetCountX(int32 InCountX)
{
	if (CountX == InCountX)
	{
		return;
	}

	CountX = InCountX;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetCountY(int32 InCountY)
{
	if (CountY == InCountY)
	{
		return;
	}

	CountY = InCountY;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetCountZ(int32 InCountZ)
{
	if (CountZ == InCountZ)
	{
		return;
	}

	CountZ = InCountZ;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetSpacingX(float InSpacingX)
{
	if (SpacingX == InSpacingX)
	{
		return;
	}

	SpacingX = InSpacingX;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetSpacingY(float InSpacingY)
{
	if (SpacingY == InSpacingY)
	{
		return;
	}

	SpacingY = InSpacingY;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetSpacingZ(float InSpacingZ)
{
	if (SpacingZ == InSpacingZ)
	{
		return;
	}

	SpacingZ = InSpacingZ;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetConstraint(ECEClonerGridConstraint InConstraint)
{
	if (Constraint == InConstraint)
	{
		return;
	}

	Constraint = InConstraint;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetInvertConstraint(bool bInInvertConstraint)
{
	if (bInvertConstraint == bInInvertConstraint)
	{
		return;
	}

	bInvertConstraint = bInInvertConstraint;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetSphereConstraint(const FCEClonerGridConstraintSphere& InConstraint)
{
	SphereConstraint = InConstraint;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetCylinderConstraint(const FCEClonerGridConstraintCylinder& InConstraint)
{
	CylinderConstraint = InConstraint;
	UpdateLayoutParameters();
}

void UCEClonerGridLayout::SetTextureConstraint(const FCEClonerGridConstraintTexture& InConstraint)
{
	TextureConstraint = InConstraint;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
const TCEPropertyChangeDispatcher<UCEClonerGridLayout> UCEClonerGridLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, CountX), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, CountY), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, CountZ), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingX), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingY), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SpacingZ), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, Constraint), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, bInvertConstraint), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, SphereConstraint), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, CylinderConstraint), &UCEClonerGridLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerGridLayout, TextureConstraint), &UCEClonerGridLayout::OnLayoutPropertyChanged },
};

void UCEClonerGridLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UCEClonerGridLayout::OnLayoutParametersChanged(UCEClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("GridCountX"), CountX);

	InComponent->SetIntParameter(TEXT("GridCountY"), CountY);

	InComponent->SetIntParameter(TEXT("GridCountZ"), CountZ);

	InComponent->SetVectorParameter(TEXT("GridSpacing"), FVector(SpacingX, SpacingY, SpacingZ));

	FNiagaraUserRedirectionParameterStore& ExposedParameters = InComponent->GetAsset()->GetExposedParameters();
	static const FNiagaraVariable ConstraintVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerGridConstraint>()), TEXT("Constraint"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(Constraint), ConstraintVar);

	InComponent->SetBoolParameter(TEXT("ConstraintInvert"), Constraint != ECEClonerGridConstraint::None ? bInvertConstraint : false);

	InComponent->SetVectorParameter(TEXT("ConstraintCylinderCenter"), CylinderConstraint.Center);

	InComponent->SetFloatParameter(TEXT("ConstraintCylinderHeight"), CylinderConstraint.Height);

	InComponent->SetFloatParameter(TEXT("ConstraintCylinderRadius"), CylinderConstraint.Radius);

	InComponent->SetVectorParameter(TEXT("ConstraintSphereCenter"), SphereConstraint.Center);

	InComponent->SetFloatParameter(TEXT("ConstraintSphereRadius"), SphereConstraint.Radius);

	static const FNiagaraVariable ConstraintTextureChannelVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerTextureSampleChannel>()), TEXT("ConstraintTextureChannel"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(TextureConstraint.Channel), ConstraintTextureChannelVar);

	static const FNiagaraVariable ConstraintTextureCompareModeVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerCompareMode>()), TEXT("ConstraintTextureCompareMode"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(TextureConstraint.CompareMode), ConstraintTextureCompareModeVar);

	static const FNiagaraVariable ConstraintTexturePlaneVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerPlane>()), TEXT("ConstraintTexturePlane"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(TextureConstraint.Plane), ConstraintTexturePlaneVar);

	InComponent->SetFloatParameter(TEXT("ConstraintTextureThreshold"), TextureConstraint.Threshold);

	static const FNiagaraVariable ConstraintTextureSamplerVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceTexture::StaticClass()), TEXT("ConstraintTextureSampler"));
	UNiagaraDataInterfaceTexture* TextureSamplerDI = Cast<UNiagaraDataInterfaceTexture>(ExposedParameters.GetDataInterface(ConstraintTextureSamplerVar));
	TextureSamplerDI->SetTexture(TextureConstraint.Texture.Get());
}
