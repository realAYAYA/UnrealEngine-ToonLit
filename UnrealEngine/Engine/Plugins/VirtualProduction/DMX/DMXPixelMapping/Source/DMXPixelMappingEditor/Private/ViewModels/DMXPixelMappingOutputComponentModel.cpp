// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/DMXPixelMappingOutputComponentModel.h"

#include "DMXPixelMappingComponentReference.h"
#include "DMXPixelMappingTypes.h"
#include "Components/DMXPixelMappingOutputComponent.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingScreenComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "MVR/Types/DMXMVRFixtureNode.h"
#include "Toolkits/DMXPixelMappingToolkit.h"


#define LOCTEXT_NAMESPACE "DMXPixelMappingOutputComponentModel"

FDMXPixelMappingOutputComponentModel::FDMXPixelMappingOutputComponentModel(const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, TWeakObjectPtr<UDMXPixelMappingOutputComponent> InOutputComponent)
	: WeakOutputComponent(InOutputComponent)
	, WeakToolkit(InToolkit)
{
	UDMXPixelMappingOutputComponent* OutputComponent = WeakOutputComponent.Get();
	if (!OutputComponent)
	{
		return;
	}

	UpdateFixtureNode();
	bSelected = InToolkit->GetSelectedComponents().Contains(FDMXPixelMappingComponentReference(InToolkit, OutputComponent));

	// Handle selection changes
	InToolkit->GetOnSelectedComponentsChangedDelegate().AddRaw(this, &FDMXPixelMappingOutputComponentModel::OnSelectedComponentsChanged);
}

FDMXPixelMappingOutputComponentModel::~FDMXPixelMappingOutputComponentModel()
{
	if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = WeakToolkit.Pin())
	{
		Toolkit->GetOnSelectedComponentsChangedDelegate().RemoveAll(this);
	}
}

FVector2D FDMXPixelMappingOutputComponentModel::GetPosition() const
{
	if (UDMXPixelMappingOutputComponent* OutputComponent = WeakOutputComponent.Get())
	{
		return OutputComponent->GetPosition();
	}
	return FVector2D::ZeroVector;
}

FVector2D FDMXPixelMappingOutputComponentModel::GetSize() const
{
	if (UDMXPixelMappingOutputComponent* OutputComponent = WeakOutputComponent.Get())
	{
		return OutputComponent->GetSize();
	}
	return FVector2D::ZeroVector;
}

FText FDMXPixelMappingOutputComponentModel::GetName() const
{
	if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(WeakOutputComponent.Get()))
	{
		return FText::FromString(OutputComponent->GetName());
	}

	return FText::GetEmpty();
}

bool FDMXPixelMappingOutputComponentModel::ShouldDrawName() const
{
	if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(WeakOutputComponent.Get()))
	{
		return OutputComponent->GetClass() != UDMXPixelMappingMatrixCellComponent::StaticClass();
	}
	return true;
}

bool FDMXPixelMappingOutputComponentModel::ShouldDrawNameAbove() const
{
	if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(WeakOutputComponent.Get()))
	{
		return
			OutputComponent->GetClass() == UDMXPixelMappingFixtureGroupComponent::StaticClass() ||
			OutputComponent->GetClass() == UDMXPixelMappingScreenComponent::StaticClass();
	}
	return true;
}

bool FDMXPixelMappingOutputComponentModel::HasPatchInfo() const
{
	return GetFixturePatch() != nullptr;
}

FText FDMXPixelMappingOutputComponentModel::GetAddressesText() const
{
	if (const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch())
	{
		const FString AddressesString = FString::Printf(TEXT("%i.%i"), FixturePatch->GetUniverseID(), FixturePatch->GetStartingChannel());
		
		return FText::Format(LOCTEXT("PatchTextWithLabel", "Patch: {0}"), FText::FromString(AddressesString));
	}

	return FText::GetEmpty();
}


FText FDMXPixelMappingOutputComponentModel::GetFixtureIDText() const
{
	if (UDMXMVRFixtureNode* FixtureNode = WeakFixtureNode.Get())
	{
		const FText FixtureIDText = FixtureNode->FixtureID.IsEmpty() ? LOCTEXT("NoFixtureID", "<not set>") : FText::FromString(FixtureNode->FixtureID);
		return FText::Format(LOCTEXT("FixtureIDTextWithLabel", "FID: {0}"), FixtureIDText);
	}

	return FText::GetEmpty();
}

bool FDMXPixelMappingOutputComponentModel::HasCellID() const
{
	if (UDMXPixelMappingMatrixCellComponent* MatrixCell = Cast<UDMXPixelMappingMatrixCellComponent>(WeakOutputComponent.Get()))
	{
		return true;
	}
	return false;
}

FText FDMXPixelMappingOutputComponentModel::GetCellIDText() const
{
	if (UDMXPixelMappingMatrixCellComponent* MatrixCell = Cast<UDMXPixelMappingMatrixCellComponent>(WeakOutputComponent.Get()))
	{
		return FText::FromString(FString::FromInt(MatrixCell->CellID));
	}

	return FText::GetEmpty();
}

FLinearColor FDMXPixelMappingOutputComponentModel::GetColor() const
{
	if (UDMXPixelMappingOutputComponent* OutputComponent = WeakOutputComponent.Get())
	{
		if (OutputComponent->IsOverParent())
		{
			if (bSelected)
			{
				return OutputComponent->GetEditorColor();
			}
			else
			{
				return OutputComponent->GetEditorColor().CopyWithNewOpacity(0.6f);
			}
		}
	}

	return FLinearColor::Red;
}

