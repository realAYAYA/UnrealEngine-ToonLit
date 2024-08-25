// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTextActor.h"
#include "AvaDefs.h"
#include "AvaTextCharacterTransform.h"

AAvaTextActor::AAvaTextActor()
{
	Text3DComponent = CreateDefaultSubobject<UAvaText3DComponent>(TEXT("Text3DComponent"));
	RootComponent = Text3DComponent;
	
	Text3DCharacterTransform = CreateDefaultSubobject<UAvaTextCharacterTransform>(TEXT("Text3DCharacterTransform"));
	Text3DCharacterTransform->SetupAttachment(RootComponent);
}

FAvaColorChangeData AAvaTextActor::GetColorData() const
{
	FAvaColorChangeData ColorData;
	ColorData.ColorStyle = EAvaColorStyle::None;

	if (const UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(Text3DComponent))
	{
		switch (AvaTextComponent->GetColoringStyle())
		{
		case EAvaTextColoringStyle::Solid:
			ColorData.ColorStyle = EAvaColorStyle::Solid;
			ColorData.PrimaryColor = AvaTextComponent->GetColor();
			ColorData.SecondaryColor = AvaTextComponent->GetColor();
			break;

		case EAvaTextColoringStyle::Gradient:
			ColorData.ColorStyle = EAvaColorStyle::LinearGradient;
			ColorData.PrimaryColor = AvaTextComponent->GetGradientSettings().ColorA;
			ColorData.SecondaryColor = AvaTextComponent->GetGradientSettings().ColorB;
			break;

		default:
			break;
		}
		
		ColorData.bIsUnlit = AvaTextComponent->GetIsUnlit();
	}

	return ColorData;
}

void AAvaTextActor::SetColorData(const FAvaColorChangeData& NewColorData)
{
	if (UAvaText3DComponent* const AvaTextComponent = Cast<UAvaText3DComponent>(Text3DComponent))
	{
		switch (NewColorData.ColorStyle)
		{
			case EAvaColorStyle::Solid:
				AvaTextComponent->SetColoringStyle(EAvaTextColoringStyle::Solid);
				AvaTextComponent->SetColor(NewColorData.PrimaryColor);
				break;

			case EAvaColorStyle::LinearGradient:
				AvaTextComponent->SetColoringStyle(EAvaTextColoringStyle::Gradient);
				AvaTextComponent->SetGradientColors(NewColorData.PrimaryColor, NewColorData.SecondaryColor);
				break;

			default:
				break;
		}

		AvaTextComponent->SetIsUnlit(NewColorData.bIsUnlit);
	}
}
