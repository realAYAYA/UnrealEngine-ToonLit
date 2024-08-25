// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Generators/DMTextureUVPropertyRowGenerator.h"
#include "Components/DMMaterialComponent.h"
#include "Components/DMTextureUV.h"
#include "DMEDefs.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Slate/SDMEditor.h"
#include "Slate/Properties/Editors/SDMPropertyEditFloat.h"
#include "Slate/Properties/Editors/SDMPropertyEditVector.h"
#include "Slate/SDMComponentEdit.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "DMTextureUVPropertyRowGenerator"

const TSharedRef<FDMTextureUVPropertyRowGenerator>& FDMTextureUVPropertyRowGenerator::Get()
{
	static TSharedRef<FDMTextureUVPropertyRowGenerator> Generator = MakeShared<FDMTextureUVPropertyRowGenerator>();
	return Generator;
}

namespace UE::DynamicMaterialEditor::Private
{
	void AddRotationRow(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV, TArray<FDMPropertyHandle>& InOutPropertyRows);

	void AddScaleRow(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV, TArray<FDMPropertyHandle>& InOutPropertyRows);

	TSharedRef<SWidget> CreateScaleLinkWidget(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV);
	ECheckBoxState GetScaleLinkWidgetState(UDMTextureUV* InTextureUV);
	void OnScaleLinkWidgetChanged(ECheckBoxState InNewState, UDMTextureUV* InTextureUV);

	TSharedRef<SWidget> CreateScaleValueWidget(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV);

	void AddMirrorRow(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV, TArray<FDMPropertyHandle>& InOutPropertyRows);
	TSharedRef<SWidget> CreateMirrorExtensionButtons(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV);
}

void FDMTextureUVPropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
	TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	if (InOutProcessedObjects.Contains(InComponent))
	{
		return;
	}

	UDMTextureUV* TextureUV = Cast<UDMTextureUV>(InComponent);

	if (!TextureUV)
	{
		return;
	}

	InOutProcessedObjects.Add(InComponent);

	using namespace UE::DynamicMaterialEditor::Private;

	const int PropertyCount = InOutPropertyRows.Num();

	AddPropertyEditRows(InComponentEditWidget, InComponent, UDMTextureUV::NAME_Offset, InOutPropertyRows, InOutProcessedObjects);
	AddRotationRow(InComponentEditWidget, TextureUV, InOutPropertyRows);
	AddScaleRow(InComponentEditWidget, TextureUV, InOutPropertyRows);
	AddPropertyEditRows(InComponentEditWidget, InComponent, UDMTextureUV::NAME_Pivot, InOutPropertyRows, InOutProcessedObjects);
	AddMirrorRow(InComponentEditWidget, TextureUV, InOutPropertyRows);
}

bool FDMTextureUVPropertyRowGenerator::AllowKeyframeButton(UDMMaterialComponent* InComponent, FProperty* InProperty)
{
	if (InProperty)
	{
		const bool* AddKeyframeButtonPtr = UDMTextureUV::TextureProperties.Find(InProperty->GetFName());

		if (AddKeyframeButtonPtr)
		{
			return *AddKeyframeButtonPtr;
		}
	}

	return FDMComponentPropertyRowGenerator::AllowKeyframeButton(InComponent, InProperty);
}

void UE::DynamicMaterialEditor::Private::AddRotationRow(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV, TArray<FDMPropertyHandle>& InOutPropertyRows)
{
	InOutPropertyRows.Add(SDMEditor::GetPropertyHandle(&*InComponentEditWidget, InTextureUV, UDMTextureUV::NAME_Rotation));
}

void UE::DynamicMaterialEditor::Private::AddScaleRow(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV, TArray<FDMPropertyHandle>& InOutPropertyRows)
{
	InOutPropertyRows.Add(SDMEditor::GetPropertyHandle(&*InComponentEditWidget, InTextureUV, UDMTextureUV::NAME_Scale));
}

TSharedRef<SWidget> UE::DynamicMaterialEditor::Private::CreateScaleLinkWidget(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV)
{
	const FSlateBrush* Checked = FSlateIconFinder::FindIcon("PropertyWindow.Locked").GetIcon();
	const FSlateBrush* Unchecked = FSlateIconFinder::FindIcon("PropertyWindow.Unlocked").GetIcon();
	const FSlateBrush* NoBorder = FSlateIconFinder::FindIcon("NoBorder").GetIcon();

	return SNew(SCheckBox)
		.IsChecked_Static(&UE::DynamicMaterialEditor::Private::GetScaleLinkWidgetState, InTextureUV)
		.OnCheckStateChanged_Static(&UE::DynamicMaterialEditor::Private::OnScaleLinkWidgetChanged, InTextureUV)
		.CheckedImage(Checked)
		.CheckedHoveredImage(Checked)
		.CheckedPressedImage(Checked)
		.UncheckedImage(Unchecked)
		.UncheckedHoveredImage(Unchecked)
		.UncheckedPressedImage(Unchecked)
		.BackgroundImage(NoBorder)
		.BackgroundHoveredImage(NoBorder)
		.BackgroundPressedImage(NoBorder);
}

