// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMXPixelMappingMatrixCellComponent.h"

#include "DMXPixelMappingTypes.h"
#include "IDMXPixelMappingRenderer.h"
#include "ColorSpace/DMXPixelMappingColorSpace.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"

#if WITH_EDITOR
#include "DMXPixelMappingComponentWidget.h"
#include "SDMXPixelMappingComponentBox.h"
#include "SDMXPixelMappingComponentLabel.h"
#endif // WITH_EDITOR

#include "TextureResource.h"
#include "Engine/Texture.h"


DECLARE_CYCLE_STAT(TEXT("Send Matrix Cell"), STAT_DMXPixelMaping_SendMatrixCell, STATGROUP_DMXPIXELMAPPING);

#define LOCTEXT_NAMESPACE "DMXPixelMappingMatrixPixelComponent"

UDMXPixelMappingMatrixCellComponent::UDMXPixelMappingMatrixCellComponent()
	: DownsamplePixelIndex(0)
{
#if WITH_EDITORONLY_DATA
	bLockInDesigner = true;
	EditorColor = FLinearColor::White.CopyWithNewOpacity(.4f);
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UDMXPixelMappingMatrixCellComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Call the parent at the first place
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
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixCellComponent, CellID))
	{
		if (ComponentWidget_DEPRECATED.IsValid())
		{
			ComponentWidget_DEPRECATED->GetComponentBox()->SetIDText(FText::Format(LOCTEXT("CellID", "{0}"), CellID));
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif // WITH_EDITOR

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedRef<FDMXPixelMappingComponentWidget> UDMXPixelMappingMatrixCellComponent::BuildSlot(TSharedRef<SConstraintCanvas> InCanvas)
{
	ComponentWidget_DEPRECATED = Super::BuildSlot(InCanvas);

	// Expect super to construct the component widget
	if (ensureMsgf(ComponentWidget_DEPRECATED.IsValid(), TEXT("PixelMapping: Expected Super to construct a component widget, but didn't.")))
	{
		ComponentWidget_DEPRECATED->GetComponentLabel()->SetText(FText::GetEmpty());
		ComponentWidget_DEPRECATED->GetComponentBox()->SetIDText(FText::Format(LOCTEXT("CellID", "{0}"), CellID));
	}

	return ComponentWidget_DEPRECATED.ToSharedRef();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR

#if WITH_EDITOR
bool UDMXPixelMappingMatrixCellComponent::IsVisible() const
{
	// Needs be over the matrix and over the group
	if (UDMXPixelMappingMatrixComponent* ParentMatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(GetParent()))
	{
		if (!ParentMatrixComponent->IsVisible())
		{
			return false;
		}
	}

	return Super::IsVisible();
}
#endif // WITH_EDITOR

#if WITH_EDITOR
FLinearColor UDMXPixelMappingMatrixCellComponent::GetEditorColor() const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (bLockInDesigner)
	{
		// When locked in designer, always use the parent color. So when the parent shows an error color, show it too.
		if (UDMXPixelMappingMatrixComponent* ParentMatrixComponent = Cast< UDMXPixelMappingMatrixComponent>(GetParent()))
		{
			if (const TSharedPtr<FDMXPixelMappingComponentWidget>& ParentComponentWidget = ParentMatrixComponent->GetComponentWidget())
			{
				return ParentComponentWidget->GetColor();
			}
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	return EditorColor;
}
#endif // WITH_EDITOR

bool UDMXPixelMappingMatrixCellComponent::IsOverParent() const
{
	// Needs be over the matrix and over the group
	UDMXPixelMappingMatrixComponent* ParentMatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(GetParent());
	if (!ParentMatrixComponent)
	{
		return false;
	}
	
	const bool bIsParentMatrixOverGroup = ParentMatrixComponent->IsOverParent();
	if (!bIsParentMatrixOverGroup)
	{
		return false;
	}

	const float Left = GetPosition().X;
	const float Top = GetPosition().Y;
	const float Right = GetPosition().X + GetSize().X;
	const float Bottom = GetPosition().Y + GetSize().Y;

	const float ParentLeft = ParentMatrixComponent->GetPosition().X;
	const float ParentTop = ParentMatrixComponent->GetPosition().Y;
	const float ParentRight = ParentMatrixComponent->GetPosition().X + ParentMatrixComponent->GetSize().X;
	const float ParentBottom = ParentMatrixComponent->GetPosition().Y + ParentMatrixComponent->GetSize().Y;

	return
		Left > ParentLeft - .49f &&
		Top > ParentTop - .49f &&
		Right < ParentRight + .49f &&
		Bottom < ParentBottom + .49f;
}

void UDMXPixelMappingMatrixCellComponent::SetPosition(const FVector2D& NewPosition)
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

void UDMXPixelMappingMatrixCellComponent::SetSize(const FVector2D& NewSize)
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

const FName& UDMXPixelMappingMatrixCellComponent::GetNamePrefix()
{
	static FName NamePrefix = TEXT("MatrixCell");
	return NamePrefix;
}

void UDMXPixelMappingMatrixCellComponent::ResetDMX()
{
	UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();
	if (RendererComponent)
	{
		RendererComponent->ResetColorDownsampleBufferPixel(DownsamplePixelIndex);
	}

	// No need to send dmx, that is done by the parent matrix
}

FString UDMXPixelMappingMatrixCellComponent::GetUserFriendlyName() const
{
	if (UDMXPixelMappingMatrixComponent* MatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(GetParent()))
	{
		UDMXEntityFixturePatch* FixturePatch = MatrixComponent->FixturePatchRef.GetFixturePatch();
		
		if (FixturePatch)
		{
			return FString::Printf(TEXT("%s: Cell %d"), *FixturePatch->GetDisplayName(), CellID);
		}
	}

	return FString(TEXT("Invalid Patch"));
}

void UDMXPixelMappingMatrixCellComponent::QueueDownsample()
{
	// Queue pixels into the downsample rendering
	UDMXPixelMappingMatrixComponent* MatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(GetParent());
	UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();

	if (!ensure(MatrixComponent))
	{
		return;
	}

	if (!ensure(RendererComponent))
	{
		return;
	}

	UTexture* InputTexture = RendererComponent->GetRendererInputTexture();
	if (!ensure(InputTexture))
	{
		return;
	}

	// Store downsample index
	DownsamplePixelIndex = RendererComponent->GetDownsamplePixelNum();

	const uint32 TextureSizeX = InputTexture->GetResource()->GetSizeX();
	const uint32 TextureSizeY = InputTexture->GetResource()->GetSizeY();
	check(TextureSizeX > 0 && TextureSizeY > 0);
	const FIntPoint PixelPosition = RendererComponent->GetPixelPosition(DownsamplePixelIndex);
	const FVector2D UV = FVector2D(GetPosition().X / TextureSizeX, GetPosition().Y / TextureSizeY);
	const FVector2D UVSize(GetSize().X / TextureSizeX, GetSize().Y / TextureSizeY);
	const FVector2D UVCellSize = UVSize / 2.f;
	constexpr bool bStaticCalculateUV = true;
			
	FDMXPixelMappingDownsamplePixelParamsV2 DownsamplePixelParam
	{ 
		PixelPosition,
		UV,
		UVSize,
		UVCellSize,
		CellBlendingQuality,
		bStaticCalculateUV
	};

	RendererComponent->AddPixelToDownsampleSet(MoveTemp(DownsamplePixelParam));
}

void UDMXPixelMappingMatrixCellComponent::SetCellCoordinate(FIntPoint InCellCoordinate)
{
	CellCoordinate = InCellCoordinate;
}

void UDMXPixelMappingMatrixCellComponent::RenderWithInputAndSendDMX()
{
	if (UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent())
	{
		RendererComponent->RendererInputTexture();
	}

	RenderAndSendDMX();
}

bool UDMXPixelMappingMatrixCellComponent::CanBeMovedTo(const UDMXPixelMappingBaseComponent* OtherComponent) const
{
	return OtherComponent && OtherComponent == GetParent();
}

UDMXPixelMappingRendererComponent* UDMXPixelMappingMatrixCellComponent::GetRendererComponent() const
{
	for (UDMXPixelMappingBaseComponent* ParentComponent = GetParent(); ParentComponent; ParentComponent = ParentComponent->GetParent())
	{
		if (UDMXPixelMappingRendererComponent* RendererComponent = Cast<UDMXPixelMappingRendererComponent>(ParentComponent))
		{
			return RendererComponent;
		}
	}
	return nullptr;
}

TMap<FDMXAttributeName, float> UDMXPixelMappingMatrixCellComponent::CreateAttributeValues() const
{
	TMap<FDMXAttributeName, float> AttributeToNormalizedValueMap;

	UDMXPixelMappingMatrixComponent* ParentMatrix = Cast<UDMXPixelMappingMatrixComponent>(GetParent());
	if (!ParentMatrix)
	{
		return AttributeToNormalizedValueMap;
	}

	UDMXPixelMappingColorSpace* ColorSpace = ParentMatrix->ColorSpace;
	if (!ColorSpace)
	{
		return AttributeToNormalizedValueMap;
	}

	UDMXPixelMappingRendererComponent* RendererComponent = GetRendererComponent();
	if (!RendererComponent)
	{
		return AttributeToNormalizedValueMap;
	}

	// Get the color data from the rendered component
	FLinearColor PixelColor;
	if (RendererComponent->GetDownsampleBufferPixel(DownsamplePixelIndex, PixelColor))
	{
		ColorSpace->SetRGBA(PixelColor);
		AttributeToNormalizedValueMap = ColorSpace->GetAttributeNameToValueMap();
	}

	return AttributeToNormalizedValueMap;
}

#undef LOCTEXT_NAMESPACE
