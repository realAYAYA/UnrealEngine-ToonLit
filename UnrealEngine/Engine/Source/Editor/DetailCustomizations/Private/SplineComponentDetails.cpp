// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineComponentDetails.h"

#include "BlueprintEditor.h"
#include "BlueprintEditorModule.h"
#include "ComponentVisualizer.h"
#include "ComponentVisualizerManager.h"
#include "Components/SplineComponent.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/EnumAsByte.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Blueprint.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMisc.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomNodeBuilder.h"
#include "Input/Reply.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Clipping.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "LevelEditorViewport.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/Axis.h"
#include "Math/InterpCurve.h"
#include "Math/InterpCurvePoint.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/VectorRegister.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/MessageDialog.h"
#include "Misc/Optional.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Serialization/Archive.h"
#include "SlotBase.h"
#include "SplineComponentVisualizer.h"
#include "SplineMetadataDetailsFactory.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Textures/SlateIcon.h"
#include "Trace/Detail/Channel.h"
#include "Types/SlateEnums.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UnrealEdGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

class FObjectInitializer;
class IDetailGroup;
class SWidget;

#define LOCTEXT_NAMESPACE "SplineComponentDetails"
DEFINE_LOG_CATEGORY_STATIC(LogSplineComponentDetails, Log, All)

USplineMetadataDetailsFactoryBase::USplineMetadataDetailsFactoryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

class FSplinePointDetails : public IDetailCustomNodeBuilder, public TSharedFromThis<FSplinePointDetails>
{
public:
	FSplinePointDetails(USplineComponent* InOwningSplineComponent);

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

	static bool bAlreadyWarnedInvalidIndex;

private:

	template <typename T>
	struct TSharedValue
	{
		TSharedValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		void Add(T InValue)
		{
			if (!bInitialized)
			{
				Value = InValue;
				bInitialized = true;
			}
			else
			{
				if (Value.IsSet() && InValue != Value.GetValue()) { Value.Reset(); }
			}
		}

		TOptional<T> Value;
		bool bInitialized;
	};

	struct FSharedVectorValue
	{
		FSharedVectorValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		bool IsValid() const { return bInitialized; }

		void Add(const FVector& V)
		{
			if (!bInitialized)
			{
				X = V.X;
				Y = V.Y;
				Z = V.Z;
				bInitialized = true;
			}
			else
			{
				if (X.IsSet() && V.X != X.GetValue()) { X.Reset(); }
				if (Y.IsSet() && V.Y != Y.GetValue()) { Y.Reset(); }
				if (Z.IsSet() && V.Z != Z.GetValue()) { Z.Reset(); }
			}
		}

		TOptional<float> X;
		TOptional<float> Y;
		TOptional<float> Z;
		bool bInitialized;
	};

	struct FSharedRotatorValue
	{
		FSharedRotatorValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		bool IsValid() const { return bInitialized; }

		void Add(const FRotator& R)
		{
			if (!bInitialized)
			{
				Roll = R.Roll;
				Pitch = R.Pitch;
				Yaw = R.Yaw;
				bInitialized = true;
			}
			else
			{
				if (Roll.IsSet() && R.Roll != Roll.GetValue()) { Roll.Reset(); }
				if (Pitch.IsSet() && R.Pitch != Pitch.GetValue()) { Pitch.Reset(); }
				if (Yaw.IsSet() && R.Yaw != Yaw.GetValue()) { Yaw.Reset(); }
			}
		}

		TOptional<float> Roll;
		TOptional<float> Pitch;
		TOptional<float> Yaw;
		bool bInitialized;
	};

