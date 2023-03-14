// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"

#include "DMXConversions.h"
#include "DMXPixelMappingRuntimeUtils.h"
#include "DMXPixelMappingTypes.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Components/DMXPixelMappingRootComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "IO/DMXOutputPort.h"
#include "Modulators/DMXModulator.h"

#if WITH_EDITOR
#include "DMXPixelMappingComponentWidget.h"
#endif // WITH_EDITOR

#include "Engine/Texture.h"


DECLARE_CYCLE_STAT(TEXT("Send Fixture Group Item"), STAT_DMXPixelMaping_FixtureGroupItem, STATGROUP_DMXPIXELMAPPING);

#define LOCTEXT_NAMESPACE "DMXPixelMappingFixtureGroupItemComponent"

UDMXPixelMappingFixtureGroupItemComponent::UDMXPixelMappingFixtureGroupItemComponent()
	: DownsamplePixelIndex(0)
{
	SetSize(FVector2D(32.f, 32.f));

	ColorMode = EDMXColorMode::CM_RGB;
	AttributeRExpose = AttributeGExpose = AttributeBExpose = true;
	bMonochromeExpose = true;

	AttributeR.SetFromName("Red");
	AttributeG.SetFromName("Green");
	AttributeB.SetFromName("Blue");

#if WITH_EDITOR
	ZOrder = 2;
#endif // WITH_EDITOR
}

void UDMXPixelMappingFixtureGroupItemComponent::PostLoad()
{
	Super::PostLoad();

	// Add valid modulators to modulator classes, remove invalid modulators
	for (int32 IndexModulator = 0; Modulators.IsValidIndex(IndexModulator); )
	{
		if (Modulators[IndexModulator])
		{
			ModulatorClasses.Add(Modulators[IndexModulator]->GetClass());
			IndexModulator++;
		}
		else
		{
			Modulators.RemoveAt(IndexModulator);
			if (!Modulators.IsValidIndex(IndexModulator++))
			{
				// Removed the last element
				break;
			}
		}
	}
}

#if WITH_EDITOR
void UDMXPixelMappingFixtureGroupItemComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = PropertyChangedEvent.GetPropertyName();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (PropertyChangedEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetPositionXPropertyName() ||
		PropertyChangedEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetPositionYPropertyName())
	{
		if (ComponentWidget_DEPRECATED.IsValid())
		{
			ComponentWidget_DEPRECATED->SetPosition(GetPosition());
		}
	}
	if (PropertyChangedEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetSizeXPropertyName() ||
		PropertyChangedEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetSizeYPropertyName())
	{
		if (ComponentWidget_DEPRECATED.IsValid())
		{
			ComponentWidget_DEPRECATED->SetSize(GetSize());
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXPixelMappingFixtureGroupItemComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedChainEvent);
	
	// For consistency with Matrix, handling modulator class changes in runtime utils
	FDMXPixelMappingRuntimeUtils::HandleModulatorPropertyChange(this, PropertyChangedChainEvent, ModulatorClasses, Modulators);
}
#endif // WITH_EDITOR

FString UDMXPixelMappingFixtureGroupItemComponent::GetUserFriendlyName() const
{
	if (UDMXEntityFixturePatch* Patch = FixturePatchRef.GetFixturePatch())
	{
		return Patch->GetDisplayName();
	}

	return FString(TEXT("Fixture Group Item: No Fixture Patch"));
}

const FName& UDMXPixelMappingFixtureGroupItemComponent::GetNamePrefix()
{
	static FName NamePrefix = TEXT("Fixture Item");
	return NamePrefix;
}

#if WITH_EDITOR
bool UDMXPixelMappingFixtureGroupItemComponent::IsVisible() const
{
	if (UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(GetParent()))
	{
		if (!FixtureGroupComponent->IsVisible())
		{
			return false;
		}
	}

	return Super::IsVisible();
}
#endif // WITH_EDITOR

void UDMXPixelMappingFixtureGroupItemComponent::ResetDMX()
{
	UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();
	if (!ensure(RendererComponent))
	{
		return;
	}

	RendererComponent->ResetColorDownsampleBufferPixel(DownsamplePixelIndex);

	SendDMX();
}