ECheckBoxState UE::DynamicMaterialEditor::Private::GetScaleLinkWidgetState(UDMTextureUV* InTextureUV)
{
	if (!IsValid(InTextureUV))
	{
		return ECheckBoxState::Unchecked;
	}

	return InTextureUV->bLinkScale ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void UE::DynamicMaterialEditor::Private::OnScaleLinkWidgetChanged(ECheckBoxState InNewState, UDMTextureUV* InTextureUV)
{
	if (!IsValid(InTextureUV))
	{
		return;
	}

	InTextureUV->bLinkScale = (InNewState == ECheckBoxState::Checked);
}

namespace UE::DynamicMaterialEditor::Private
{
	class SDMPropertyEditVectorScale : public SDMPropertyEditVector
	{
	public:
		using Super = SDMPropertyEditVector;

		void Construct(const FArguments& InArgs, const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV)
		{
			if (ensure(IsValid(InTextureUV)))
			{
				Super::Construct(
					Super::FArguments()
						.ComponentEditWidget(InComponentEditWidget), 
					InTextureUV->GetPropertyHandle(UDMTextureUV::NAME_Scale), 
					2
				);
			}
		}

	protected:
		FVector2D InitialScale;

		virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override
		{
			TSharedRef<SWidget> Widget = Super::GetComponentWidget(InIndex);

			if (InIndex > 0)
			{
				TWeakPtr<SDMPropertyEditVectorScale> ThisWeak = SharedThis(this).ToWeakPtr();

				Widget->SetVisibility(TAttribute<EVisibility>::CreateLambda([ThisWeak]()
					{
						if (TSharedPtr<SDMPropertyEditVectorScale> This = ThisWeak.Pin())
						{
							TArray<UObject*> Outers;
							This->PropertyHandle->GetOuterObjects(Outers);

							if (Outers.Num() == 1 && IsValid(Outers[0]))
							{
								if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(Outers[0]))
								{
									if (TextureUV->bLinkScale && FMath::IsNearlyEqual(TextureUV->GetScale().X, TextureUV->GetScale().Y))
									{
										return EVisibility::Hidden;
									}
								}
							}
						}

						return EVisibility::Visible;
					}));
				Widget->SetEnabled(TAttribute<bool>::CreateLambda([ThisWeak]()
					{
						if (TSharedPtr<SDMPropertyEditVectorScale> This = ThisWeak.Pin())
						{
							TArray<UObject*> Outers;
							This->PropertyHandle->GetOuterObjects(Outers);

							if (Outers.Num() == 1 && IsValid(Outers[0]))
							{
								if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(Outers[0]))
								{
									if (TextureUV->bLinkScale)
									{
										return false;
									}
								}
							}
						}

						return true;
					}));
			}

			return Widget;
		}

		virtual void StartTransaction(FText InDescription) override
		{
			Super::StartTransaction(InDescription);

			if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
			{
				TArray<UObject*> Outers;
				PropertyHandle->GetOuterObjects(Outers);

				if (Outers.IsEmpty() == false)
				{
					if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(Outers[0]))
					{
						InitialScale = TextureUV->GetScale();
					}
				}
			}
		}

		virtual void OnValueChanged(float InNewValue, int32 InComponent) const override
		{
			if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
			{
				TArray<UObject*> Outers;
				PropertyHandle->GetOuterObjects(Outers);

				if (Outers.IsEmpty() == false)
				{
					if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(Outers[0]))
					{
						FVector2D CurrentScale = TextureUV->GetScale();

						if (FMath::IsNearlyEqual(CurrentScale[InComponent], InNewValue))
						{
							return;
						}

						auto IsScaledLinked = [this, TextureUV]()
						{
							if (FMath::IsNearlyZero(InitialScale.X) || FMath::IsNearlyZero(InitialScale.Y))
							{
								return false;
							}

							if (!IsValid(TextureUV))
							{
								return false;
							}

							return TextureUV->bLinkScale;
						};

						if (IsScaledLinked())
						{
							switch (InComponent)
							{
								case 0: // X
									CurrentScale.Y = (InNewValue / InitialScale.X) * InitialScale.Y;
									break;

								case 1: // Y
									CurrentScale.X = (InNewValue / InitialScale.Y) * InitialScale.X;
									break;

								default:
									checkNoEntry();
							}
						}

						switch (InComponent)
						{
							case 0: // X
								CurrentScale.X = InNewValue;
								break;

							case 1: // Y
								CurrentScale.Y = InNewValue;
								break;

							default:
								checkNoEntry();
						}

						TextureUV->SetScale(CurrentScale);
					}
				}
			}
		}
	};
}