	EVisibility IsEnabled() const { return (SelectedKeys.Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed; }
	EVisibility IsDisabled() const { return (SelectedKeys.Num() == 0) ? EVisibility::Visible : EVisibility::Collapsed; }
	bool IsOnePointSelected() const { return SelectedKeys.Num() == 1; }
	bool ArePointsSelected() const { return (SelectedKeys.Num() > 0); };
	bool AreNoPointsSelected() const { return (SelectedKeys.Num() == 0); };
	TOptional<float> GetInputKey() const { return InputKey.Value; }
	TOptional<float> GetPositionX() const { return Position.X; }
	TOptional<float> GetPositionY() const { return Position.Y; }
	TOptional<float> GetPositionZ() const { return Position.Z; }
	TOptional<float> GetArriveTangentX() const { return ArriveTangent.X; }
	TOptional<float> GetArriveTangentY() const { return ArriveTangent.Y; }
	TOptional<float> GetArriveTangentZ() const { return ArriveTangent.Z; }
	TOptional<float> GetLeaveTangentX() const { return LeaveTangent.X; }
	TOptional<float> GetLeaveTangentY() const { return LeaveTangent.Y; }
	TOptional<float> GetLeaveTangentZ() const { return LeaveTangent.Z; }
	TOptional<float> GetRotationRoll() const { return Rotation.Roll; }
	TOptional<float> GetRotationPitch() const { return Rotation.Pitch; }
	TOptional<float> GetRotationYaw() const { return Rotation.Yaw; }
	TOptional<float> GetScaleX() const { return Scale.X; }
	TOptional<float> GetScaleY() const { return Scale.Y; }
	TOptional<float> GetScaleZ() const { return Scale.Z; }
	void OnSetInputKey(float NewValue, ETextCommit::Type CommitInfo);
	void OnSetPosition(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	void OnSetArriveTangent(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	void OnSetLeaveTangent(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	void OnSetRotation(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	void OnSetScale(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis);
	FText GetPointType() const;
	void OnSplinePointTypeChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> OnGenerateComboWidget(TSharedPtr<FString> InComboString);

	void GenerateSplinePointSelectionControls(IDetailChildrenBuilder& ChildrenBuilder);
	FReply OnSelectFirstLastSplinePoint(bool bFirst);
	FReply OnSelectPrevNextSplinePoint(bool bNext, bool bAddToSelection);
	FReply OnSelectAllSplinePoints();

	USplineComponent* GetSplineComponentToVisualize() const;

	void UpdateValues();

	enum class ESplinePointProperty
	{
		Location,
		Rotation,
		Scale,
		ArriveTangent,
		LeaveTangent
	};

	TSharedRef<SWidget> BuildSplinePointPropertyLabel(ESplinePointProperty SplinePointProp);
	void OnSetTransformEditingAbsolute(ESplinePointProperty SplinePointProp, bool bIsAbsolute);
	bool IsTransformEditingAbsolute(ESplinePointProperty SplinePointProperty) const;
	bool IsTransformEditingRelative(ESplinePointProperty SplinePointProperty) const;
	FText GetSplinePointPropertyText(ESplinePointProperty SplinePointProp) const;
	void SetSplinePointProperty(ESplinePointProperty SplinePointProp, FVector NewValue, EAxisList::Type Axis, bool bCommitted);

	FUIAction CreateCopyAction(ESplinePointProperty SplinePointProp);
	FUIAction CreatePasteAction(ESplinePointProperty SplinePointProp);

	bool OnCanCopy(ESplinePointProperty SplinePointProp) const { return true; }
	void OnCopy(ESplinePointProperty SplinePointProp);
	void OnPaste(ESplinePointProperty SplinePointProp);

	void OnPasteFromText(const FString& InTag, const FString& InText, const TOptional<FGuid>& InOperationId, ESplinePointProperty SplinePointProp);
	void PasteFromText(const FString& InTag, const FString& InText, ESplinePointProperty SplinePointProp);

	void OnBeginPositionSlider();
	void OnBeginScaleSlider();
	void OnEndSlider(float);

	USplineComponent* SplineComp;
	USplineComponent* SplineCompArchetype;
	TSet<int32> SelectedKeys;

	TSharedValue<float> InputKey;
	FSharedVectorValue Position;
	FSharedVectorValue ArriveTangent;
	FSharedVectorValue LeaveTangent;
	FSharedVectorValue Scale;
	FSharedRotatorValue Rotation;
	TSharedValue<ESplinePointType::Type> PointType;

	TSharedPtr<FSplineComponentVisualizer> SplineVisualizer;
	FProperty* SplineCurvesProperty;
	TArray<TSharedPtr<FString>> SplinePointTypes;
	TSharedPtr<ISplineMetadataDetails> SplineMetaDataDetails;
	FSimpleDelegate OnRegenerateChildren;

	bool bEditingLocationAbsolute = false;
	bool bEditingRotationAbsolute = false;

	bool bInSliderTransaction = false;
};

bool FSplinePointDetails::bAlreadyWarnedInvalidIndex = false;

FSplinePointDetails::FSplinePointDetails(USplineComponent* InOwningSplineComponent)
	: SplineComp(nullptr)
{
	TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(InOwningSplineComponent->GetClass());
	SplineVisualizer = StaticCastSharedPtr<FSplineComponentVisualizer>(Visualizer);
	check(SplineVisualizer.IsValid());

	SplineCurvesProperty = FindFProperty<FProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves));

	const TArray<ESplinePointType::Type> EnabledSplinePointTypes = InOwningSplineComponent->GetEnabledSplinePointTypes();

	UEnum* SplinePointTypeEnum = StaticEnum<ESplinePointType::Type>();
	check(SplinePointTypeEnum);
	for (int32 EnumIndex = 0; EnumIndex < SplinePointTypeEnum->NumEnums() - 1; ++EnumIndex)
	{
		const int32 Value = SplinePointTypeEnum->GetValueByIndex(EnumIndex);
		if (EnabledSplinePointTypes.Contains(Value))
		{
			SplinePointTypes.Add(MakeShareable(new FString(SplinePointTypeEnum->GetNameStringByIndex(EnumIndex))));
		}
	}

	check(InOwningSplineComponent);
	if (InOwningSplineComponent->IsTemplate())
	{
		// For blueprints, SplineComp will be set to the preview actor in UpdateValues().
		SplineComp = nullptr;
		SplineCompArchetype = InOwningSplineComponent;
	}
	else
	{
		SplineComp = InOwningSplineComponent;
		SplineCompArchetype = nullptr;
	}

	bAlreadyWarnedInvalidIndex = false;
}

void FSplinePointDetails::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

void FSplinePointDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
}

void FSplinePointDetails::GenerateSplinePointSelectionControls(IDetailChildrenBuilder& ChildrenBuilder)
{
	FMargin ButtonPadding(2.f, 0.f);

	ChildrenBuilder.AddCustomRow(LOCTEXT("SelectSplinePoints", "Select Spline Points"))
	.RowTag("SelectSplinePoints")
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("SelectSplinePoints", "Select Spline Points"))
	]
	.ValueContent()
	.VAlign(VAlign_Fill)
	.MaxDesiredWidth(170.f)
	.MinDesiredWidth(170.f)
	[
		SNew(SHorizontalBox)
		.Clipping(EWidgetClipping::ClipToBounds)

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectFirst")
			.ContentPadding(2.0f)
			.ToolTipText(LOCTEXT("SelectFirstSplinePointToolTip", "Select first spline point."))
			.OnClicked(this, &FSplinePointDetails::OnSelectFirstLastSplinePoint, true)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.AddPrev")
			.ContentPadding(2.f)
			.ToolTipText(LOCTEXT("SelectAddPrevSplinePointToolTip", "Add previous spline point to current selection."))
			.OnClicked(this, &FSplinePointDetails::OnSelectPrevNextSplinePoint, false, true)
			.IsEnabled(this, &FSplinePointDetails::ArePointsSelected)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectPrev")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectPrevSplinePointToolTip", "Select previous spline point."))
			.OnClicked(this, &FSplinePointDetails::OnSelectPrevNextSplinePoint, false, false)
			.IsEnabled(this, &FSplinePointDetails::ArePointsSelected)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectAll")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectAllSplinePointToolTip", "Select all spline points."))
			.OnClicked(this, &FSplinePointDetails::OnSelectAllSplinePoints)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectNext")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectNextSplinePointToolTip", "Select next spline point."))
			.OnClicked(this, &FSplinePointDetails::OnSelectPrevNextSplinePoint, true, false)
			.IsEnabled(this, &FSplinePointDetails::ArePointsSelected)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.AddNext")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectAddNextSplinePointToolTip", "Add next spline point to current selection."))
			.OnClicked(this, &FSplinePointDetails::OnSelectPrevNextSplinePoint, true, true)
			.IsEnabled(this, &FSplinePointDetails::ArePointsSelected)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ButtonPadding)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SplineComponentDetails.SelectLast")
			.ContentPadding(2.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("SelectLastSplinePointToolTip", "Select last spline point."))
			.OnClicked(this, &FSplinePointDetails::OnSelectFirstLastSplinePoint, false)
		]
	];
}