void UDMXPixelMappingFixtureGroupItemComponent::SendDMX()
{
	SCOPE_CYCLE_COUNTER(STAT_DMXPixelMaping_FixtureGroupItem);

	UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.GetFixturePatch();

	if(FixturePatch)
	{
		UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();
		if (RendererComponent)
		{
			TMap<FDMXAttributeName, float> AttributeToValueMap = CreateAttributeValues();
			
			// No need to apply matrix modulators
			for (UDMXModulator* Modulator : Modulators)
			{
				Modulator->Modulate(FixturePatch, AttributeToValueMap, AttributeToValueMap);
			}
			
			TMap<int32, uint8> ChannelToValueMap;
			for (const TTuple<FDMXAttributeName, float>& AttributeValuePair : AttributeToValueMap)
			{
				FDMXPixelMappingRuntimeUtils::ConvertNormalizedAttributeValueToChannelValue(FixturePatch, AttributeValuePair.Key, AttributeValuePair.Value, ChannelToValueMap);
			}

			// Send DMX
			if (UDMXLibrary* Library = FixturePatch->GetParentLibrary())
			{
				for (const FDMXOutputPortSharedRef& OutputPort : Library->GetOutputPorts())
				{
					OutputPort->SendDMX(FixturePatch->GetUniverseID(), ChannelToValueMap);
				}
			}
		}
	}
}

void UDMXPixelMappingFixtureGroupItemComponent::QueueDownsample()
{
	// Queue pixels into the downsample rendering
	UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();

	if (!ensure(RendererComponent))
	{
		return;
	}

	UTexture* InputTexture = RendererComponent->GetRendererInputTexture();
	if (!ensure(InputTexture))
	{
		return;
	}

	// Store pixel position
	DownsamplePixelIndex = RendererComponent->GetDownsamplePixelNum();
	
	const uint32 TextureSizeX = InputTexture->GetResource()->GetSizeX();
	const uint32 TextureSizeY = InputTexture->GetResource()->GetSizeY();
	check(TextureSizeX > 0 && TextureSizeY > 0);
	const FIntPoint PixelPosition = RendererComponent->GetPixelPosition(DownsamplePixelIndex);
	const FVector2D UV = FVector2D(GetPosition().X / TextureSizeX, GetPosition().Y / TextureSizeY);
	const FVector2D UVSize(GetSize().X / TextureSizeX, GetSize().Y / TextureSizeY);
	const FVector2D UVCellSize = UVSize / 2.f;
	constexpr bool bStaticCalculateUV = true;

	FVector4 ExposeFactor{};
	FIntVector4 InvertFactor{};
	if (ColorMode == EDMXColorMode::CM_RGB)
	{
		ExposeFactor = FVector4(AttributeRExpose ? 1.f : 0.f, AttributeGExpose ? 1.f : 0.f, AttributeBExpose ? 1.f : 0.f, 1.f);
		InvertFactor = FIntVector4(AttributeRInvert, AttributeGInvert, AttributeBInvert, 0);
	}
	else if (ColorMode == EDMXColorMode::CM_Monochrome)
	{
		static const FVector4 Expose(1.f, 1.f, 1.f, 1.f);
		static const FVector4 NoExpose(0.f, 0.f, 0.f, 0.f);
		ExposeFactor = FVector4(bMonochromeExpose ? Expose : NoExpose);
		InvertFactor = FIntVector4(bMonochromeInvert, bMonochromeInvert, bMonochromeInvert, 0);
	}

	FDMXPixelMappingDownsamplePixelParam DownsamplePixelParam
	{
		ExposeFactor,
		InvertFactor,
		PixelPosition,
		UV,
		UVSize,
		UVCellSize,
		CellBlendingQuality,
		bStaticCalculateUV
	};

	RendererComponent->AddPixelToDownsampleSet(MoveTemp(DownsamplePixelParam));
}

