// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/CEClonerHoneycombLayout.h"

#include "Cloner/CEClonerComponent.h"
#include "NiagaraSystem.h"

void UCEClonerHoneycombLayout::SetPlane(ECEClonerPlane InPlane)
{
	if (Plane == InPlane)
	{
		return;
	}

	Plane = InPlane;
	UpdateLayoutParameters();
}

void UCEClonerHoneycombLayout::SetWidthCount(int32 InWidthCount)
{
	if (WidthCount == InWidthCount)
	{
		return;
	}

	WidthCount = InWidthCount;
	UpdateLayoutParameters();
}

void UCEClonerHoneycombLayout::SetHeightCount(int32 InHeightCount)
{
	if (HeightCount == InHeightCount)
	{
		return;
	}

	HeightCount = InHeightCount;
	UpdateLayoutParameters();
}

void UCEClonerHoneycombLayout::SetWidthOffset(float InWidthOffset)
{
	if (WidthOffset == InWidthOffset)
	{
		return;
	}

	WidthOffset = InWidthOffset;
	UpdateLayoutParameters();
}

void UCEClonerHoneycombLayout::SetHeightOffset(float InHeightOffset)
{
	if (HeightOffset == InHeightOffset)
	{
		return;
	}

	HeightOffset = InHeightOffset;
	UpdateLayoutParameters();
}

void UCEClonerHoneycombLayout::SetHeightSpacing(float InHeightSpacing)
{
	if (HeightSpacing == InHeightSpacing)
	{
		return;
	}

	HeightSpacing = InHeightSpacing;
	UpdateLayoutParameters();
}

void UCEClonerHoneycombLayout::SetWidthSpacing(float InWidthSpacing)
{
	if (WidthSpacing == InWidthSpacing)
	{
		return;
	}

	WidthSpacing = InWidthSpacing;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
const TCEPropertyChangeDispatcher<UCEClonerHoneycombLayout> UCEClonerHoneycombLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, Plane), &UCEClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, WidthCount), &UCEClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, HeightCount), &UCEClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, WidthOffset), &UCEClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, HeightOffset), &UCEClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, WidthSpacing), &UCEClonerHoneycombLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerHoneycombLayout, HeightSpacing), &UCEClonerHoneycombLayout::OnLayoutPropertyChanged },
};

void UCEClonerHoneycombLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UCEClonerHoneycombLayout::OnLayoutParametersChanged(UCEClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("HoneycombWidthCount"), WidthCount);

	InComponent->SetIntParameter(TEXT("HoneycombHeightCount"), HeightCount);

	InComponent->SetFloatParameter(TEXT("HoneycombWidthOffset"), WidthOffset);

	InComponent->SetFloatParameter(TEXT("HoneycombHeightOffset"), HeightOffset);

	InComponent->SetFloatParameter(TEXT("HoneycombWidthSpacing"), WidthSpacing);

	InComponent->SetFloatParameter(TEXT("HoneycombHeightSpacing"), HeightSpacing);

	FNiagaraUserRedirectionParameterStore& ExposedParameters = InComponent->GetAsset()->GetExposedParameters();
	static const FNiagaraVariable HoneycombPlaneVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerPlane>()), TEXT("HoneycombPlane"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(Plane), HoneycombPlaneVar);
}