void FSplinePointDetails::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	// Select spline point buttons
	GenerateSplinePointSelectionControls(ChildrenBuilder);

	// Message which is shown when no points are selected
	ChildrenBuilder.AddCustomRow(LOCTEXT("NoneSelected", "None selected"))
		.RowTag(TEXT("NoneSelected"))
		.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsDisabled))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoPointsSelected", "No spline points are selected."))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		];

	if (!SplineComp)
	{
		return;
	}

	// Input key
	ChildrenBuilder.AddCustomRow(LOCTEXT("InputKey", "Input Key"))
		.RowTag(TEXT("InputKey"))
		.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("InputKey", "Input Key"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	.ValueContent()
		.MinDesiredWidth(125.0f)
		.MaxDesiredWidth(125.0f)
		[
			SNew(SNumericEntryBox<float>)
			.IsEnabled(TAttribute<bool>(this, &FSplinePointDetails::IsOnePointSelected))
			.Value(this, &FSplinePointDetails::GetInputKey)
			.UndeterminedString(LOCTEXT("Multiple", "Multiple"))
			.OnValueCommitted(this, &FSplinePointDetails::OnSetInputKey)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	IDetailCategoryBuilder& ParentCategory = ChildrenBuilder.GetParentCategory();
	TSharedPtr<FOnPasteFromText> PasteFromTextDelegate = ParentCategory.OnPasteFromText();
	const bool bUsePasteFromText = PasteFromTextDelegate.IsValid();	

	// Position
	if (SplineComp->AllowsSpinePointLocationEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FSplinePointDetails::OnPasteFromText, ESplinePointProperty::Location);
		
		ChildrenBuilder.AddCustomRow(LOCTEXT("Location", "Location"))
			.RowTag(TEXT("Location"))
			.CopyAction(CreateCopyAction(ESplinePointProperty::Location))
			.PasteAction(CreatePasteAction(ESplinePointProperty::Location))
			.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				BuildSplinePointPropertyLabel(ESplinePointProperty::Location)
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SVectorInputBox)
				.X(this, &FSplinePointDetails::GetPositionX)
				.Y(this, &FSplinePointDetails::GetPositionY)
				.Z(this, &FSplinePointDetails::GetPositionZ)
				.AllowSpin(true)
				.bColorAxisLabels(true)
				.SpinDelta(1.f)
				.OnXChanged(this, &FSplinePointDetails::OnSetPosition, ETextCommit::Default, EAxis::X)
				.OnYChanged(this, &FSplinePointDetails::OnSetPosition, ETextCommit::Default, EAxis::Y)
				.OnZChanged(this, &FSplinePointDetails::OnSetPosition, ETextCommit::Default, EAxis::Z)
				.OnXCommitted(this, &FSplinePointDetails::OnSetPosition, EAxis::X)
				.OnYCommitted(this, &FSplinePointDetails::OnSetPosition, EAxis::Y)
				.OnZCommitted(this, &FSplinePointDetails::OnSetPosition, EAxis::Z)
				.OnBeginSliderMovement(this, &FSplinePointDetails::OnBeginPositionSlider)
				.OnEndSliderMovement(this, &FSplinePointDetails::OnEndSlider)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// Rotation
	if (SplineComp->AllowsSplinePointRotationEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FSplinePointDetails::OnPasteFromText, ESplinePointProperty::Rotation);	
		
		ChildrenBuilder.AddCustomRow(LOCTEXT("Rotation", "Rotation"))
			.RowTag(TEXT("Rotation"))
			.CopyAction(CreateCopyAction(ESplinePointProperty::Rotation))
			.PasteAction(CreatePasteAction(ESplinePointProperty::Rotation))
			.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				BuildSplinePointPropertyLabel(ESplinePointProperty::Rotation)
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SRotatorInputBox)
				.Roll(this, &FSplinePointDetails::GetRotationRoll)
				.Pitch(this, &FSplinePointDetails::GetRotationPitch)
				.Yaw(this, &FSplinePointDetails::GetRotationYaw)
				.AllowSpin(false)
				.bColorAxisLabels(false)
				.OnRollCommitted(this, &FSplinePointDetails::OnSetRotation, EAxis::X)
				.OnPitchCommitted(this, &FSplinePointDetails::OnSetRotation, EAxis::Y)
				.OnYawCommitted(this, &FSplinePointDetails::OnSetRotation, EAxis::Z)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// Scale
	if (SplineComp->AllowsSplinePointScaleEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FSplinePointDetails::OnPasteFromText, ESplinePointProperty::Scale);
		
		ChildrenBuilder.AddCustomRow(LOCTEXT("Scale", "Scale"))
			.RowTag(TEXT("Scale"))
			.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
			.CopyAction(CreateCopyAction(ESplinePointProperty::Scale))
			.PasteAction(CreatePasteAction(ESplinePointProperty::Scale))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ScaleLabel", "Scale"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SVectorInputBox)
				.X(this, &FSplinePointDetails::GetScaleX)
			.Y(this, &FSplinePointDetails::GetScaleY)
			.Z(this, &FSplinePointDetails::GetScaleZ)
			.AllowSpin(true)
			.bColorAxisLabels(true)
			.OnXChanged(this, &FSplinePointDetails::OnSetScale, ETextCommit::Default, EAxis::X)
			.OnYChanged(this, &FSplinePointDetails::OnSetScale, ETextCommit::Default, EAxis::Y)
			.OnZChanged(this, &FSplinePointDetails::OnSetScale, ETextCommit::Default, EAxis::Z)
			.OnXCommitted(this, &FSplinePointDetails::OnSetScale, EAxis::X)
			.OnYCommitted(this, &FSplinePointDetails::OnSetScale, EAxis::Y)
			.OnZCommitted(this, &FSplinePointDetails::OnSetScale, EAxis::Z)
			.OnBeginSliderMovement(this, &FSplinePointDetails::OnBeginScaleSlider)
			.OnEndSliderMovement(this, &FSplinePointDetails::OnEndSlider)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// ArriveTangent
	if (SplineComp->AllowsSplinePointArriveTangentEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FSplinePointDetails::OnPasteFromText, ESplinePointProperty::ArriveTangent);
	
		ChildrenBuilder.AddCustomRow(LOCTEXT("ArriveTangent", "Arrive Tangent"))
			.RowTag(TEXT("ArriveTangent"))
			.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
			.CopyAction(CreateCopyAction(ESplinePointProperty::ArriveTangent))
			.PasteAction(CreatePasteAction(ESplinePointProperty::ArriveTangent))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ArriveTangent", "Arrive Tangent"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SVectorInputBox)
				.X(this, &FSplinePointDetails::GetArriveTangentX)
				.Y(this, &FSplinePointDetails::GetArriveTangentY)
				.Z(this, &FSplinePointDetails::GetArriveTangentZ)
				.AllowSpin(false)
				.bColorAxisLabels(false)
				.OnXCommitted(this, &FSplinePointDetails::OnSetArriveTangent, EAxis::X)
				.OnYCommitted(this, &FSplinePointDetails::OnSetArriveTangent, EAxis::Y)
				.OnZCommitted(this, &FSplinePointDetails::OnSetArriveTangent, EAxis::Z)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// LeaveTangent
	if (SplineComp->AllowsSplinePointLeaveTangentEditing())
	{
		PasteFromTextDelegate->AddSP(this, &FSplinePointDetails::OnPasteFromText, ESplinePointProperty::LeaveTangent);
	
		ChildrenBuilder.AddCustomRow(LOCTEXT("LeaveTangent", "Leave Tangent"))
			.RowTag(TEXT("LeaveTangent"))
			.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
			.CopyAction(CreateCopyAction(ESplinePointProperty::LeaveTangent))
			.PasteAction(CreatePasteAction(ESplinePointProperty::LeaveTangent))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LeaveTangent", "Leave Tangent"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		.ValueContent()
			.MinDesiredWidth(375.0f)
			.MaxDesiredWidth(375.0f)
			[
				SNew(SVectorInputBox)
				.X(this, &FSplinePointDetails::GetLeaveTangentX)
				.Y(this, &FSplinePointDetails::GetLeaveTangentY)
				.Z(this, &FSplinePointDetails::GetLeaveTangentZ)
				.AllowSpin(false)
				.bColorAxisLabels(false)
				.OnXCommitted(this, &FSplinePointDetails::OnSetLeaveTangent, EAxis::X)
				.OnYCommitted(this, &FSplinePointDetails::OnSetLeaveTangent, EAxis::Y)
				.OnZCommitted(this, &FSplinePointDetails::OnSetLeaveTangent, EAxis::Z)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	// Type
	if (SplineComp->GetEnabledSplinePointTypes().Num() > 1)
	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("Type", "Type"))
			.RowTag(TEXT("Type"))
			.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Type", "Type"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		.ValueContent()
			.MinDesiredWidth(125.0f)
			.MaxDesiredWidth(125.0f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&SplinePointTypes)
				.OnGenerateWidget(this, &FSplinePointDetails::OnGenerateComboWidget)
				.OnSelectionChanged(this, &FSplinePointDetails::OnSplinePointTypeChanged)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(this, &FSplinePointDetails::GetPointType)
				]
			];
	}

	if (SplineVisualizer.IsValid() && SplineVisualizer->GetSelectedKeys().Num() > 0)
	{
		for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
		{
			if (ClassIterator->IsChildOf(USplineMetadataDetailsFactoryBase::StaticClass()) && !ClassIterator->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				USplineMetadataDetailsFactoryBase* Factory = ClassIterator->GetDefaultObject<USplineMetadataDetailsFactoryBase>();
				const USplineMetadata* SplineMetadata = SplineComp->GetSplinePointsMetadata();
				if (SplineMetadata && SplineMetadata->GetClass() == Factory->GetMetadataClass())
				{
					SplineMetaDataDetails = Factory->Create();
					IDetailGroup& Group = ChildrenBuilder.AddGroup(SplineMetaDataDetails->GetName(), SplineMetaDataDetails->GetDisplayName());
					SplineMetaDataDetails->GenerateChildContent(Group);
					break;
				}
			}
		}
	}
}