void UDMXPixelMappingFixtureGroupItemComponent::SetPosition(const FVector2D& NewPosition)
{
	Super::SetPosition(NewPosition);

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetPosition(GetPosition());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

void UDMXPixelMappingFixtureGroupItemComponent::SetSize(const FVector2D& NewSize)
{
	Super::SetSize(NewSize);

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetSize(GetSize());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

bool UDMXPixelMappingFixtureGroupItemComponent::IsOverParent() const
{
	// Needs be over the over the group
	if (UDMXPixelMappingFixtureGroupComponent* ParentFixtureGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(GetParent()))
	{
		const float Left = GetPosition().X;
		const float Top = GetPosition().Y;
		const float Right = GetPosition().X + GetSize().X;
		const float Bottom = GetPosition().Y + GetSize().Y;

		const float ParentLeft = ParentFixtureGroupComponent->GetPosition().X;
		const float ParentTop = ParentFixtureGroupComponent->GetPosition().Y;
		const float ParentRight = ParentFixtureGroupComponent->GetPosition().X + ParentFixtureGroupComponent->GetSize().X;
		const float ParentBottom = ParentFixtureGroupComponent->GetPosition().Y + ParentFixtureGroupComponent->GetSize().Y;

		return
			Left > ParentLeft - .49f &&
			Top > ParentTop - .49f &&
			Right < ParentRight + .49f &&
			Bottom < ParentBottom + .49f;
	}

	return false;
}

void UDMXPixelMappingFixtureGroupItemComponent::RenderWithInputAndSendDMX()
{
	if (UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent())
	{
		RendererComponent->RendererInputTexture();
	}

	RenderAndSendDMX();
}

bool UDMXPixelMappingFixtureGroupItemComponent::CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const
{
	if (const UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(Component))
	{
		if (FixtureGroupComponent->DMXLibrary == FixturePatchRef.DMXLibrary)
		{
			return true;
		}
	}

	return false;
}

UDMXPixelMappingRendererComponent* UDMXPixelMappingFixtureGroupItemComponent::UDMXPixelMappingFixtureGroupItemComponent::GetRendererComponent() const
{
	return GetParent() ? Cast<UDMXPixelMappingRendererComponent>(GetParent()->GetParent()) : nullptr;
}

TMap<FDMXAttributeName, float> UDMXPixelMappingFixtureGroupItemComponent::CreateAttributeValues() const
{
	TMap<FDMXAttributeName, float> AttributeToNormalizedValueMap;

	UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();
	if (RendererComponent)
	{
		// Get the color data from the rendered component
		FLinearColor PixelColor;
		if (RendererComponent->GetDownsampleBufferPixel(DownsamplePixelIndex, PixelColor))
		{
			if (ColorMode == EDMXColorMode::CM_RGB)
			{
				if (AttributeRExpose)
				{
					const float AttributeRValue = FMath::Clamp(PixelColor.R, 0.f, 1.f);
					AttributeToNormalizedValueMap.Add(AttributeR, AttributeRValue);
				}

				if (AttributeGExpose)
				{
					const float AttributeGValue = FMath::Clamp(PixelColor.G, 0.f, 1.f);
					AttributeToNormalizedValueMap.Add(AttributeG, AttributeGValue);
				}

				if (AttributeBExpose)
				{
					const float AttributeBValue = FMath::Clamp(PixelColor.B, 0.f, 1.f);
					AttributeToNormalizedValueMap.Add(AttributeB, AttributeBValue);
				}
			}
			else if (ColorMode == EDMXColorMode::CM_Monochrome)
			{
				if (bMonochromeExpose)
				{
					// https://www.w3.org/TR/AERT/#color-contrast
					float Intensity = 0.299f * PixelColor.R + 0.587f * PixelColor.G + 0.114f * PixelColor.B;
					Intensity = FMath::Clamp(Intensity, 0.f, 1.f);

					AttributeToNormalizedValueMap.Add(MonochromeIntensity, Intensity);
				}
			}
		}
	}

	return AttributeToNormalizedValueMap;
}

#undef LOCTEXT_NAMESPACE