TSharedRef<SWidget> UE::DynamicMaterialEditor::Private::CreateScaleValueWidget(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV)
{
	return SNew(UE::DynamicMaterialEditor::Private::SDMPropertyEditVectorScale, InComponentEditWidget, InTextureUV);
}

void UE::DynamicMaterialEditor::Private::AddMirrorRow(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV, TArray<FDMPropertyHandle>& InOutPropertyRows)
{
	InOutPropertyRows.Add(SDMEditor::GetPropertyHandle(&*InComponentEditWidget, InTextureUV, UDMTextureUV::NAME_bMirrorOnX));
	InOutPropertyRows.Add(SDMEditor::GetPropertyHandle(&*InComponentEditWidget, InTextureUV, UDMTextureUV::NAME_bMirrorOnY));
}

TSharedRef<SWidget> UE::DynamicMaterialEditor::Private::CreateMirrorExtensionButtons(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMTextureUV* InTextureUV)
{
	if (!IsValid(InTextureUV))
	{
		return SNullWidget::NullWidget;
	}

	FProperty* XProperty = InTextureUV->GetClass()->FindPropertyByName(UDMTextureUV::NAME_bMirrorOnX);
	FProperty* YProperty = InTextureUV->GetClass()->FindPropertyByName(UDMTextureUV::NAME_bMirrorOnY);

	if (!XProperty || !YProperty)
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<IPropertyHandle> XPropertyHandle = InTextureUV->GetPropertyHandle(UDMTextureUV::NAME_bMirrorOnX);
	TSharedPtr<IPropertyHandle> YPropertyHandle = InTextureUV->GetPropertyHandle(UDMTextureUV::NAME_bMirrorOnY);
	TSharedPtr<IDetailTreeNode> XDetailTreeNode = InTextureUV->GetDetailTreeNode(UDMTextureUV::NAME_bMirrorOnX);
	TSharedPtr<IDetailTreeNode> YDetailTreeNode = InTextureUV->GetDetailTreeNode(UDMTextureUV::NAME_bMirrorOnY);

	if (!XPropertyHandle.IsValid() || !YPropertyHandle.IsValid() || !XDetailTreeNode.IsValid() || !YDetailTreeNode.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	ensure(XPropertyHandle->GetProperty() == XProperty && YPropertyHandle->GetProperty() == YProperty);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	TSharedRef<SWidget> ResetButtonWidget = PropertyCustomizationHelpers::MakeResetButton(
		FSimpleDelegate::CreateLambda([XPropertyHandle, YPropertyHandle]()
			{
				if (XPropertyHandle.IsValid() && XPropertyHandle->IsValidHandle()
					&& YPropertyHandle.IsValid() && YPropertyHandle->IsValidHandle())
				{
					TArray<UObject*> Outers;
					XPropertyHandle->GetOuterObjects(Outers);

					if (Outers.IsEmpty() == false)
					{
						if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(Outers[0]))
						{
							if (TextureUV->GetMirrorOnX() || TextureUV->GetMirrorOnY())
							{
								FScopedTransaction Transaction(LOCTEXT("ResetMirror", "Reset Texture UV Mirror to default."));
								TextureUV->Modify();
								TextureUV->SetMirrorOnX(false);
								TextureUV->SetMirrorOnY(false);
							}
						}
					}
				}
			})
	);
	ResetButtonWidget->SetVisibility(
		TAttribute<EVisibility>::CreateLambda([XPropertyHandle, YPropertyHandle]()
			{
				if (XPropertyHandle.IsValid() && XPropertyHandle->IsValidHandle()
					&& YPropertyHandle.IsValid() && YPropertyHandle->IsValidHandle())
				{
					TArray<UObject*> Outers;
					XPropertyHandle->GetOuterObjects(Outers);

					if (Outers.IsEmpty() == false)
					{
						if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(Outers[0]))
						{
							return (TextureUV->GetMirrorOnX() || TextureUV->GetMirrorOnY())
								? EVisibility::Visible
								: EVisibility::Hidden;
						}
					}
				}

				return EVisibility::Hidden;
			})
	);

	TSharedRef<SHorizontalBox> ButtonsBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			ResetButtonWidget
		];

	ButtonsBox->AddSlot()
		[
			SNew(SBox)
			.WidthOverride(20.f)
			.HeightOverride(18.f)
		];

	return ButtonsBox;
}

#undef LOCTEXT_NAMESPACE
