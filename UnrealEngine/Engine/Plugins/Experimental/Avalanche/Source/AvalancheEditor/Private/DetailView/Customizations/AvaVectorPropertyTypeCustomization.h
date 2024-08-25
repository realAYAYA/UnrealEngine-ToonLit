// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "AvaVectorPropertyTypeCustomization.generated.h"

class IAvaViewportClient;
class SButton;
class SComboButton;

class FAvaVectorPropertyTypeIdentifier : public IPropertyTypeIdentifier
{
public:
	virtual bool IsPropertyTypeCustomized(const IPropertyHandle& InPropertyHandle) const override
	{
		static const TArray<FName> PropertyMetaTags = { 
			TEXT("AllowPreserveRatio"), 
			TEXT("VectorRatioMode"), 
			TEXT("Delta")
		};

		for (const FName& MetaTag : PropertyMetaTags)
		{
			if (InPropertyHandle.HasMetaData(MetaTag))
			{
				return true;
			}
		}

		return false;
	}
};

UENUM()
enum class ERatioMode : uint8
{
	None = 0,                        // Free
	X = 1 << 0,
	Y = 1 << 1,
	Z = 1 << 2,
	PreserveXY = X | Y,              // Lock XY
	PreserveYZ = Y | Z,              // Lock YZ  (3D)
	PreserveXZ = X | Z,              // Lock XZ  (3D)
	PreserveXYZ = X | Y | Z          // Lock XYZ (3D)
};

ENUM_CLASS_FLAGS(ERatioMode)

class FAvaVectorPropertyTypeCustomization : public IPropertyTypeCustomization
{
public:
	using SNumericVectorInputBox2D = SNumericVectorInputBox<double, UE::Math::TVector2<double>, 2>;
	using SNumericVectorInputBox3D = SNumericVectorInputBox<FVector::FReal, UE::Math::TVector<FVector::FReal>, 3>;

	static constexpr uint8 MULTI_OBJECT_DEBOUNCE  = 3;
	static constexpr uint8 SINGLE_OBJECT_DEBOUNCE = 2;
	static constexpr uint8 INVALID_COMPONENT_IDX  = 5;

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FAvaVectorPropertyTypeCustomization>();
	}

	explicit FAvaVectorPropertyTypeCustomization()
	{
	}

	// BEGIN IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// END IPropertyTypeCustomization interface
	
protected:
	TWeakPtr<IAvaViewportClient> ViewportClient;

	// used to close the dropdown menu
	TSharedPtr<SComboButton> ComboButton;
	
	TSharedPtr<IPropertyHandle> VectorPropertyHandle;
	TSharedPtr<IPropertyHandle> XPropertyHandle;
	TSharedPtr<IPropertyHandle> YPropertyHandle;
	TSharedPtr<IPropertyHandle> ZPropertyHandle;

	// optional begin values to compute ratios change
	TArray<TOptional<FVector>> Begin3DValues;
	TArray<TOptional<FVector2D>> Begin2DValues;
	
	int32 SelectedObjectNum = 0;
	uint8 DebounceValueSet = 0;
	uint8 LastComponentValueSet = INDEX_NONE;
	bool bMovingSlider = false;
	bool bIsVector3d = false;
	ERatioMode RatioMode = ERatioMode::None;
	// specific case to handle that needs conversion
	bool bPixelSizeProperty = false;

	// optional clamp values
	TOptional<FVector> MinVectorClamp;
	TOptional<FVector> MaxVectorClamp;
	TOptional<FVector2D> MinVector2DClamp;
	TOptional<FVector2D> MaxVector2DClamp;

	TOptional<double> GetVectorComponent(const uint8 Component) const;
	void SetVectorComponent(double NewValue, const uint8 Component);
	void SetVectorComponent(double NewValue, ETextCommit::Type CommitType, const uint8 Component);

	void OnBeginSliderMovement();
	void OnEndSliderMovement(double NewValue);

	FReply OnComboButtonClicked(const ERatioMode NewMode);
	const FSlateBrush* GetComboButtonBrush(const ERatioMode Mode) const;
	FText GetComboButtonText(const ERatioMode Mode) const;
	const FSlateBrush* GetCurrentComboButtonBrush() const;
	FText GetCurrentComboButtonText() const;

	bool CanEditValue() const;
	void InitVectorValuesForRatio();
	void ResetVectorValuesForRatio();

	void SetComponentValue(const double NewValue, const uint8 Component, const EPropertyValueSetFlags::Type Flags);

	// get the correct clamped ratio if a component hit min/max value
	double GetClampedRatioValueChange(const int32 ObjectIdx, const double NewValue, const uint8 Component, const TArray<bool>& PreserveRatios) const;
	// get the new clamped value if a component original value is zero since ratio * 0 = 0
	double GetClampedComponentValue(const int32 ObjectIdx, double NewValue, const double Ratio, const uint8 ComponentIdx, const uint8 OriginalComponent);

	// special case for the pixel property only available in editor
	double MeshSizeToPixelSize(double MeshSize) const;
	double PixelSizeToMeshSize(double PixelSize) const;
};
