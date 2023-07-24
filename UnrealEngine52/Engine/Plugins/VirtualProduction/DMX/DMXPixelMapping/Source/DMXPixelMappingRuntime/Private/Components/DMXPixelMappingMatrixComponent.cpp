// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMXPixelMappingMatrixComponent.h"

#include "DMXPixelMapping.h"
#include "DMXPixelMappingMainStreamObjectVersion.h"
#include "DMXPixelMappingRuntimeCommon.h"
#include "DMXPixelMappingRuntimeUtils.h"
#include "DMXPixelMappingTypes.h"
#include "ColorSpace/DMXPixelMappingColorSpace_RGBCMY.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "IO/DMXOutputPort.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Modulators/DMXModulator.h"
#include "Templates/DMXPixelMappingComponentTemplate.h"

#if WITH_EDITOR
#include "DMXPixelMappingComponentWidget.h"
#endif // WITH_EDITOR


#define LOCTEXT_NAMESPACE "DMXPixelMappingMatrixComponent"

UDMXPixelMappingMatrixComponent::UDMXPixelMappingMatrixComponent()
{
	ColorSpaceClass = UDMXPixelMappingColorSpace_RGBCMY::StaticClass();
	ColorSpace = CreateDefaultSubobject<UDMXPixelMappingColorSpace_RGBCMY>("ColorSpace");

	SetSize(FVector2D(32.f, 32.f));

#if WITH_EDITORONLY_DATA
	// Even tho deprecated, default values on deprecated properties need be set so they don't load their type's default value.
	ColorMode_DEPRECATED = EDMXColorMode::CM_RGB;
	AttributeR_DEPRECATED.SetFromName("Red");
	AttributeG_DEPRECATED.SetFromName("Green");
	AttributeB_DEPRECATED.SetFromName("Blue");
#endif // WITH_EDITORONLY_DATA
}

void UDMXPixelMappingMatrixComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	Ar.UsingCustomVersion(FDMXPixelMappingMainStreamObjectVersion::GUID);
	if(Ar.IsLoading())
	{
		if (Ar.CustomVer(FDMXPixelMappingMainStreamObjectVersion::GUID) < FDMXPixelMappingMainStreamObjectVersion::ChangePixelMappingMatrixComponentToFixturePatchReference)
		{
			// Upgrade from custom FixturePatchMatrixRef to FixturePatchRef
			FixturePatchRef.SetEntity(FixturePatchMatrixRef_DEPRECATED.GetFixturePatch());
		}

		if (Ar.CustomVer(FDMXPixelMappingMainStreamObjectVersion::GUID) < FDMXPixelMappingMainStreamObjectVersion::UseDMXPixelMappingColorSpace)
		{		
			// Upgrade to the DMXPixelMappingColorSpace default subobject
			if (!ensureMsgf(ColorSpace, TEXT("Missing default Subobject ColorSpace")))
			{
				ColorSpace = NewObject<UDMXPixelMappingColorSpace_RGBCMY>(this, "ColorSpace");
			}
			UDMXPixelMappingColorSpace_RGBCMY* ColorSpace_RGBCMY = Cast<UDMXPixelMappingColorSpace_RGBCMY>(ColorSpace);

			if (ColorMode_DEPRECATED == EDMXColorMode::CM_Monochrome)
			{
				ColorSpace_RGBCMY->LuminanceAttribute = MonochromeIntensity_DEPRECATED;
				ColorSpace_RGBCMY->RedAttribute = FDMXAttributeName(NAME_None);
				ColorSpace_RGBCMY->GreenAttribute = FDMXAttributeName(NAME_None);
				ColorSpace_RGBCMY->BlueAttribute = FDMXAttributeName(NAME_None);
			}
			else
			{
				ColorSpace_RGBCMY->bSendCyan = AttributeRInvert_DEPRECATED;
				ColorSpace_RGBCMY->bSendMagenta = AttributeGInvert_DEPRECATED;
				ColorSpace_RGBCMY->bSendYellow = AttributeBInvert_DEPRECATED;
				ColorSpace_RGBCMY->RedAttribute = AttributeR_DEPRECATED;
				ColorSpace_RGBCMY->GreenAttribute = AttributeG_DEPRECATED;
				ColorSpace_RGBCMY->BlueAttribute = AttributeB_DEPRECATED;
				ColorSpace_RGBCMY->LuminanceType = EDMXPixelMappingLuminanceType_RGBCMY::None;
				ColorSpace_RGBCMY->LuminanceAttribute = FDMXAttributeName(NAME_None);
			}
		}
	}
#endif
}

void UDMXPixelMappingMatrixComponent::PostLoad()
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

	// Set the transient Color Space Class property to the initial value
	if (ensureMsgf(ColorSpace, TEXT("Unexpected invalid Color Space in Pixel Mapping Fixture Group Item %s."), *GetName()))
	{
		ColorSpaceClass = ColorSpace->GetClass();
	}
}