bool FDMXPixelMappingOutputComponentModel::Equals(UDMXPixelMappingBaseComponent* Other) const
{
	return WeakOutputComponent.Get() == Other; 
}

void FDMXPixelMappingOutputComponentModel::OnSelectedComponentsChanged()
{
	TSharedPtr<FDMXPixelMappingToolkit> Toolkit = WeakToolkit.Pin();
	if (!Toolkit.IsValid())
	{
		return;
	}

	if (UDMXPixelMappingOutputComponent* OutputComponent = WeakOutputComponent.Get())
	{
		bSelected = Toolkit->GetSelectedComponents().Contains(FDMXPixelMappingComponentReference(Toolkit, OutputComponent));
	}
}

const UDMXEntityFixturePatch* FDMXPixelMappingOutputComponentModel::GetFixturePatch() const
{
	const UDMXEntityFixturePatch* FixturePatch = nullptr;
	if (UDMXPixelMappingMatrixComponent* Matrix = Cast<UDMXPixelMappingMatrixComponent>(WeakOutputComponent.Get()))
	{
		FixturePatch = Matrix->FixturePatchRef.GetFixturePatch();
	}
	else if (UDMXPixelMappingFixtureGroupItemComponent* GroupItem = Cast<UDMXPixelMappingFixtureGroupItemComponent>(WeakOutputComponent.Get()))
	{
		FixturePatch = GroupItem->FixturePatchRef.GetFixturePatch();
	}

	return FixturePatch;
}

void FDMXPixelMappingOutputComponentModel::UpdateFixtureNode()
{
	WeakFixtureNode.Reset();

	const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	const FGuid& MVRUUID = FixturePatch->GetMVRFixtureUUID();
	if (!MVRUUID.IsValid())
	{
		return;
	}

	const UDMXLibrary* DMXLibrary = FixturePatch->GetParentLibrary();
	if (!DMXLibrary)
	{
		return;
	}

	// We can use the lazy General Scene Description
	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = DMXLibrary->GetLazyGeneralSceneDescription();
	if (!GeneralSceneDescription)
	{
		return;
	}

	WeakFixtureNode = GeneralSceneDescription->FindFixtureNode(MVRUUID);
}

FDMXPixelMappingScreenComponentModel::FDMXPixelMappingScreenComponentModel(const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, TWeakObjectPtr<UDMXPixelMappingScreenComponent> InScreenComponent)
	: WeakScreenComponent(InScreenComponent)
	, WeakToolkit(InToolkit)
{
	UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get();
	if (!ScreenComponent)
	{
		return;
	}

	bSelected = InToolkit->GetSelectedComponents().Contains(FDMXPixelMappingComponentReference(InToolkit, ScreenComponent));

	// Handle selection changes
	InToolkit->GetOnSelectedComponentsChangedDelegate().AddRaw(this, &FDMXPixelMappingScreenComponentModel::OnSelectedComponentsChanged);
}

FDMXPixelMappingScreenComponentModel::~FDMXPixelMappingScreenComponentModel()
{
	if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = WeakToolkit.Pin())
	{
		Toolkit->GetOnSelectedComponentsChangedDelegate().RemoveAll(this);
	}
}

FVector2D FDMXPixelMappingScreenComponentModel::GetPosition() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->GetPosition();
	}
	return FVector2D::ZeroVector;
}

FVector2D FDMXPixelMappingScreenComponentModel::GetSize() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->GetSize();
	}
	return FVector2D::ZeroVector;
}

int32 FDMXPixelMappingScreenComponentModel::GetNumColumns() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->NumXCells;
	}
	return 1;
}

int32 FDMXPixelMappingScreenComponentModel::GetNumRows() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->NumYCells;
	}
	return 1;
}

FLinearColor FDMXPixelMappingScreenComponentModel::GetColor() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		if (bSelected)
		{
			return ScreenComponent->GetEditorColor();
		}
		else
		{
			return ScreenComponent->GetEditorColor().CopyWithNewOpacity(0.6f);
		}
	}

	return FLinearColor::Red;
}

bool FDMXPixelMappingScreenComponentModel::Equals(UDMXPixelMappingBaseComponent* Other) const
{
	return WeakScreenComponent.Get() == Other;
}

void FDMXPixelMappingScreenComponentModel::OnSelectedComponentsChanged()
{
	TSharedPtr<FDMXPixelMappingToolkit> Toolkit = WeakToolkit.Pin();
	if (!Toolkit.IsValid())
	{
		return;
	}

	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		bSelected = Toolkit->GetSelectedComponents().Contains(FDMXPixelMappingComponentReference(Toolkit, ScreenComponent));
	}
}

EDMXPixelMappingDistribution FDMXPixelMappingScreenComponentModel::GetDistribution() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->Distribution;
	}
	return EDMXPixelMappingDistribution::BottomLeftToRight;
}

EDMXCellFormat FDMXPixelMappingScreenComponentModel::GetCellFormat() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->PixelFormat;
	}
	return EDMXCellFormat::PF_RGB;
}

bool FDMXPixelMappingScreenComponentModel::ComponentWantsToShowUniverse() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->bShowUniverse;
	}
	return false;
}

int32 FDMXPixelMappingScreenComponentModel::GetUniverse() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->LocalUniverse;
	}

	return 0;
}

bool FDMXPixelMappingScreenComponentModel::ComponentWantsToShowChannel() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->bShowAddresses;
	}
	return false;
}

int32 FDMXPixelMappingScreenComponentModel::GetStartingChannel() const
{
	if (UDMXPixelMappingScreenComponent* ScreenComponent = WeakScreenComponent.Get())
	{
		return ScreenComponent->StartAddress;
	}

	return 0;
}

#undef LOCTEXT_NAMESPACE