void FSplinePointDetails::Tick(float DeltaTime)
{
	UpdateValues();
}

void FSplinePointDetails::UpdateValues()
{
	// If this is a blueprint spline, always update the spline component based on 
	// the spline component visualizer's currently edited spline component.
	if (SplineCompArchetype)
	{
		USplineComponent* EditedSplineComp = SplineVisualizer.IsValid() ? SplineVisualizer->GetEditedSplineComponent() : nullptr;

		if (!EditedSplineComp || (EditedSplineComp->GetArchetype() != SplineCompArchetype))
		{
			return;
		}

		SplineComp = EditedSplineComp;
	}

	if (!SplineComp || !SplineVisualizer.IsValid())
	{
		return;
	}

	bool bNeedsRebuild = false;
	const TSet<int32>& NewSelectedKeys = SplineVisualizer->GetSelectedKeys();

	if (NewSelectedKeys.Num() != SelectedKeys.Num())
	{
		bNeedsRebuild = true;
	}
	SelectedKeys = NewSelectedKeys;

	// Cache values to be shown by the details customization.
	// An unset optional value represents 'multiple values' (in the case where multiple points are selected).
	InputKey.Reset();
	Position.Reset();
	ArriveTangent.Reset();
	LeaveTangent.Reset();
	Rotation.Reset();
	Scale.Reset();
	PointType.Reset();

	// Only display point details when there are selected keys
	if (SelectedKeys.Num() > 0)
	{
		bool bValidIndices = true;
		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 ||
				Index >= SplineComp->GetSplinePointsPosition().Points.Num() ||
				Index >= SplineComp->GetSplinePointsRotation().Points.Num() ||
				Index >= SplineComp->GetSplinePointsScale().Points.Num())
			{
				bValidIndices = false;
				if (!bAlreadyWarnedInvalidIndex)
				{
					UE_LOG(LogSplineComponentDetails, Error, TEXT("Spline component details selected keys contains invalid index %d for spline %s with %d points, %d rotations, %d scales"),
						Index,
						*SplineComp->GetPathName(),
						SplineComp->GetSplinePointsPosition().Points.Num(),
						SplineComp->GetSplinePointsRotation().Points.Num(),
						SplineComp->GetSplinePointsScale().Points.Num());
					bAlreadyWarnedInvalidIndex = true;
				}
				break;
			}
		}

		if (bValidIndices)
		{
			for (int32 Index : SelectedKeys)
			{
				const FTransform SplineToWorld = SplineComp->GetComponentToWorld();

				if (bEditingLocationAbsolute)
				{
					const FVector AbsoluteLocation = SplineToWorld.TransformPosition(SplineComp->GetSplinePointsPosition().Points[Index].OutVal);
					Position.Add(AbsoluteLocation);
				}
				else
				{
					Position.Add(SplineComp->GetSplinePointsPosition().Points[Index].OutVal);
				}

				if (bEditingRotationAbsolute)
				{
					const FQuat AbsoluteRotation = SplineToWorld.TransformRotation(SplineComp->GetSplinePointsRotation().Points[Index].OutVal);
					Rotation.Add(AbsoluteRotation.Rotator());
				}
				else
				{
					Rotation.Add(SplineComp->GetSplinePointsRotation().Points[Index].OutVal.Rotator());
				}

				InputKey.Add(SplineComp->GetSplinePointsPosition().Points[Index].InVal);
				Scale.Add(SplineComp->GetSplinePointsScale().Points[Index].OutVal);
				ArriveTangent.Add(SplineComp->GetSplinePointsPosition().Points[Index].ArriveTangent);
				LeaveTangent.Add(SplineComp->GetSplinePointsPosition().Points[Index].LeaveTangent);
				PointType.Add(ConvertInterpCurveModeToSplinePointType(SplineComp->GetSplinePointsPosition().Points[Index].InterpMode));
			}

			if (SplineMetaDataDetails)
			{
				SplineMetaDataDetails->Update(SplineComp, SelectedKeys);
			}
		}
	}

	if (bNeedsRebuild)
	{
		OnRegenerateChildren.ExecuteIfBound();
	}
}