void UDMXPixelMappingMatrixComponent::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		// Listen to Fixture Type and Fixture Patch changes
		UDMXEntityFixtureType::GetOnFixtureTypeChanged().AddUObject(this, &UDMXPixelMappingMatrixComponent::OnFixtureTypeChanged);
		UDMXEntityFixturePatch::GetOnFixturePatchChanged().AddUObject(this, &UDMXPixelMappingMatrixComponent::OnFixturePatchChanged);
	}
}

void UDMXPixelMappingMatrixComponent::LogInvalidProperties()
{
	UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.GetFixturePatch();
	if (IsValid(FixturePatch))
	{
		const FDMXFixtureMode* ModePtr = FixturePatch->GetActiveMode();
		if (!ModePtr)
		{
			UE_LOG(LogDMXPixelMappingRuntime, Warning, TEXT("%s has no valid Active Mode set. %s will not receive DMX."), *FixturePatch->GetDisplayName(), *GetName());
		}
		else if (!FixturePatch->GetFixtureType())
		{
			UE_LOG(LogDMXPixelMappingRuntime, Warning, TEXT("%s has no valid Fixture Type set. %s will not receive DMX."), *FixturePatch->GetDisplayName(), *GetName());
		}
		else if(Children.Num() != ModePtr->FixtureMatrixConfig.XCells * ModePtr->FixtureMatrixConfig.YCells)
			{
				UE_LOG(LogDMXPixelMappingRuntime, Warning, TEXT("Number of cells in %s no longer matches %s. %s will not function properly."), *GetName(), *FixturePatch->GetFixtureType()->Name, *GetName());
			}
		}
	else
	{
		UE_LOG(LogDMXPixelMappingRuntime, Warning, TEXT("%s has no valid Fixture Patch set."), *GetName());
	}
}

#if WITH_EDITOR
void UDMXPixelMappingMatrixComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange)
	{
		const FName PropertyName = PropertyAboutToChange->GetFName();

		if (PropertyName == UDMXPixelMappingOutputComponent::GetPositionXPropertyName() ||
			PropertyName == UDMXPixelMappingOutputComponent::GetPositionYPropertyName())
		{
			PreEditChangePosition = GetPosition();
		}
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXPixelMappingMatrixComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Note, property changes of fixture patch are listened for in tick

	// Call the parent at the first place
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, ColorSpaceClass))
	{
		if (ensureAlwaysMsgf(ColorSpaceClass, TEXT("Color Space Class was set to nullptr. This is not supported.")))
		{
			ColorSpace = NewObject<UDMXPixelMappingColorSpace>(this, ColorSpaceClass);
		}
	}
	
	if (PropertyName == UDMXPixelMappingOutputComponent::GetPositionXPropertyName() ||
		PropertyName == UDMXPixelMappingOutputComponent::GetPositionYPropertyName())
	{
		constexpr bool bUpdateSizeRecursive = false;
		ForEachChildOfClass<UDMXPixelMappingMatrixCellComponent>([this](UDMXPixelMappingMatrixCellComponent* ChildComponent)
			{
				FVector2D RelativePosition = ChildComponent->GetPosition() - PreEditChangePosition;
				ChildComponent->SetPosition(GetPosition() + RelativePosition);

			}, bUpdateSizeRecursive);

		HandlePositionChanged();
	}
	
	if (PropertyName == UDMXPixelMappingOutputComponent::GetSizeXPropertyName() ||
		PropertyName == UDMXPixelMappingOutputComponent::GetSizeYPropertyName() ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, CellSize))
	{
		HandleSizeChanged();

		// Update size again from the new CellSize that results from handling size or matrix changes
		const FVector2D NewSize(CellSize.X * CoordinateGrid.X, CellSize.Y * CoordinateGrid.Y);
		SetSize(NewSize);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, FixturePatchRef) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, CoordinateGrid) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FDMXEntityReference, DMXLibrary))
	{
		HandleMatrixChanged();
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXPixelMappingMatrixComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedChainEvent);

	// For consistency with GroupItem, handling modulator class changes in runtime utils
	FDMXPixelMappingRuntimeUtils::HandleModulatorPropertyChange(this, PropertyChangedChainEvent, ModulatorClasses, Modulators);
}
#endif // WITH_EDITOR

#if WITH_EDITOR
const FText UDMXPixelMappingMatrixComponent::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}
#endif // WITH_EDITOR

bool UDMXPixelMappingMatrixComponent::IsOverParent() const
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

const FName& UDMXPixelMappingMatrixComponent::GetNamePrefix()
{
	static FName NamePrefix = TEXT("Matrix");
	return NamePrefix;
}