FName FSplinePointDetails::GetName() const
{
	static const FName Name("SplinePointDetails");
	return Name;
}

void FSplinePointDetails::OnSetInputKey(float NewValue, ETextCommit::Type CommitInfo)
{
	if ((CommitInfo != ETextCommit::OnEnter && CommitInfo != ETextCommit::OnUserMovedFocus) || !SplineComp)
	{
		return;
	}

	check(SelectedKeys.Num() == 1);
	const int32 Index = *SelectedKeys.CreateConstIterator();
	TArray<FInterpCurvePoint<FVector>>& Positions = SplineComp->GetSplinePointsPosition().Points;

	const int32 NumPoints = Positions.Num();

	bool bModifyOtherPoints = false;
	if ((Index > 0 && NewValue <= Positions[Index - 1].InVal) ||
		(Index < NumPoints - 1 && NewValue >= Positions[Index + 1].InVal))
	{
		const FText Title(LOCTEXT("InputKeyTitle", "Input key out of range"));
		const FText Message(LOCTEXT("InputKeyMessage", "Spline input keys must be numerically ascending. Would you like to modify other input keys in the spline in order to be able to set this value?"));

		// Ensure input keys remain ascending
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message, Title) == EAppReturnType::No)
		{
			return;
		}

		bModifyOtherPoints = true;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointInputKey", "Set spline point input key"));
		SplineComp->Modify();

		TArray<FInterpCurvePoint<FQuat>>& Rotations = SplineComp->GetSplinePointsRotation().Points;
		TArray<FInterpCurvePoint<FVector>>& Scales = SplineComp->GetSplinePointsScale().Points;

		if (bModifyOtherPoints)
		{
			// Shuffle the previous or next input keys down or up so the input value remains in sequence
			if (Index > 0 && NewValue <= Positions[Index - 1].InVal)
			{
				float Delta = (NewValue - Positions[Index].InVal);
				for (int32 PrevIndex = 0; PrevIndex < Index; PrevIndex++)
				{
					Positions[PrevIndex].InVal += Delta;
					Rotations[PrevIndex].InVal += Delta;
					Scales[PrevIndex].InVal += Delta;
				}
			}
			else if (Index < NumPoints - 1 && NewValue >= Positions[Index + 1].InVal)
			{
				float Delta = (NewValue - Positions[Index].InVal);
				for (int32 NextIndex = Index + 1; NextIndex < NumPoints; NextIndex++)
				{
					Positions[NextIndex].InVal += Delta;
					Rotations[NextIndex].InVal += Delta;
					Scales[NextIndex].InVal += Delta;
				}
			}
		}

		Positions[Index].InVal = NewValue;
		Rotations[Index].InVal = NewValue;
		Scales[Index].InVal = NewValue;
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->PostEditMove(true);
	}
	UpdateValues();

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplinePointDetails::OnSetPosition(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointPosition", "Set spline point position"), !bInSliderTransaction);
		SplineComp->Modify();

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsPosition().Points.Num())
			{
				UE_LOG(LogSplineComponentDetails, Error, TEXT("Set spline point location: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsPosition().Points.Num());
				continue;
			}

			if (bEditingLocationAbsolute)
			{
				const FTransform SplineToWorld = SplineComp->GetComponentToWorld();
				const FVector RelativePos = SplineComp->GetSplinePointsPosition().Points[Index].OutVal;
				FVector AbsolutePos = SplineToWorld.TransformPosition(RelativePos);
				AbsolutePos.SetComponentForAxis(Axis, NewValue);
				FVector PointPosition = SplineToWorld.InverseTransformPosition(AbsolutePos);

				SplineComp->GetSplinePointsPosition().Points[Index].OutVal = PointPosition;
			}
			else
			{
				FVector PointPosition = SplineComp->GetSplinePointsPosition().Points[Index].OutVal;
				PointPosition.SetComponentForAxis(Axis, NewValue);
				SplineComp->GetSplinePointsPosition().Points[Index].OutVal = PointPosition;
			}
		}
	}

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplinePointDetails::OnSetArriveTangent(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointTangent", "Set spline point tangent"));
		SplineComp->Modify();

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsPosition().Points.Num())
			{
				UE_LOG(LogSplineComponentDetails, Error, TEXT("Set spline point arrive tangent: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsPosition().Points.Num());
				continue;
			}

			FVector PointTangent = SplineComp->GetSplinePointsPosition().Points[Index].ArriveTangent;
			PointTangent.SetComponentForAxis(Axis, NewValue);
			SplineComp->GetSplinePointsPosition().Points[Index].ArriveTangent = PointTangent;
			SplineComp->GetSplinePointsPosition().Points[Index].InterpMode = CIM_CurveUser;
		}
	}

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplinePointDetails::OnSetLeaveTangent(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointTangent", "Set spline point tangent"));
		SplineComp->Modify();

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsPosition().Points.Num())
			{
				UE_LOG(LogSplineComponentDetails, Error, TEXT("Set spline point leave tangent: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsPosition().Points.Num());
				continue;
			}

			FVector PointTangent = SplineComp->GetSplinePointsPosition().Points[Index].LeaveTangent;
			PointTangent.SetComponentForAxis(Axis, NewValue);
			SplineComp->GetSplinePointsPosition().Points[Index].LeaveTangent = PointTangent;
			SplineComp->GetSplinePointsPosition().Points[Index].InterpMode = CIM_CurveUser;
		}
	}

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplinePointDetails::OnSetRotation(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}
	
	FQuat NewRotationRelative;
	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointRotation", "Set spline point rotation"));
		SplineComp->Modify();
		FQuat SplineComponentRotation = SplineComp->GetComponentQuat();
		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsRotation().Points.Num())
			{
				UE_LOG(LogSplineComponentDetails, Error, TEXT("Set spline point rotation: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsRotation().Points.Num());
				continue;
			}

			FInterpCurvePoint<FVector>& EditedPoint = SplineComp->GetSplinePointsPosition().Points[Index];
			FInterpCurvePoint<FQuat>& EditedRotPoint = SplineComp->GetSplinePointsRotation().Points[Index];
			const FQuat CurrentRotationRelative = EditedRotPoint.OutVal;

			if (bEditingRotationAbsolute)
			{
				FRotator AbsoluteRot = (SplineComponentRotation * CurrentRotationRelative).Rotator();

				switch (Axis)
				{
				case EAxis::X: AbsoluteRot.Roll = NewValue; break;
				case EAxis::Y: AbsoluteRot.Pitch = NewValue; break;
				case EAxis::Z: AbsoluteRot.Yaw = NewValue; break;
				}

				NewRotationRelative = SplineComponentRotation.Inverse() * AbsoluteRot.Quaternion();
			}
			else
			{
				FRotator NewRotationRotator(CurrentRotationRelative);

				switch (Axis)
				{
				case EAxis::X: NewRotationRotator.Roll = NewValue; break;
				case EAxis::Y: NewRotationRotator.Pitch = NewValue; break;
				case EAxis::Z: NewRotationRotator.Yaw = NewValue; break;
				}

				NewRotationRelative = NewRotationRotator.Quaternion();
			}

			SplineComp->GetSplinePointsRotation().Points[Index].OutVal = NewRotationRelative;

			FQuat DeltaRotate(NewRotationRelative * CurrentRotationRelative.Inverse());
			// Rotate tangent according to delta rotation
			FVector NewTangent = SplineComponentRotation.RotateVector(EditedPoint.LeaveTangent); // convert local-space tangent vector to world-space
			NewTangent = DeltaRotate.RotateVector(NewTangent); // apply world-space delta rotation to world-space tangent
			NewTangent = SplineComponentRotation.Inverse().RotateVector(NewTangent); // convert world-space tangent vector back into local-space
			EditedPoint.LeaveTangent = NewTangent;
			EditedPoint.ArriveTangent = NewTangent;
		}
	}

	SplineVisualizer->SetCachedRotation(NewRotationRelative);

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}
	GEditor->RedrawLevelEditingViewports(true);
}

void FSplinePointDetails::OnSetScale(float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointScale", "Set spline point scale"));
		SplineComp->Modify();

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsScale().Points.Num())
			{
				UE_LOG(LogSplineComponentDetails, Error, TEXT("Set spline point scale: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsScale().Points.Num());
				continue;
			}

			FVector PointScale = SplineComp->GetSplinePointsScale().Points[Index].OutVal;
			PointScale.SetComponentForAxis(Axis, NewValue);
			SplineComp->GetSplinePointsScale().Points[Index].OutVal = PointScale;
		}
	}

	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;
		FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
		UpdateValues();
	}

	GEditor->RedrawLevelEditingViewports(true);
}

FText FSplinePointDetails::GetPointType() const
{
	if (PointType.Value.IsSet())
	{
		const UEnum* SplinePointTypeEnum = StaticEnum<ESplinePointType::Type>();
		check(SplinePointTypeEnum);
		return SplinePointTypeEnum->GetDisplayNameTextByValue(PointType.Value.GetValue());
	}

	return LOCTEXT("MultipleTypes", "Multiple Types");
}

void FSplinePointDetails::OnSplinePointTypeChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (!SplineComp)
	{
		return;
	}

	// Scope the transaction to only include the value change and none of the derived data changes that might arise from NotifyPropertyModified
	{
		const FScopedTransaction Transaction(LOCTEXT("SetSplinePointType", "Set spline point type"));
		SplineComp->Modify();

		EInterpCurveMode Mode = CIM_Unknown;
		if (NewValue.IsValid() && SplinePointTypes.Contains(NewValue))
		{
			const UEnum* SplinePointTypeEnum = StaticEnum<ESplinePointType::Type>();
			check(SplinePointTypeEnum);
			const int64 SplinePointType = SplinePointTypeEnum->GetValueByNameString(*NewValue);

			Mode = ConvertSplinePointTypeToInterpCurveMode(static_cast<ESplinePointType::Type>(SplinePointType));
		}

		for (int32 Index : SelectedKeys)
		{
			if (Index < 0 || Index >= SplineComp->GetSplinePointsPosition().Points.Num())
			{
				UE_LOG(LogSplineComponentDetails, Error, TEXT("Set spline point type: invalid index %d in selected points for spline component %s which contains %d spline points."),
					Index, *SplineComp->GetPathName(), SplineComp->GetSplinePointsPosition().Points.Num());
				continue;
			}

			SplineComp->GetSplinePointsPosition().Points[Index].InterpMode = Mode;
		}
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->PostEditMove(true);
	}
	UpdateValues();

	GEditor->RedrawLevelEditingViewports(true);
}

USplineComponent* FSplinePointDetails::GetSplineComponentToVisualize() const
{
	if (SplineCompArchetype) 
	{
		check(SplineCompArchetype->IsTemplate());

		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");

		const UClass* BPClass;
		if (const AActor* OwningCDO = SplineCompArchetype->GetOwner())
		{
			// Native component template
			BPClass = OwningCDO->GetClass();
		}
		else
		{
			// Non-native component template
			BPClass = Cast<UClass>(SplineCompArchetype->GetOuter());
		}

		if (BPClass)
		{
			if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(BPClass))
			{
				if (FBlueprintEditor* BlueprintEditor = StaticCast<FBlueprintEditor*>(GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Blueprint, false)))
				{
					const AActor* PreviewActor = BlueprintEditor->GetPreviewActor();
					TArray<UObject*> Instances;
					SplineCompArchetype->GetArchetypeInstances(Instances);

					for (UObject* Instance : Instances)
					{
						USplineComponent* SplineCompInstance = Cast<USplineComponent>(Instance);
						if (SplineCompInstance->GetOwner() == PreviewActor)
						{
							return SplineCompInstance;
						}
					}
				}
			}
		}

		// If we failed to find an archetype instance, must return nullptr 
		// since component visualizer cannot visualize the archetype.
		return nullptr;
	}

	return SplineComp;
}