void UDMXPixelMappingMatrixComponent::ResetDMX()
{
	ForEachChild([&](UDMXPixelMappingBaseComponent* InComponent)
	{
		if (UDMXPixelMappingOutputComponent* Component = Cast<UDMXPixelMappingOutputComponent>(InComponent))
		{
			Component->ResetDMX();
		}
	}, false);

	SendDMX();
}

void UDMXPixelMappingMatrixComponent::SendDMX()
{
	UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.GetFixturePatch();

	if (FixturePatch)
	{
		// An array of attribute to value maps for each child, in order of the childs
		TArray<FDMXNormalizedAttributeValueMap> AttributeToValueMapArray;

		// Accumulate matrix cell data
		for (UDMXPixelMappingBaseComponent* Component : Children)
		{
			if (UDMXPixelMappingMatrixCellComponent* CellComponent = Cast<UDMXPixelMappingMatrixCellComponent>(Component))
			{
				FDMXNormalizedAttributeValueMap NormalizedAttributeToValueMap;
				NormalizedAttributeToValueMap.Map = CellComponent->CreateAttributeValues();

				AttributeToValueMapArray.Add(NormalizedAttributeToValueMap);
			}
		}

		// Apply matrix modulators
		for (UDMXModulator* Modulator : Modulators)
		{
			Modulator->ModulateMatrix(FixturePatch, AttributeToValueMapArray, AttributeToValueMapArray);
		}

		for (int32 IndexChild = 0; IndexChild < Children.Num(); IndexChild++)
		{
			if (UDMXPixelMappingMatrixCellComponent* CellComponent = Cast<UDMXPixelMappingMatrixCellComponent>(Children[IndexChild]))
			{
				// Relies on the order of childs and AttributeToValueMapArray didn't change during the lifetime of this function!
				if (!AttributeToValueMapArray.IsValidIndex(IndexChild))
				{
					break;
				}

				TMap<int32, uint8> ChannelToValueMap;
				for (const TTuple<FDMXAttributeName, float>& AttributeValuePair : AttributeToValueMapArray[IndexChild].Map)
				{
					FDMXPixelMappingRuntimeUtils::ConvertNormalizedAttributeValueToChannelValue(FixturePatch, AttributeValuePair.Key, AttributeValuePair.Value, ChannelToValueMap);

					FixturePatch->SendNormalizedMatrixCellValue(CellComponent->CellCoordinate, AttributeValuePair.Key, AttributeValuePair.Value);
				}
			}
		}

		// Send normal modulators. This is important to allow modulators to generate attribute values
		TMap<FDMXAttributeName, float> ModulatorGeneratedAttributeValueMap;
		for (UDMXModulator* Modulator : Modulators)
		{
			Modulator->Modulate(FixturePatch, ModulatorGeneratedAttributeValueMap, ModulatorGeneratedAttributeValueMap);
		}

		TMap<int32, uint8> ChannelToValueMap;
		for (const TTuple<FDMXAttributeName, float>& AttributeValuePair : ModulatorGeneratedAttributeValueMap)
		{
			FDMXPixelMappingRuntimeUtils::ConvertNormalizedAttributeValueToChannelValue(FixturePatch, AttributeValuePair.Key, AttributeValuePair.Value, ChannelToValueMap);
		}

		if (UDMXLibrary* Library = FixturePatch->GetParentLibrary())
		{
			for (const FDMXOutputPortSharedRef& OutputPort : Library->GetOutputPorts())
			{
				OutputPort->SendDMX(FixturePatch->GetUniverseID(), ChannelToValueMap);
			}
		}
	}
}

void UDMXPixelMappingMatrixComponent::QueueDownsample()
{
	ForEachChild([&](UDMXPixelMappingBaseComponent* InComponent) {
		if (UDMXPixelMappingOutputComponent* Component = Cast<UDMXPixelMappingOutputComponent>(InComponent))
		{
			Component->QueueDownsample();
		}
	}, false);
}

bool UDMXPixelMappingMatrixComponent::CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const
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

FString UDMXPixelMappingMatrixComponent::GetUserFriendlyName() const
{
	if (UDMXEntityFixturePatch* Patch = FixturePatchRef.GetFixturePatch())
	{
		return FString::Printf(TEXT("Fixture Matrix: %s"), *Patch->GetDisplayName());
	}

	return FString(TEXT("Fixture Matrix: No Fixture Patch"));
}