FReply FSplinePointDetails::OnSelectFirstLastSplinePoint(bool bFirst)
{
	if (SplineVisualizer.IsValid())
	{
		bool bActivateComponentVis = false;

		if (!SplineComp)
		{
			SplineComp = GetSplineComponentToVisualize();
			bActivateComponentVis = true;
		}

		if (SplineComp)
		{
			if (SplineVisualizer->HandleSelectFirstLastSplinePoint(SplineComp, bFirst))
			{
				if (bActivateComponentVis)
				{
					TSharedPtr<FComponentVisualizer> Visualizer = StaticCastSharedPtr<FComponentVisualizer>(SplineVisualizer);
					GUnrealEd->ComponentVisManager.SetActiveComponentVis(GCurrentLevelEditingViewportClient, Visualizer);
				}
			}
		}
	}
	return FReply::Handled();
}

FReply FSplinePointDetails::OnSelectPrevNextSplinePoint(bool bNext, bool bAddToSelection)
{
	if (SplineVisualizer.IsValid())
	{
		SplineVisualizer->OnSelectPrevNextSplinePoint(bNext, bAddToSelection);
	}
	return FReply::Handled();
}

FReply FSplinePointDetails::OnSelectAllSplinePoints()
{
	if (SplineVisualizer.IsValid())
	{
		bool bActivateComponentVis = false;

		if (!SplineComp)
		{
			SplineComp = GetSplineComponentToVisualize();
			bActivateComponentVis = true;
		}

		if (SplineComp)
		{
			if (SplineVisualizer->HandleSelectAllSplinePoints(SplineComp))
			{
				if (bActivateComponentVis)
				{
					TSharedPtr<FComponentVisualizer> Visualizer = StaticCastSharedPtr<FComponentVisualizer>(SplineVisualizer);
					GUnrealEd->ComponentVisManager.SetActiveComponentVis(GCurrentLevelEditingViewportClient, Visualizer);
				}
			}
		}
	}
	return FReply::Handled();
}

TSharedRef<SWidget> FSplinePointDetails::OnGenerateComboWidget(TSharedPtr<FString> InComboString)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InComboString))
		.Font(IDetailLayoutBuilder::GetDetailFont());
}

TSharedRef<SWidget> FSplinePointDetails::BuildSplinePointPropertyLabel(ESplinePointProperty SplinePointProp)
{
	FText Label;
	switch (SplinePointProp)
	{
	case ESplinePointProperty::Rotation:
		Label = LOCTEXT("RotationLabel", "Rotation");
		break;
	case ESplinePointProperty::Location:
		Label = LOCTEXT("LocationLabel", "Location");
		break;
	default:
		return SNullWidget::NullWidget;
	}

	FMenuBuilder MenuBuilder(true, NULL, NULL);

	FUIAction SetRelativeLocationAction
	(
		FExecuteAction::CreateSP(this, &FSplinePointDetails::OnSetTransformEditingAbsolute, SplinePointProp, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplinePointDetails::IsTransformEditingRelative, SplinePointProp)
	);

	FUIAction SetWorldLocationAction
	(
		FExecuteAction::CreateSP(this, &FSplinePointDetails::OnSetTransformEditingAbsolute, SplinePointProp, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplinePointDetails::IsTransformEditingAbsolute, SplinePointProp)
	);

	MenuBuilder.BeginSection(TEXT("TransformType"), FText::Format(LOCTEXT("TransformType", "{0} Type"), Label));

	MenuBuilder.AddMenuEntry
	(
		FText::Format(LOCTEXT("RelativeLabel", "Relative"), Label),
		FText::Format(LOCTEXT("RelativeLabel_ToolTip", "{0} is relative to its parent"), Label),
		FSlateIcon(),
		SetRelativeLocationAction,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry
	(
		FText::Format(LOCTEXT("WorldLabel", "World"), Label),
		FText::Format(LOCTEXT("WorldLabel_ToolTip", "{0} is relative to the world"), Label),
		FSlateIcon(),
		SetWorldLocationAction,
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.EndSection();


	return
		SNew(SComboButton)
		.ContentPadding(0)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ForegroundColor(FSlateColor::UseForeground())
		.MenuContent()
		[
			MenuBuilder.MakeWidget()
		]
	.ButtonContent()
		[
			SNew(SBox)
			.Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(this, &FSplinePointDetails::GetSplinePointPropertyText, SplinePointProp)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		];
}

void FSplinePointDetails::OnSetTransformEditingAbsolute(ESplinePointProperty SplinePointProp, bool bIsAbsolute)
{
	if (SplinePointProp == ESplinePointProperty::Location)
	{
		bEditingLocationAbsolute = bIsAbsolute;
	}
	else if (SplinePointProp == ESplinePointProperty::Rotation)
	{
		bEditingRotationAbsolute = bIsAbsolute;
	}
	else
	{
		return;
	}

	UpdateValues();
}

bool FSplinePointDetails::IsTransformEditingAbsolute(ESplinePointProperty SplinePointProp) const
{
	if (SplinePointProp == ESplinePointProperty::Location)
	{
		return bEditingLocationAbsolute;
	}
	else if (SplinePointProp == ESplinePointProperty::Rotation)
	{
		return bEditingRotationAbsolute;
	}

	return false;
}

bool FSplinePointDetails::IsTransformEditingRelative(ESplinePointProperty SplinePointProp) const
{
	if (SplinePointProp == ESplinePointProperty::Location)
	{
		return !bEditingLocationAbsolute;
	}
	else if (SplinePointProp == ESplinePointProperty::Rotation)
	{
		return !bEditingRotationAbsolute;
	}

	return false;
}


FText FSplinePointDetails::GetSplinePointPropertyText(ESplinePointProperty SplinePointProp) const
{
	if (SplinePointProp == ESplinePointProperty::Location)
	{
		return bEditingLocationAbsolute ? LOCTEXT("AbsoluteLocation", "Absolute Location") : LOCTEXT("Location", "Location");
	}
	else if (SplinePointProp == ESplinePointProperty::Rotation)
	{
		return bEditingRotationAbsolute ? LOCTEXT("AbsoluteRotation", "Absolute Rotation") : LOCTEXT("Rotation", "Rotation");
	}

	return FText::GetEmpty();
}

void FSplinePointDetails::SetSplinePointProperty(ESplinePointProperty SplinePointProp, FVector NewValue, EAxisList::Type Axis, bool bCommitted)
{
	switch (SplinePointProp)
	{
	case ESplinePointProperty::Location:
		OnSetPosition(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetPosition(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetPosition(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	case ESplinePointProperty::Rotation:
		OnSetRotation(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetRotation(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetRotation(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	case ESplinePointProperty::Scale:
		OnSetScale(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetScale(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetScale(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	case ESplinePointProperty::ArriveTangent:
		OnSetArriveTangent(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetArriveTangent(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetArriveTangent(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	case ESplinePointProperty::LeaveTangent:
		OnSetLeaveTangent(NewValue.X, ETextCommit::Default, EAxis::X);
		OnSetLeaveTangent(NewValue.Y, ETextCommit::Default, EAxis::Y);
		OnSetLeaveTangent(NewValue.Z, ETextCommit::OnEnter, EAxis::Z);
		break;
	default:
		break;
	}
}

FUIAction FSplinePointDetails::CreateCopyAction(ESplinePointProperty SplinePointProp)
{
	return
		FUIAction
		(
			FExecuteAction::CreateSP(this, &FSplinePointDetails::OnCopy, SplinePointProp),
			FCanExecuteAction::CreateSP(this, &FSplinePointDetails::OnCanCopy, SplinePointProp)
		);
}

FUIAction FSplinePointDetails::CreatePasteAction(ESplinePointProperty SplinePointProp)
{
	return
		FUIAction(FExecuteAction::CreateSP(this, &FSplinePointDetails::OnPaste, SplinePointProp));
}

void FSplinePointDetails::OnCopy(ESplinePointProperty SplinePointProp)
{
	FString CopyStr;
	switch (SplinePointProp)
	{
	case ESplinePointProperty::Location:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), Position.X.GetValue(), Position.Y.GetValue(), Position.Z.GetValue());
		break;
	case ESplinePointProperty::Rotation:
		CopyStr = FString::Printf(TEXT("(Pitch=%f,Yaw=%f,Roll=%f)"), Rotation.Pitch.GetValue(), Rotation.Yaw.GetValue(), Rotation.Roll.GetValue());
		break;
	case ESplinePointProperty::Scale:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), Scale.X.GetValue(), Scale.Y.GetValue(), Scale.Z.GetValue());
		break;
	case ESplinePointProperty::ArriveTangent:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), ArriveTangent.X.GetValue(), ArriveTangent.Y.GetValue(), ArriveTangent.Z.GetValue());
		break;
	case ESplinePointProperty::LeaveTangent:
		CopyStr = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), LeaveTangent.X.GetValue(), LeaveTangent.Y.GetValue(), LeaveTangent.Z.GetValue());
		break;
	default:
		break;
	}

	if (!CopyStr.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
	}
}

void FSplinePointDetails::OnPaste(ESplinePointProperty SplinePointProp)
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	PasteFromText(TEXT(""), PastedText, SplinePointProp);
}

void FSplinePointDetails::OnPasteFromText(
	const FString& InTag,
	const FString& InText,
	const TOptional<FGuid>& InOperationId,
	ESplinePointProperty SplinePointProp)
{
	PasteFromText(InTag, InText, SplinePointProp);
}

void FSplinePointDetails::PasteFromText(
	const FString& InTag,
	const FString& InText,
	ESplinePointProperty SplinePointProp)
{
	FString PastedText = InText;
	switch (SplinePointProp)
	{
	case ESplinePointProperty::Location:
		{
			FVector NewLocation;
			if (NewLocation.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteLocation", "Paste Location"));
				SetSplinePointProperty(ESplinePointProperty::Location, NewLocation, EAxisList::All, true);
			}
			break;
		}
	case ESplinePointProperty::Rotation:
		{
			FVector NewRotation;
			PastedText.ReplaceInline(TEXT("Pitch="), TEXT("X="));
			PastedText.ReplaceInline(TEXT("Yaw="), TEXT("Y="));
			PastedText.ReplaceInline(TEXT("Roll="), TEXT("Z="));
			if (NewRotation.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteRotation", "Paste Rotation"));
				SetSplinePointProperty(ESplinePointProperty::Rotation, NewRotation, EAxisList::All, true);
			}
			break;
		}
	case ESplinePointProperty::Scale:
		{
			FVector NewScale;
			if (NewScale.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteScale", "Paste Scale"));
				SetSplinePointProperty(ESplinePointProperty::Scale, NewScale, EAxisList::All, true);
			}
			break;
		}
	case ESplinePointProperty::ArriveTangent:
		{
			FVector NewArrive;
			if (NewArrive.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteArriveTangent", "Paste Arrive Tangent"));
				SetSplinePointProperty(ESplinePointProperty::ArriveTangent, NewArrive, EAxisList::All, true);
			}
			break;
		}
	case ESplinePointProperty::LeaveTangent:
		{
			FVector NewLeave;
			if (NewLeave.InitFromString(PastedText))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteLeaveTangent", "Paste Leave Tangent"));
				SetSplinePointProperty(ESplinePointProperty::LeaveTangent, NewLeave, EAxisList::All, true);
			}
			break;
		}
	default:
		break;
	}
}

void FSplinePointDetails::OnBeginPositionSlider()
{
	bInSliderTransaction = true;
	SplineComp->Modify();
	GEditor->BeginTransaction(LOCTEXT("SetSplinePointPosition", "Set spline point position"));
}

void FSplinePointDetails::OnBeginScaleSlider()
{
	bInSliderTransaction = true;
	SplineComp->Modify();
	GEditor->BeginTransaction(LOCTEXT("SetSplinePointScale", "Set spline point scale"));
}

void FSplinePointDetails::OnEndSlider(float)
{
	bInSliderTransaction = false;
	GEditor->EndTransaction();
}

////////////////////////////////////

TSharedRef<IDetailCustomization> FSplineComponentDetails::MakeInstance()
{
	return MakeShareable(new FSplineComponentDetails);
}

void FSplineComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide the SplineCurves property
	TSharedPtr<IPropertyHandle> SplineCurvesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves));
	SplineCurvesProperty->MarkHiddenByCustomization();


	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	if (ObjectsBeingCustomized.Num() == 1)
	{
		if (USplineComponent* SplineComp = Cast<USplineComponent>(ObjectsBeingCustomized[0]))
		{
			// Set the spline points details as important in order to have it on top 
			IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Selected Points", FText::GetEmpty(), ECategoryPriority::Important);
			TSharedRef<FSplinePointDetails> SplinePointDetails = MakeShareable(new FSplinePointDetails(SplineComp));
			Category.AddCustomBuilder(SplinePointDetails);
		}
	}
}

#undef LOCTEXT_NAMESPACE