void UDMXPixelMappingMatrixComponent::SetPosition(const FVector2D& NewPosition)
{
	const FVector2D OldPosition = GetPosition();

	Super::SetPosition(NewPosition);

	constexpr bool bUpdateSizeRecursive = false;
	ForEachChildOfClass<UDMXPixelMappingMatrixCellComponent>([&OldPosition, &NewPosition](UDMXPixelMappingMatrixCellComponent* ChildComponent)
		{
			FVector2D RelativePosition = ChildComponent->GetPosition() - OldPosition;
			ChildComponent->SetPosition(NewPosition + RelativePosition);

		}, bUpdateSizeRecursive);


	HandlePositionChanged();
}

void UDMXPixelMappingMatrixComponent::SetSize(const FVector2D& NewSize)
{
	const FVector2D OldSize = GetSize();
	Super::SetSize(NewSize);

	if (OldSize != NewSize)
	{
		HandleSizeChanged();
	}
}

void UDMXPixelMappingMatrixComponent::HandlePositionChanged()
{
#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetPosition(GetPosition());
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

void UDMXPixelMappingMatrixComponent::HandleSizeChanged()
{
	CellSize = FVector2D::ZeroVector;
	if (CoordinateGrid.X > 0 && CoordinateGrid.Y > 0)
	{
		CellSize = FVector2D(GetSize().X / CoordinateGrid.X, GetSize().Y / CoordinateGrid.Y);
	}

	if (Children.Num() > 0)
	{
		constexpr bool bUpdateSizeRecursive = false;
		ForEachChildOfClass<UDMXPixelMappingMatrixCellComponent>([this](UDMXPixelMappingMatrixCellComponent* ChildComponent)
			{
#if WITH_EDITOR
				if (ChildComponent->IsLockInDesigner())
				{
					ChildComponent->SetSize(CellSize);
					ChildComponent->SetPosition(GetPosition() + FVector2D(CellSize * ChildComponent->GetCellCoordinate()));
				}
#else
				ChildComponent->SetSize(CellSize);
				ChildComponent->SetPosition(GetPosition() + FVector2D(CellSize * ChildComponent->GetCellCoordinate()));
#endif // WITH_EDITOR

			}, bUpdateSizeRecursive);
	}
	else
	{
		// Set the default size
		const FVector2D DefaultSize(32.f, 32.f);
		SetSize(DefaultSize);
	}

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetSize(GetSize());
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

void UDMXPixelMappingMatrixComponent::HandleMatrixChanged()
{
	TGuardValue<bool>(bIsUpdatingChildren, true);

	UDMXPixelMapping* PixelMapping = GetPixelMapping();
	UDMXPixelMappingRootComponent* RootComponent = PixelMapping ? PixelMapping->GetRootComponent() : nullptr;
	if (!RootComponent)
	{
		// Component was removed from pixel mapping
		return;
	}

	CoordinateGrid = 0;

	// Remove all existing children and rebuild them anew
	for (UDMXPixelMappingBaseComponent* Child : TArray<UDMXPixelMappingBaseComponent*>(Children))
	{
		RemoveChild(Child);
	}

	UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.GetFixturePatch();
	UDMXEntityFixtureType* FixtureType = FixturePatch ? FixturePatch->GetFixtureType() : nullptr;
	const FDMXFixtureMode* ModePtr = FixturePatch ? FixturePatch->GetActiveMode() : nullptr;
	if (FixturePatch && FixtureType && ModePtr && ModePtr->bFixtureMatrixEnabled)
	{
		TArray<FDMXCell> MatrixCells;
		if (FixturePatch->GetAllMatrixCells(MatrixCells))
		{
			Distribution = ModePtr->FixtureMatrixConfig.PixelMappingDistribution;
			CoordinateGrid = FIntPoint(ModePtr->FixtureMatrixConfig.XCells, ModePtr->FixtureMatrixConfig.YCells);

			for (const FDMXCell& Cell : MatrixCells)
			{
				TSharedPtr<FDMXPixelMappingComponentTemplate> ComponentTemplate = MakeShared<FDMXPixelMappingComponentTemplate>(UDMXPixelMappingMatrixCellComponent::StaticClass());
				UDMXPixelMappingMatrixCellComponent* Component = ComponentTemplate->CreateComponent<UDMXPixelMappingMatrixCellComponent>(RootComponent);

				Component->CellID = Cell.CellID;
				Component->SetCellCoordinate(Cell.Coordinate);

				AddChild(Component);
			}
		}
	}

	HandleSizeChanged();

	GetOnMatrixChanged().Broadcast(PixelMapping, this);
}

void UDMXPixelMappingMatrixComponent::OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType)
{
	if (UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.GetFixturePatch())
	{
		if (FixturePatch->GetFixtureType() == FixtureType)
		{
			HandleMatrixChanged();

			LogInvalidProperties();
		}
	}
}

void UDMXPixelMappingMatrixComponent::OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch)
{
	if (FixturePatch)
	{
		HandleMatrixChanged();

		LogInvalidProperties();
	}
}

#undef LOCTEXT_NAMESPACE
