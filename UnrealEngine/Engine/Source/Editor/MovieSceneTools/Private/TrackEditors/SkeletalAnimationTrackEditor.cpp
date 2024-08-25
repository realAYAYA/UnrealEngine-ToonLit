// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/SkeletalAnimationTrackEditor.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetData.h"
#include "Animation/AnimSequenceBase.h"
#include "Modules/ModuleManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "SequencerSectionPainter.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "SequencerSettings.h"
#include "MVVM/Views/ViewUtilities.h"
#include "MVVM/ViewModels/ViewDensity.h"
#include "ISectionLayoutBuilder.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "Animation/MirrorDataTable.h"
#include "Styling/AppStyle.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "MovieSceneTimeHelpers.h"
#include "SequencerTimeSliderController.h"
#include "FrameNumberDisplayFormat.h"
#include "FrameNumberNumericInterface.h"
#include "AnimationBlueprintLibrary.h"
#include "AnimationEditorUtils.h"
#include "Factories/PoseAssetFactory.h"
#include "Misc/MessageDialog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Blueprint.h"

#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "PropertyEditorModule.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"
#include "IPropertyUtilities.h"
#include "Factories/AnimSequenceFactory.h"
#include "MovieSceneToolHelpers.h"

#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Exporters/AnimSeqExportOption.h"

#include "EditModes/SkeletalAnimationTrackEditMode.h"
#include "EditorModeManager.h"

#include "LevelSequence.h"
#include "LevelSequenceAnimSequenceLink.h"
#include "AnimSequenceLevelSequenceLink.h"
#include "UObject/SavePackage.h"
#include "AnimSequencerInstanceProxy.h"
#include "TimeToPixel.h"
#include "SequencerAnimationOverride.h"

int32 FSkeletalAnimationTrackEditor::NumberActive = 0;

namespace SkeletalAnimationEditorConstants
{
	// @todo Sequencer Allow this to be customizable
	const uint32 AnimationTrackHeight = 28;
}

#define LOCTEXT_NAMESPACE "FSkeletalAnimationTrackEditor"


class SAnimSequenceOptionsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAnimSequenceOptionsWindow)
		: _ExportOptions(nullptr)
		, _WidgetWindow()
		, _FullPath()
	{}

	SLATE_ARGUMENT(UAnimSeqExportOption*, ExportOptions)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
		SLATE_ARGUMENT(FText, FullPath)
		SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	FReply OnExport()
	{
		bShouldExport = true;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}


	FReply OnCancel()
	{
		bShouldExport = false;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return OnCancel();
		}

		return FReply::Unhandled();
	}

	bool ShouldExport() const
	{
		return bShouldExport;
	}

	
	SAnimSequenceOptionsWindow()
		: ExportOptions(nullptr)
		, bShouldExport(false)
	{}

private:

	FReply OnResetToDefaultClick() const;

private:
	UAnimSeqExportOption* ExportOptions;
	TSharedPtr<class IDetailsView> DetailsView;
	TWeakPtr< SWindow > WidgetWindow;
	bool			bShouldExport;
};


void SAnimSequenceOptionsWindow::Construct(const FArguments& InArgs)
{
	ExportOptions = InArgs._ExportOptions;
	WidgetWindow = InArgs._WidgetWindow;

	check(ExportOptions);

	FText CancelText =  LOCTEXT("AnimSequenceOptions_Cancel", "Cancel");
	FText CancelTooltipText = LOCTEXT("AnimSequenceOptions_Cancel_ToolTip", "Cancel the current Anim Sequence Creation.");

	TSharedPtr<SBox> HeaderToolBox;
	TSharedPtr<SHorizontalBox> AnimHeaderButtons;
	TSharedPtr<SBox> InspectorBox;
	this->ChildSlot
		[
			SNew(SBox)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SAssignNew(HeaderToolBox, SBox)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("CurveEd.LabelFont"))
		.Text(LOCTEXT("Export_CurrentFileTitle", "Current File: "))
		]
	+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("CurveEd.InfoFont"))
		.Text(InArgs._FullPath)
		]
		]
		]
	+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2)
		[
			SAssignNew(InspectorBox, SBox)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(2)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(2)
	   + SUniformGridPanel::Slot(1, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("AnimExportOptionsWindow_Export", "Export To Animation Sequence"))
			.OnClicked(this, &SAnimSequenceOptionsWindow::OnExport)
		]
	   + SUniformGridPanel::Slot(2, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
		.Text(CancelText)
		.ToolTipText(CancelTooltipText)
		.OnClicked(this, &SAnimSequenceOptionsWindow::OnCancel)
		]
		]
			]
		];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InspectorBox->SetContent(DetailsView->AsShared());

	HeaderToolBox->SetContent(
		SNew(SBorder)
		.Padding(FMargin(3))
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
		[
			SAssignNew(AnimHeaderButtons, SHorizontalBox)
			+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(2.0f, 0.0f))
		[
			SNew(SButton)
			.Text(LOCTEXT("AnimSequenceOptions_ResetOptions", "Reset to Default"))
		.OnClicked(this, &SAnimSequenceOptionsWindow::OnResetToDefaultClick)
		]
		]
		]
		]
	);

	DetailsView->SetObject(ExportOptions);
}

FReply SAnimSequenceOptionsWindow::OnResetToDefaultClick() const
{
	ExportOptions->ResetToDefault();
	//Refresh the view to make sure the custom UI are updating correctly
	DetailsView->SetObject(ExportOptions, true);
	return FReply::Handled();
}


USkeletalMeshComponent* AcquireSkeletalMeshFromObjectGuid(const FGuid& Guid, TSharedPtr<ISequencer> SequencerPtr)
{
	UObject* BoundObject = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(Guid) : nullptr;

	if (AActor* Actor = Cast<AActor>(BoundObject))
	{
		if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Actor->GetRootComponent()))
		{
			return SkeletalMeshComponent;
		}

		TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		Actor->GetComponents(SkeletalMeshComponents);
		
		if (SkeletalMeshComponents.Num() == 1)
		{
			return SkeletalMeshComponents[0];
		}
	}
	else if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(BoundObject))
	{
		if (SkeletalMeshComponent->GetSkeletalMeshAsset())
		{
			return SkeletalMeshComponent;
		}
	}

	return nullptr;
}

USkeleton* GetSkeletonFromComponent(UActorComponent* InComponent)
{
	USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(InComponent);
	if (SkeletalMeshComp && SkeletalMeshComp->GetSkeletalMeshAsset() && SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton())
	{
		// @todo Multiple actors, multiple components
		return SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton();
	}

	return nullptr;
}

// Get the skeletal mesh components from the guid
// If bGetSingleRootComponent - return only the root component if it is a skeletal mesh component. 
// This allows the root object binding to have an animation track without needing a skeletal mesh component binding
//
TArray<USkeletalMeshComponent*> AcquireSkeletalMeshComponentsFromObjectGuid(const FGuid& Guid, TSharedPtr<ISequencer> SequencerPtr, const bool bGetSingleRootComponent = true)
{
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;

	UObject* BoundObject = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(Guid) : nullptr;

	AActor* Actor = Cast<AActor>(BoundObject);

	if (!Actor)
	{
		if (UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(BoundObject))
		{
			Actor = ChildActorComponent->GetChildActor();
		}
	}

	if (Actor)
	{
		if (bGetSingleRootComponent)
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Actor->GetRootComponent()))
			{
				SkeletalMeshComponents.Add(SkeletalMeshComponent);
				return SkeletalMeshComponents;
			}
		}

		Actor->GetComponents(SkeletalMeshComponents);
		if (SkeletalMeshComponents.Num())
		{
			return SkeletalMeshComponents;
		}

		AActor* ActorCDO = Cast<AActor>(Actor->GetClass()->GetDefaultObject());
		if (ActorCDO)
		{
			if (bGetSingleRootComponent)
			{
				if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ActorCDO->GetRootComponent()))
				{
					SkeletalMeshComponents.Add(SkeletalMeshComponent);
					return SkeletalMeshComponents;
				}
			}

			ActorCDO->GetComponents(SkeletalMeshComponents);
			if (SkeletalMeshComponents.Num())
			{
				return SkeletalMeshComponents;
			}
		}

		UBlueprintGeneratedClass* ActorBlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(Actor->GetClass());
		if (ActorBlueprintGeneratedClass)
		{
			const TArray<USCS_Node*>& ActorBlueprintNodes = ActorBlueprintGeneratedClass->SimpleConstructionScript->GetAllNodes();

			for (USCS_Node* Node : ActorBlueprintNodes)
			{
				if (Node->ComponentClass->IsChildOf(USkeletalMeshComponent::StaticClass()))
				{
					if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Node->GetActualComponentTemplate(ActorBlueprintGeneratedClass)))
					{
						SkeletalMeshComponents.Add(SkeletalMeshComponent);
					}
				}
			}

			if (SkeletalMeshComponents.Num())
			{
				return SkeletalMeshComponents;
			}
		}
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(BoundObject))
	{
		SkeletalMeshComponents.Add(SkeletalMeshComponent);
		return SkeletalMeshComponents;
	}
	
	return SkeletalMeshComponents;
}

USkeleton* AcquireSkeletonFromObjectGuid(const FGuid& Guid, TSharedPtr<ISequencer> SequencerPtr)
{
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents = AcquireSkeletalMeshComponentsFromObjectGuid(Guid, SequencerPtr);

	if (SkeletalMeshComponents.Num() == 1)
	{
		return GetSkeletonFromComponent(SkeletalMeshComponents[0]);
	}

	return nullptr;
}


class FMovieSceneSkeletalAnimationParamsDetailCustomization : public IPropertyTypeCustomization
{
public:
	FMovieSceneSkeletalAnimationParamsDetailCustomization(const FSequencerSectionPropertyDetailsViewCustomizationParams& InParams)
		: Params(InParams)
	{
		if (Params.ParentObjectBindingGuid.IsValid())
		{
			if (USkeletalMeshComponent* SkelMeshComp = AcquireSkeletalMeshFromObjectGuid(Params.ParentObjectBindingGuid, Params.Sequencer))
			{
				TScriptInterface<ISequencerAnimationOverride> SequencerAnimOverride = ISequencerAnimationOverride::GetSequencerAnimOverride(SkelMeshComp);
				if (SequencerAnimOverride.GetObject())
				{
					bAllowsCinematicOverride = ISequencerAnimationOverride::Execute_AllowsCinematicOverride(SequencerAnimOverride.GetObject());
					SlotNameOptions = ISequencerAnimationOverride::Execute_GetSequencerAnimSlotNames(SequencerAnimOverride.GetObject());
					bShowSlotNameOptions = SlotNameOptions.Num() > 0 && !bAllowsCinematicOverride;
				}
			}
		}
	}

	// IDetailCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		SlotNameProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSkeletalAnimationParams, SlotName));
	}

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		const FName AnimationPropertyName = GET_MEMBER_NAME_CHECKED(FMovieSceneSkeletalAnimationParams, Animation);
		const FName MirrorDataTableName = GET_MEMBER_NAME_CHECKED(FMovieSceneSkeletalAnimationParams, MirrorDataTable);
		const FName SlotNamePropertyName = GET_MEMBER_NAME_CHECKED(FMovieSceneSkeletalAnimationParams, SlotName);


		uint32 NumChildren;
		PropertyHandle->GetNumChildren(NumChildren);
		for (uint32 i = 0; i < NumChildren; ++i)
		{
			TSharedPtr<IPropertyHandle> ChildPropertyHandle = PropertyHandle->GetChildHandle(i);
			IDetailPropertyRow& ChildPropertyRow = ChildBuilder.AddProperty(ChildPropertyHandle.ToSharedRef());
			FName ChildPropertyName = ChildPropertyHandle->GetProperty()->GetFName();
			// Let most properties be whatever they want to be... we just want to customize the `Animation` and `MirrorDataTable` properties
			// by making it look like a normal asset reference property, but with some custom filtering.
			if (ChildPropertyName == AnimationPropertyName || ChildPropertyName == MirrorDataTableName)
			{
				FDetailWidgetRow& Row = ChildPropertyRow.CustomWidget();

				if (Params.ParentObjectBindingGuid.IsValid())
				{
					// Store the compatible skeleton's name, and create a property widget with a filter that will check
					// for animations that match that skeleton.
					Skeleton = AcquireSkeletonFromObjectGuid(Params.ParentObjectBindingGuid, Params.Sequencer);
					SkeletonName = FAssetData(Skeleton).GetExportTextName();

					TSharedPtr<IPropertyUtilities> PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
					UClass* AllowedStaticClass = ChildPropertyName == AnimationPropertyName ? UAnimSequenceBase::StaticClass() : UMirrorDataTable::StaticClass(); 

					TSharedRef<SObjectPropertyEntryBox> ContentWidget = SNew(SObjectPropertyEntryBox)
						.PropertyHandle(ChildPropertyHandle)
						.AllowedClass(AllowedStaticClass)
						.DisplayThumbnail(true)
						.ThumbnailPool(PropertyUtilities.IsValid() ? PropertyUtilities->GetThumbnailPool() : nullptr)
						.OnShouldFilterAsset(FOnShouldFilterAsset::CreateRaw(this, &FMovieSceneSkeletalAnimationParamsDetailCustomization::ShouldFilterAsset));

					Row.NameContent()[ChildPropertyHandle->CreatePropertyNameWidget()];
					Row.ValueContent()[ContentWidget];

					float MinDesiredWidth, MaxDesiredWidth;
					ContentWidget->GetDesiredWidth(MinDesiredWidth, MaxDesiredWidth);
					Row.ValueContent().MinWidth = MinDesiredWidth;
					Row.ValueContent().MaxWidth = MaxDesiredWidth;

					// The content widget already contains a "reset to default" button, so we don't want the details view row
					// to make another one. We add this metadata on the property handle instance to suppress it.
					ChildPropertyHandle->SetInstanceMetaData(TEXT("NoResetToDefault"), TEXT("true"));
				}
			}
			else if (ChildPropertyName == SlotNamePropertyName)
			{
				if (bShowSlotNameOptions)
				{
					ChildPropertyRow.IsEnabled(TAttribute<bool>::CreateSP(this, &FMovieSceneSkeletalAnimationParamsDetailCustomization::GetCanEditSlotName));
					FDetailWidgetRow& Row = ChildPropertyRow.CustomWidget();
					Row.NameContent()[ChildPropertyHandle->CreatePropertyNameWidget()];

					Row.ValueContent()
						[
							SNew(SComboBox<FName>)
							.OptionsSource(&SlotNameOptions)
						.OnSelectionChanged(this, &FMovieSceneSkeletalAnimationParamsDetailCustomization::OnSlotNameChanged)
						.OnGenerateWidget_Lambda([](FName InSlotName) { return SNew(STextBlock).Text(FText::FromName(InSlotName)); })
						[
							SNew(STextBlock)
							.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
						.Text(this, &FMovieSceneSkeletalAnimationParamsDetailCustomization::GetSlotNameDesc)
						]
						];
				}
			}
		}
	}

	bool ShouldFilterAsset(const FAssetData& AssetData)
	{
		// Since the `SObjectPropertyEntryBox` doesn't support passing some `Filter` properties for the asset picker, 
		// we just combine the tag value filtering we want (i.e. checking the skeleton compatibility) along with the
		// other filtering we already get from the track editor's filter callback.
		FSkeletalAnimationTrackEditor& TrackEditor = static_cast<FSkeletalAnimationTrackEditor&>(Params.TrackEditor);
		if (TrackEditor.ShouldFilterAsset(AssetData))
		{
			return true;
		}

		return !(Skeleton && Skeleton->IsCompatibleForEditor(AssetData));
	}


	FText GetSlotNameDesc() const
	{
		FName NameValue;
		SlotNameProperty->GetValue(NameValue);

		return FText::FromString(NameValue.ToString());
	}

	bool GetCanEditSlotName() const
	{
		if (bShowSlotNameOptions)
		{
			FName NameValue;
			SlotNameProperty->GetValue(NameValue);
			// If we're allowing cinematic override, then the slot names are irrelevant, don't allow edit.
			// If we have less than 2 slot name options, then changing them is irrelevant, don't allow edit.
			// Always allow an edit if the current slot name isn't currently set to one of the provided ones.
			if (bAllowsCinematicOverride || (SlotNameOptions.Num() < 2 && SlotNameOptions.Contains(NameValue)))
			{
				return false;
			}
		}
		return true;
	}

	void OnSlotNameChanged(FName InSlotName, ESelectInfo::Type InInfo)
	{
		SlotNameProperty->SetValue(InSlotName);
	}

private:
	FSequencerSectionPropertyDetailsViewCustomizationParams Params;
	FString SkeletonName;
	USkeleton* Skeleton = nullptr;
	TSharedPtr<IPropertyHandle> SlotNameProperty;
	TArray<FName> SlotNameOptions;
	bool bShowSlotName = true;
	bool bShowSlotNameOptions = false;
	bool bAllowsCinematicOverride = false;
};


FSkeletalAnimationSection::FSkeletalAnimationSection( UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
	: Section(*CastChecked<UMovieSceneSkeletalAnimationSection>(&InSection))
	, Sequencer(InSequencer)
	, InitialFirstLoopStartOffsetDuringResize(0)
	, InitialStartTimeDuringResize(0)
{ 
}

void FSkeletalAnimationSection::BeginDilateSection()
{
	Section.PreviousPlayRate = Section.Params.PlayRate; //make sure to cache the play rate
}

void FSkeletalAnimationSection::DilateSection(const TRange<FFrameNumber>& NewRange, float DilationFactor)
{
	Section.Params.PlayRate = Section.PreviousPlayRate / DilationFactor;
	Section.SetRange(NewRange);
}

UMovieSceneSection* FSkeletalAnimationSection::GetSectionObject()
{ 
	return &Section;
}

FText FSkeletalAnimationSection::GetSectionTitle() const
{
	if (Section.Params.Animation != nullptr)
	{
		if (!Section.Params.MirrorDataTable)
		{
			return FText::FromString( Section.Params.Animation->GetName() );
		}
		else
		{
			return FText::Format(LOCTEXT("SectionTitleContentFormat", "{0} mirrored with {1}"), FText::FromString(Section.Params.Animation->GetName()), FText::FromString(Section.Params.MirrorDataTable->GetName()));
		}
	}
	return LOCTEXT("NoAnimationSection", "No Animation");
}

FText FSkeletalAnimationSection::GetSectionToolTip() const
{
	if (Section.Params.Animation != nullptr && Section.HasStartFrame() && Section.HasEndFrame())
	{
		UMovieScene* MovieScene = Section.GetTypedOuter<UMovieScene>();
		FFrameRate TickResolution = MovieScene->GetTickResolution();

		const float AnimPlayRate = FMath::IsNearlyZero(Section.Params.PlayRate) || Section.Params.Animation == nullptr ? 1.0f : Section.Params.PlayRate * Section.Params.Animation->RateScale;
		const float StartOffset = TickResolution.AsSeconds(Section.Params.FirstLoopStartFrameOffset);
		const float SectionLength = Section.GetRange().Size<FFrameTime>() / TickResolution;

		if (!FMath::IsNearlyZero(SectionLength, KINDA_SMALL_NUMBER) && SectionLength > 0)
		{
			return FText::Format(LOCTEXT("ToolTipContentFormat", "Start: {0}s\nDuration: {1}s\nPlay Rate:{2}"), StartOffset, SectionLength, AnimPlayRate);
		}
	}
	return FText::GetEmpty();
}

TOptional<FFrameTime> FSkeletalAnimationSection::GetSectionTime(FSequencerSectionPainter& InPainter) const
{
	if (!InPainter.bIsSelected || !Sequencer.Pin() || Section.Params.Animation == nullptr)
	{
		return TOptional<FFrameTime>();
	}

	FFrameTime CurrentTime = Sequencer.Pin()->GetLocalTime().Time;
	if (!Section.GetRange().Contains(CurrentTime.FrameNumber))
	{
		return TOptional<FFrameTime>();
	}

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();
	FFrameRate TickResolution = TimeToPixelConverter.GetTickResolution();

	// Draw the current time next to the scrub handle
	const double AnimTime = Section.MapTimeToAnimation(CurrentTime, TickResolution);
	const FFrameRate SamplingFrameRate = Section.Params.Animation->GetSamplingFrameRate();

	FQualifiedFrameTime HintFrameTime;
	if (!UAnimationBlueprintLibrary::EvaluateRootBoneTimecodeAttributesAtTime(Section.Params.Animation, static_cast<float>(AnimTime), HintFrameTime))
	{
		const FFrameTime FrameTime = SamplingFrameRate.AsFrameTime(AnimTime);
		HintFrameTime = FQualifiedFrameTime(FrameTime, SamplingFrameRate);
	}

	// Convert to tick resolution
	HintFrameTime = FQualifiedFrameTime(ConvertFrameTime(HintFrameTime.Time, SamplingFrameRate, TickResolution), TickResolution);

	// Get the desired frame display format and zero padding from
	// the sequencer settings, if possible.
	TAttribute<EFrameNumberDisplayFormats> DisplayFormatAttr(EFrameNumberDisplayFormats::Frames);
	TAttribute<uint8> ZeroPadFrameNumbersAttr(0u);
	if (const USequencerSettings* SequencerSettings = Sequencer.Pin()->GetSequencerSettings())
	{
		DisplayFormatAttr.Set(SequencerSettings->GetTimeDisplayFormat());
		ZeroPadFrameNumbersAttr.Set(SequencerSettings->GetZeroPadFrames());
	}

	// No frame rate conversion necessary since we're displaying
	// the source frame time/rate.
	const TAttribute<FFrameRate> TickResolutionAttr(HintFrameTime.Rate);
	const TAttribute<FFrameRate> DisplayRateAttr(HintFrameTime.Rate);

	FFrameNumberInterface FrameNumberInterface(DisplayFormatAttr, ZeroPadFrameNumbersAttr, TickResolutionAttr, DisplayRateAttr);

	float Subframe = 0.0f;
	if (UAnimationBlueprintLibrary::EvaluateRootBoneTimecodeSubframeAttributeAtTime(Section.Params.Animation, static_cast<float>(AnimTime), Subframe))
	{
		if (FMath::IsNearlyEqual(Subframe, FMath::RoundToFloat(Subframe)))
		{
			FrameNumberInterface.SetSubframeIndicator(FString::Printf(TEXT(" (%d)"), FMath::RoundToInt(Subframe)));
		}
		else
		{
			FrameNumberInterface.SetSubframeIndicator(FString::Printf(TEXT(" (%s)"), *LexToSanitizedString(Subframe)));
		}
	}

	return HintFrameTime.Time;
}


float FSkeletalAnimationSection::GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const
{
	return ViewDensity.UniformHeight.Get(SkeletalAnimationEditorConstants::AnimationTrackHeight);
}


FMargin FSkeletalAnimationSection::GetContentPadding() const
{
	return FMargin(8.0f, 8.0f);
}


int32 FSkeletalAnimationSection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	int32 LayerId = Painter.PaintSectionBackground();

	static const FSlateBrush* GenericDivider = FAppStyle::GetBrush("Sequencer.GenericDivider");

	if (!Section.HasStartFrame() || !Section.HasEndFrame())
	{
		return LayerId;
	}

	FFrameRate TickResolution = TimeToPixelConverter.GetTickResolution();

	// Add lines where the animation starts and ends/loops
	const float AnimPlayRate = FMath::IsNearlyZero(Section.Params.PlayRate) || Section.Params.Animation == nullptr ? 1.0f : Section.Params.PlayRate * Section.Params.Animation->RateScale;
	const float SeqLength = (Section.Params.GetSequenceLength() - TickResolution.AsSeconds(Section.Params.StartFrameOffset + Section.Params.EndFrameOffset)) / AnimPlayRate;
	const float FirstLoopSeqLength = SeqLength - TickResolution.AsSeconds(Section.Params.FirstLoopStartFrameOffset) / AnimPlayRate;

	if (!FMath::IsNearlyZero(SeqLength, KINDA_SMALL_NUMBER) && SeqLength > 0)
	{
		float MaxOffset  = Section.GetRange().Size<FFrameTime>() / TickResolution;
		float OffsetTime = FirstLoopSeqLength;
		float StartTime  = Section.GetInclusiveStartFrame() / TickResolution;

		while (OffsetTime < MaxOffset)
		{
			float OffsetPixel = TimeToPixelConverter.SecondsToPixel(StartTime + OffsetTime) - TimeToPixelConverter.SecondsToPixel(StartTime);

			FSlateDrawElement::MakeBox(
				Painter.DrawElements,
				LayerId,
				Painter.SectionGeometry.MakeChild(
					FVector2D(2.f, Painter.SectionGeometry.Size.Y-2.f),
					FSlateLayoutTransform(FVector2D(OffsetPixel, 1.f))
				).ToPaintGeometry(),
				GenericDivider,
				DrawEffects
			);

			OffsetTime += SeqLength;
		}
	}

	return LayerId;
}

void FSkeletalAnimationSection::BeginResizeSection()
{
	InitialFirstLoopStartOffsetDuringResize = Section.Params.FirstLoopStartFrameOffset;
	InitialStartTimeDuringResize = Section.HasStartFrame() ? Section.GetInclusiveStartFrame() : 0;
}

void FSkeletalAnimationSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
{
	// Adjust the start offset when resizing from the beginning
	if (ResizeMode == SSRM_LeadingEdge)
	{
		// Get the effective animation length, in frames (rounded up), after taking into account start/end trimming.
		const FFrameRate FrameRate = Section.GetTypedOuter<UMovieScene>()->GetTickResolution();
		const FFrameNumber SeqLength = FrameRate.AsFrameTime(Section.Params.GetSequenceLength()).CeilToFrame() - Section.Params.StartFrameOffset - Section.Params.EndFrameOffset;

		// Note that there is no scalar-multiplication support for frame numbers, so we need to multiply Value directly.
		FFrameNumber ResizeAmount = (ResizeTime - InitialStartTimeDuringResize);
		ResizeAmount.Value = (int32)FMath::Floor(ResizeAmount.Value * Section.Params.PlayRate);
		FFrameNumber NewFirstLoopStartFrameOffset = InitialFirstLoopStartOffsetDuringResize + ResizeAmount;

		// If the start offset exceeds the length of one loop, trim it back.
		if (SeqLength > 0)
		{
			NewFirstLoopStartFrameOffset = NewFirstLoopStartFrameOffset % SeqLength;
		}
		// If the start offset is negative, add an extra loop at the beginning by making this start offset the complement.
		if (NewFirstLoopStartFrameOffset < 0)
		{
			NewFirstLoopStartFrameOffset = SeqLength + NewFirstLoopStartFrameOffset;
		}

		Section.Params.FirstLoopStartFrameOffset = NewFirstLoopStartFrameOffset;
	}

	ISequencerSection::ResizeSection(ResizeMode, ResizeTime);
}

void FSkeletalAnimationSection::BeginSlipSection()
{
	BeginResizeSection();
}

void FSkeletalAnimationSection::SlipSection(FFrameNumber SlipTime)
{
	FFrameRate FrameRate = Section.GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber StartOffset = FrameRate.AsFrameNumber((SlipTime - InitialStartTimeDuringResize) / FrameRate * Section.Params.PlayRate);

	StartOffset += InitialFirstLoopStartOffsetDuringResize;

	if (StartOffset < 0)
	{
		// Ensure start offset is not less than 0 and adjust ResizeTime
		SlipTime = SlipTime - StartOffset;

		StartOffset = FFrameNumber(0);
	}
	else
	{
		// If the start offset exceeds the length of one loop, trim it back.
		const FFrameNumber SeqLength = FrameRate.AsFrameNumber(Section.Params.GetSequenceLength()) - Section.Params.StartFrameOffset - Section.Params.EndFrameOffset;
		StartOffset = StartOffset % SeqLength;
	}

	Section.Params.FirstLoopStartFrameOffset = StartOffset;

	ISequencerSection::SlipSection(SlipTime);
}


void FSkeletalAnimationSection::CustomizePropertiesDetailsView(TSharedRef<IDetailsView> DetailsView, const FSequencerSectionPropertyDetailsViewCustomizationParams& InParams) const
{
	DetailsView->RegisterInstancedCustomPropertyTypeLayout(
		TEXT("MovieSceneSkeletalAnimationParams"),
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([=]() { return MakeShared<FMovieSceneSkeletalAnimationParamsDetailCustomization>(InParams); }));
}

void FSkeletalAnimationSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding)
{
	// Can't pick the object that this track binds
	TSharedPtr<ISequencer> SequencerPtr = Sequencer.Pin();
	USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBinding, SequencerPtr);
	USkeletalMeshComponent* SkelMeshComp = AcquireSkeletalMeshFromObjectGuid(ObjectBinding, SequencerPtr);
	UMovieSceneSkeletalAnimationTrack* Track = Section.GetTypedOuter<UMovieSceneSkeletalAnimationTrack>();

	if (Track && Skeleton)
	{
		const int32 NumBones = Skeleton->GetReferenceSkeleton().GetNum();
		TArray<FName> BoneNames;
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			BoneNames.Add(Skeleton->GetReferenceSkeleton().GetBoneName(BoneIndex));
		}

		auto MatchToBone = [=, this](bool bMatchPrevious, int32 Index)
		{
			return FUIAction(
				FExecuteAction::CreateLambda([=, this]
					{
						FScopedTransaction MatchSection(LOCTEXT("MatchSectionByBone_Transaction", "Match Section By Bone"));
						Section.Modify();	
						Section.bMatchWithPrevious = bMatchPrevious;
						if (Index >= 0)
						{
							FName Name = Skeleton->GetReferenceSkeleton().GetBoneName(Index);
							Section.MatchSectionByBoneTransform(SkelMeshComp, SequencerPtr->GetLocalTime().Time, SequencerPtr->GetLocalTime().Rate, Name);
						}
						else
						{
							Section.ClearMatchedOffsetTransforms();
						}
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

					}
				),
				FCanExecuteAction::CreateLambda([=]()->bool 
					{ 
						return SequencerPtr.IsValid(); 
					}),
				FIsActionChecked::CreateLambda([=, this]()->bool
					{
						if (Index >= 0)
						{
							FName Name = Skeleton->GetReferenceSkeleton().GetBoneName(Index);
							return (Section.MatchedBoneName == Name);
						}
						return (Section.MatchedBoneName == NAME_None);
					})
				);
		};


		MenuBuilder.BeginSection(NAME_None, LOCTEXT("MotionBlendingOptions", "Motion Blending Options"));
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("MatchWithThisBoneInPreviousClip", "Match With This Bone In Previous Clip"), LOCTEXT("MatchWithThisBoneInPreviousClip_Tooltip", "Match This Bone With Previous Clip At Current Frame"),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder) {
					int32 Index = -1;
					FText NoNameText = LOCTEXT("TurnOffBoneMatching", "Turn Off Matching");
					FText NoNameTooltipText = LOCTEXT("TurnOffMatchingTooltip", "Turn Off Any Bone Matching");
					SubMenuBuilder.AddMenuEntry(
						NoNameText, NoNameTooltipText,
						FSlateIcon(), MatchToBone(true, Index++), NAME_None, EUserInterfaceActionType::RadioButton);
					

					for (const FName& BoneName : BoneNames)
					{
						FText Name = FText::FromName(BoneName);
						FText Text = FText::Format(LOCTEXT("BoneNameSelect", "{0}"), Name);
						FText TooltipText = FText::Format(LOCTEXT("BoneNameSelectTooltip", "Match To This Bone {0}"), Name);
						SubMenuBuilder.AddMenuEntry(
							Text, TooltipText,
							FSlateIcon(), MatchToBone(true, Index++), NAME_None, EUserInterfaceActionType::RadioButton);
					}
					}));

			MenuBuilder.AddSubMenu(
				LOCTEXT("MatchWithThisBoneInNextClip", "Match With This Bone In Next Clip"), LOCTEXT("MatchWithThisBoneInNextClip_Tooltip", "Match This Bone With Next Clip At Current Frame"),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder) {
					int32 Index = -1;
					
					FText NoNameText = LOCTEXT("TurnOffBoneMatching", "Turn Off Matching");
					FText NoNameTooltipText = LOCTEXT("TurnOffMatchingTooltip", "Turn Off Any Bone Matching");
					SubMenuBuilder.AddMenuEntry(
						NoNameText, NoNameTooltipText,
						FSlateIcon(), MatchToBone(false, Index++), NAME_None, EUserInterfaceActionType::RadioButton);
					

					for (const FName& BoneName : BoneNames)
					{
						FText Name = FText::FromName(BoneName);
						FText Text = FText::Format(LOCTEXT("BoneNameSelect", "{0}"), Name);
						FText TooltipText = FText::Format(LOCTEXT("BoneNameSelectTooltip", "Match To This Bone {0}"), Name);
						SubMenuBuilder.AddMenuEntry(
							Text, TooltipText,
							FSlateIcon(), MatchToBone(false, Index++), NAME_None, EUserInterfaceActionType::RadioButton);
					}
					}));
			MenuBuilder.AddMenuEntry(
				LOCTEXT("MatchTranslation", "Match X and Y Translation"),
				LOCTEXT("MatchTranslationTooltip", "Match the Translation to the Specified Bone"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=, this]()->void {
						FScopedTransaction MatchTransaction(LOCTEXT("MatchTranslation_Transaction", "Match Translation"));
						Section.Modify();	
						Section.ToggleMatchTranslation();
						Section.MatchSectionByBoneTransform(SkelMeshComp, SequencerPtr->GetLocalTime().Time, SequencerPtr->GetLocalTime().Rate, Section.MatchedBoneName);
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);}),
						FCanExecuteAction::CreateLambda([]()->bool { return true; }),
						FIsActionChecked::CreateLambda([this]()->bool { return Section.bMatchTranslation; })),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("MatchZHeight", "Match Z Height"),
				LOCTEXT("MatchZHeightTooltip", "Match the Z Height, may want this off for better matching"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=, this]()->void { 
						FScopedTransaction MatchTransaction(LOCTEXT("MatchZHeight_Transaction", "Match Z Height"));
						Section.Modify();
						Section.ToggleMatchIncludeZHeight(); 
						Section.MatchSectionByBoneTransform(SkelMeshComp, SequencerPtr->GetLocalTime().Time, SequencerPtr->GetLocalTime().Rate, Section.MatchedBoneName);
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged); }),
						FCanExecuteAction::CreateLambda([]()->bool { return true; }),
						FIsActionChecked::CreateLambda([this]()->bool { return Section.bMatchIncludeZHeight;  })),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("MatchYawRotation", "Match Yaw Rotation"),
				LOCTEXT("MatchYawRotationTooltip", "Match the Yaw Rotation, may want this off for better matching"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=, this]()->void {
						FScopedTransaction MatchTransaction(LOCTEXT("MatchYawRotation_Transaction", "Match Yaw Rotation"));
						Section.Modify();
						Section.ToggleMatchIncludeYawRotation();
						Section.MatchSectionByBoneTransform(SkelMeshComp, SequencerPtr->GetLocalTime().Time, SequencerPtr->GetLocalTime().Rate, Section.MatchedBoneName);
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged); }),
						FCanExecuteAction::CreateLambda([]()->bool { return true; }),
						FIsActionChecked::CreateLambda([this]()->bool { return Section.bMatchRotationYaw; })),
				NAME_None,
							EUserInterfaceActionType::ToggleButton
							);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("MatchPitchRotation", "Match Pitch Rotation"),
				LOCTEXT("MatchPitchRotationTooltip", "Match the Pitch Rotation, may want this off for better matching"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=, this]()->void { 
						FScopedTransaction MatchTransaction(LOCTEXT("MatchPitchRotation_Transaction", "Match Pitch Rotation"));
						Section.Modify();
						Section.ToggleMatchIncludePitchRotation();
						Section.MatchSectionByBoneTransform(SkelMeshComp, SequencerPtr->GetLocalTime().Time, SequencerPtr->GetLocalTime().Rate, Section.MatchedBoneName);
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged); }),
						FCanExecuteAction::CreateLambda([]()->bool { return true; }),
						FIsActionChecked::CreateLambda([this]()->bool { return Section.bMatchRotationPitch;})),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("MatchRollRotation", "Match Roll Rotation"),
				LOCTEXT("MatchRollRotationTooltip", "Match the Roll Rotation, may want this off for better matching"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=, this]()->void {
						FScopedTransaction MatchTransaction(LOCTEXT("MatchRollRotation_Transaction", "Match Roll Rotation"));
						Section.Modify();
						Section.ToggleMatchIncludeRollRotation();
						Section.MatchSectionByBoneTransform(SkelMeshComp, SequencerPtr->GetLocalTime().Time, SequencerPtr->GetLocalTime().Rate, Section.MatchedBoneName);
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged); }),
						FCanExecuteAction::CreateLambda([]()->bool { return true; }),
						FIsActionChecked::CreateLambda([this]()->bool { return Section.bMatchRotationRoll; })),
				NAME_None,
							EUserInterfaceActionType::ToggleButton
				);


			MenuBuilder.EndSection();
		}
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("SkelAnimSectionDisplay", "Display"));
		{

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("Sequencer", "ShowSkeletons", "Show Skeleton"),
				NSLOCTEXT("Sequencer", "ShowSkeletonsTooltip", "Show A Skeleton for this Section."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([this, SequencerPtr]()->void {
						Section.ToggleShowSkeleton();
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

						}),
					FCanExecuteAction::CreateLambda([=]()->bool { return SequencerPtr != nullptr; }),
						FIsActionChecked::CreateLambda([this]()->bool { return Section.bShowSkeleton; })),
						NAME_None,
						EUserInterfaceActionType::ToggleButton
					);

			MenuBuilder.EndSection();

		}
	}
}


////FSkeletalAnimationTrackEditor


bool FSkeletalAnimationTrackEditor::CreatePoseAsset(const TArray<UObject*> NewAssets, FGuid InObjectBinding)
{
	USkeletalMeshComponent* SkeletalMeshComponent = AcquireSkeletalMeshFromObjectGuid(InObjectBinding, GetSequencer());

	bool bResult = false;
	if (NewAssets.Num() > 0)
	{
		for (auto NewAsset : NewAssets)
		{
			UPoseAsset* NewPoseAsset = Cast<UPoseAsset>(NewAsset);
			if (NewPoseAsset)
			{
				NewPoseAsset->AddPoseWithUniqueName(SkeletalMeshComponent);
				bResult = true;
			}
		}

		// if it contains error, warn them
		if (bResult)
		{				
			FText NotificationText;
			if (NewAssets.Num() == 1)
			{
				NotificationText = FText::Format(LOCTEXT("NumPoseAssetsCreated", "{0} Pose assets created."), NewAssets.Num());
			}
			else
			{
				NotificationText = FText::Format(LOCTEXT("PoseAssetsCreated", "Pose asset created: '{0}'."), FText::FromString(NewAssets[0]->GetName()));
			}
						
			FNotificationInfo Info(NotificationText);	
			Info.ExpireDuration = 8.0f;	
			Info.bUseLargeFont = false;
			Info.Hyperlink = FSimpleDelegate::CreateLambda([NewAssets]()
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(NewAssets);
			});
			Info.HyperlinkText = FText::Format(LOCTEXT("OpenNewPoseAssetHyperlink", "Open {0}"), FText::FromString(NewAssets[0]->GetName()));
				
			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if ( Notification.IsValid() )
			{
				Notification->SetCompletionState( SNotificationItem::CS_Success );
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToCreateAsset", "Failed to create asset"));
		}
	}
	return bResult;
}


void FSkeletalAnimationTrackEditor::HandleCreatePoseAsset(FGuid InObjectBinding)
{
	USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(InObjectBinding, GetSequencer());
	if (Skeleton)
	{
		TArray<TSoftObjectPtr<UObject>> Skeletons;
		Skeletons.Add(Skeleton);
		AnimationEditorUtils::ExecuteNewAnimAsset<UPoseAssetFactory, UPoseAsset>(Skeletons, FString("_PoseAsset"), FAnimAssetCreated::CreateSP(this, &FSkeletalAnimationTrackEditor::CreatePoseAsset, InObjectBinding), false, false);
	}
}


FSkeletalAnimationTrackEditor::FSkeletalAnimationTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{ 
	//We use the FGCObject pattern to keep the anim export option alive during the editor session

	AnimSeqExportOption = NewObject<UAnimSeqExportOption>();
}

void FSkeletalAnimationTrackEditor::OnInitialize()
{
	SequencerSavedHandle = GetSequencer()->OnPostSave().AddRaw(this, &FSkeletalAnimationTrackEditor::OnSequencerSaved);
	SequencerChangedHandle = GetSequencer()->OnMovieSceneDataChanged().AddRaw(this, &FSkeletalAnimationTrackEditor::OnSequencerDataChanged);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FSkeletalAnimationTrackEditor::OnPostPropertyChanged);

	++FSkeletalAnimationTrackEditor::NumberActive;

	// Activate the default mode in case FEditorModeTools::Tick isn't run before here. 
	// This can be removed once a general fix for UE-143791 has been implemented.
	GLevelEditorModeTools().ActivateDefaultMode();

	GLevelEditorModeTools().ActivateMode(FSkeletalAnimationTrackEditMode::ModeName);
	FSkeletalAnimationTrackEditMode* EditMode = static_cast<FSkeletalAnimationTrackEditMode*>(GLevelEditorModeTools().GetActiveMode(FSkeletalAnimationTrackEditMode::ModeName));
	if (EditMode)
	{
		EditMode->SetSequencer(GetSequencer());
	}

}
void FSkeletalAnimationTrackEditor::OnRelease()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);

	--FSkeletalAnimationTrackEditor::NumberActive;

	if (GetSequencer().IsValid())
	{
		if (SequencerSavedHandle.IsValid())
		{
			GetSequencer()->OnPostSave().Remove(SequencerSavedHandle);
			SequencerSavedHandle.Reset();
		}
		if (SequencerChangedHandle.IsValid())
		{
			GetSequencer()->OnMovieSceneDataChanged().Remove(SequencerChangedHandle);
			SequencerChangedHandle.Reset();
		}
	}
	if (FSkeletalAnimationTrackEditor::NumberActive == 0)
	{
		GLevelEditorModeTools().DeactivateMode(FSkeletalAnimationTrackEditMode::ModeName);
	}

}

void FSkeletalAnimationTrackEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (AnimSeqExportOption != nullptr)
	{
		Collector.AddReferencedObject(AnimSeqExportOption);
	}
}


TSharedRef<ISequencerTrackEditor> FSkeletalAnimationTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FSkeletalAnimationTrackEditor( InSequencer ) );
}

bool FSkeletalAnimationTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	ETrackSupport TrackSupported = InSequence ? InSequence->IsTrackSupported(UMovieSceneSkeletalAnimationTrack::StaticClass()) : ETrackSupport::NotSupported;
	return TrackSupported == ETrackSupport::Supported;
}

bool FSkeletalAnimationTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneSkeletalAnimationTrack::StaticClass();
}


TSharedRef<ISequencerSection> FSkeletalAnimationTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	
	return MakeShareable( new FSkeletalAnimationSection(SectionObject, GetSequencer()) );
}


bool FSkeletalAnimationTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	if (Asset->IsA<UAnimSequenceBase>() && SequencerPtr.IsValid())
	{
		UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(Asset);
		
		if (TargetObjectGuid.IsValid() && AnimSequence->CanBeUsedInComposition())
		{
			USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(TargetObjectGuid, GetSequencer());

			if (Skeleton && Skeleton->IsCompatibleForEditor(AnimSequence->GetSkeleton()))
			{
				UObject* Object = SequencerPtr->FindSpawnedObjectOrTemplate(TargetObjectGuid);
				
				UMovieSceneTrack* Track = nullptr;

				const FScopedTransaction Transaction(LOCTEXT("AddAnimation_Transaction", "Add Animation"));

				int32 RowIndex = INDEX_NONE;
				AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSkeletalAnimationTrackEditor::AddKeyInternal, Object, AnimSequence, Track, RowIndex));

				return true;
			}
		}
	}
	return false;
}

void FSkeletalAnimationTrackEditor::BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) || ObjectClass->IsChildOf(AActor::StaticClass()) || ObjectClass->IsChildOf(UChildActorComponent::StaticClass()))
	{
		ConstructObjectBindingTrackMenu(MenuBuilder, ObjectBindings);
	}
}
void FSkeletalAnimationTrackEditor::OnSequencerSaved(ISequencer& )
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid())
	{
		return;
	}
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(SequencerPtr->GetFocusedMovieSceneSequence());
	if (LevelSequence && LevelSequence->GetClass()->ImplementsInterface(UInterface_AssetUserData::StaticClass()))
	{
		if (IInterface_AssetUserData* AssetUserDataInterface = Cast< IInterface_AssetUserData >(LevelSequence))
		{
			ULevelSequenceAnimSequenceLink* LevelAnimLink = AssetUserDataInterface->GetAssetUserData< ULevelSequenceAnimSequenceLink >();
			if (LevelAnimLink)
			{
				UMovieScene* MovieScene = SequencerPtr->GetFocusedMovieSceneSequence()->GetMovieScene();
				FMovieSceneSequenceIDRef Template = SequencerPtr->GetFocusedTemplateID();
				FMovieSceneSequenceTransform RootToLocalTransform = SequencerPtr->GetFocusedMovieSceneSequenceTransform();
				for (int32 Index = LevelAnimLink->AnimSequenceLinks.Num() -1; Index >=0 ; --Index)
				{
					FLevelSequenceAnimSequenceLinkItem& Item = LevelAnimLink->AnimSequenceLinks[Index];
					UAnimSequence* AnimSequence = Item.ResolveAnimSequence();
					if (AnimSequence == nullptr)
					{
						LevelAnimLink->AnimSequenceLinks.RemoveAt(Index);
						continue;
					}
					if (IInterface_AssetUserData* AnimAssetUserData = Cast< IInterface_AssetUserData >(AnimSequence))
					{
						UAnimSequenceLevelSequenceLink* AnimLevelLink = AnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
						if (!AnimLevelLink)
						{
							AnimLevelLink = NewObject<UAnimSequenceLevelSequenceLink>(AnimSequence, NAME_None, RF_Public | RF_Transactional);
							AnimAssetUserData->AddAssetUserData(AnimLevelLink);
						}
						AnimLevelLink->SetLevelSequence(LevelSequence);
						AnimLevelLink->SkelTrackGuid = Item.SkelTrackGuid;
					}
					USkeletalMeshComponent* SkelMeshComp = AcquireSkeletalMeshFromObjectGuid(Item.SkelTrackGuid, GetSequencer());
					if (AnimSequence && SkelMeshComp)
					{
						const bool bSavedExportMorphTargets = AnimSeqExportOption->bExportMorphTargets;
						const bool bSavedExportAttributeCurves = AnimSeqExportOption->bExportAttributeCurves;
						const bool bSavedExportMaterialCurves = AnimSeqExportOption->bExportMaterialCurves;
						const bool bSavedExportTransforms = AnimSeqExportOption->bExportTransforms;
						const bool bSavedIncludeComponentTransform = AnimSeqExportOption->bRecordInWorldSpace;
						const bool bSavedEvaluateAllSkeletalMeshComponents = AnimSeqExportOption->bEvaluateAllSkeletalMeshComponents;
						const EAnimInterpolationType SavedInterpolationType = AnimSeqExportOption->Interpolation;
						const ERichCurveInterpMode SavedCurveInterpolationType = AnimSeqExportOption->CurveInterpolation;

						AnimSeqExportOption->bExportMorphTargets = Item.bExportMorphTargets;
						AnimSeqExportOption->bExportAttributeCurves = Item.bExportAttributeCurves;
						AnimSeqExportOption->bExportMaterialCurves = Item.bExportMaterialCurves;
						AnimSeqExportOption->bExportTransforms = Item.bExportTransforms;
						AnimSeqExportOption->bRecordInWorldSpace = Item.bRecordInWorldSpace;
						AnimSeqExportOption->bEvaluateAllSkeletalMeshComponents = Item.bEvaluateAllSkeletalMeshComponents;
						AnimSeqExportOption->Interpolation = Item.Interpolation;
						AnimSeqExportOption->CurveInterpolation = Item.CurveInterpolation;

						bool bResult = MovieSceneToolHelpers::ExportToAnimSequence(AnimSequence, AnimSeqExportOption, MovieScene, SequencerPtr.Get(), SkelMeshComp, Template, RootToLocalTransform);

						AnimSeqExportOption->bExportMorphTargets = bSavedExportMorphTargets;
						AnimSeqExportOption->bExportAttributeCurves = bSavedExportAttributeCurves;
						AnimSeqExportOption->bExportMaterialCurves = bSavedExportMaterialCurves;
						AnimSeqExportOption->bExportTransforms = bSavedExportTransforms;
						AnimSeqExportOption->bRecordInWorldSpace = bSavedIncludeComponentTransform;
						AnimSeqExportOption->bEvaluateAllSkeletalMeshComponents = bSavedEvaluateAllSkeletalMeshComponents;
						AnimSeqExportOption->Interpolation = SavedInterpolationType;
						AnimSeqExportOption->CurveInterpolation = SavedCurveInterpolationType;

						//save the anim sequence to disk to make sure they are in sync
						UPackage* const Package = AnimSequence->GetOutermost();
						FString const PackageName = Package->GetName();
						FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

						FSavePackageArgs SaveArgs;
						SaveArgs.TopLevelFlags = RF_Standalone;
						SaveArgs.SaveFlags = SAVE_NoError;
						UPackage::SavePackage(Package, NULL, *PackageFileName, SaveArgs);
					}
				}
			}
		}
	}
}

//dirty anim sequence when the sequencer changes, to make sure it get's checked out etc..
void FSkeletalAnimationTrackEditor::OnSequencerDataChanged(EMovieSceneDataChangeType DataChangeType)
{
	//only return if data really changed
	if(DataChangeType ==  EMovieSceneDataChangeType::RefreshTree ||
		DataChangeType == EMovieSceneDataChangeType::ActiveMovieSceneChanged ||
		DataChangeType == EMovieSceneDataChangeType::RefreshAllImmediately)
	{
		return;
	}
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid())
	{
		return;
	}
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(SequencerPtr->GetFocusedMovieSceneSequence());
	if (LevelSequence && LevelSequence->GetClass()->ImplementsInterface(UInterface_AssetUserData::StaticClass()))
	{
		if (IInterface_AssetUserData* AssetUserDataInterface = Cast< IInterface_AssetUserData >(LevelSequence))
		{
			ULevelSequenceAnimSequenceLink* LevelAnimLink = AssetUserDataInterface->GetAssetUserData< ULevelSequenceAnimSequenceLink >();
			if (LevelAnimLink)
			{
				for (int32 Index = LevelAnimLink->AnimSequenceLinks.Num() - 1; Index >= 0; --Index)
				{
					FLevelSequenceAnimSequenceLinkItem& Item = LevelAnimLink->AnimSequenceLinks[Index];
					UAnimSequence* AnimSequence = Item.ResolveAnimSequence();
					if (AnimSequence)
					{
						AnimSequence->Modify();
					}
				}
			}
		}
	}
}

void FSkeletalAnimationTrackEditor::OnPostPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent)
{
	// This attempts to fix tposes when changing properties without evaluating Sequencer UE-101261, 
	// but unfortunately causes other problems like the temporary unkeyed value getting lost UE-136405
/*
	if (InPropertyChangedEvent.ChangeType != EPropertyChangeType::ValueSet)
	{
		return;
	}

	// If the object changed has any animation track:
	// 1. Store the current transform (which may be an unkeyed value),
	// 2. Evaluate Sequencer so that the skeletal animation track will be evaluated, and then the skeletal mesh with tick
	// 3. Restore the current transform
	// Without this, changing a value on a skeletal mesh will tick but not necessarily evaluate Sequencer, resulting in a tpose.
	const bool bCreateIfMissing = false;
	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(InObject, bCreateIfMissing );
	FGuid ObjectHandle = HandleResult.Handle;
	if (ObjectHandle.IsValid())
	{
		FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieSceneSkeletalAnimationTrack::StaticClass(), NAME_None, bCreateIfMissing);
		if (TrackResult.Track)
		{
			USceneComponent* SceneComponent = Cast<USceneComponent>(InObject);
			if (!SceneComponent)
			{
				if (AActor* Actor = Cast<AActor>(InObject))
				{
					SceneComponent = Actor->GetRootComponent();
				}
			}

			FTransform RelativeTransform;
			if (SceneComponent)
			{
				RelativeTransform = SceneComponent->GetRelativeTransform();
			}

			GetSequencer()->ForceEvaluate();

			if (SceneComponent)
			{
				SceneComponent->SetRelativeTransform(RelativeTransform);
			}
		}
	}
*/
}

bool FSkeletalAnimationTrackEditor::CreateAnimationSequence(const TArray<UObject*> NewAssets, USkeletalMeshComponent* SkelMeshComp, FGuid Binding, bool bCreateSoftLink)
{
	bool bResult = false;
	if (NewAssets.Num() > 0)
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(NewAssets[0]);
		if (AnimSequence)
		{
			UObject* NewAsset = NewAssets[0];
			TSharedPtr<SWindow> ParentWindow;
			if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
			{
				IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
				ParentWindow = MainFrame.GetParentWindow();
			}

			TSharedRef<SWindow> Window = SNew(SWindow)
				.Title(NSLOCTEXT("UnrealEd", "AnimSeqOpionsTitle", "Animation Sequence Options"))
				.SizingRule(ESizingRule::UserSized)
				.AutoCenter(EAutoCenter::PrimaryWorkArea)
				.ClientSize(FVector2D(500, 445));

			TSharedPtr<SAnimSequenceOptionsWindow> OptionWindow;
			Window->SetContent
			(
				SAssignNew(OptionWindow, SAnimSequenceOptionsWindow)
				.ExportOptions(AnimSeqExportOption)
				.WidgetWindow(Window)
				.FullPath(FText::FromString(NewAssets[0]->GetName()))
			);

			FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

			if (OptionWindow->ShouldExport())
			{
				const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();
				UMovieScene* MovieScene = ParentSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
				FMovieSceneSequenceIDRef Template = ParentSequencer->GetFocusedTemplateID();
				FMovieSceneSequenceTransform RootToLocalTransform = ParentSequencer->GetFocusedMovieSceneSequenceTransform();

				bResult  = MovieSceneToolHelpers::ExportToAnimSequence(AnimSequence, AnimSeqExportOption,MovieScene, ParentSequencer.Get(), SkelMeshComp, Template, RootToLocalTransform);
			}
		}

		if (bResult && bCreateSoftLink)
		{
			FScopedTransaction Transaction(LOCTEXT("SaveLinkedAnimation_Transaction", "Save Link Animation"));
			TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
			ULevelSequence* LevelSequence = Cast<ULevelSequence>(SequencerPtr->GetFocusedMovieSceneSequence());
			if (LevelSequence && LevelSequence->GetClass()->ImplementsInterface(UInterface_AssetUserData::StaticClass())
				&& AnimSequence->GetClass()->ImplementsInterface(UInterface_AssetUserData::StaticClass()))
			{
				LevelSequence->Modify();
				if (IInterface_AssetUserData* AnimAssetUserData = Cast< IInterface_AssetUserData >(AnimSequence))
				{
					UAnimSequenceLevelSequenceLink* AnimLevelLink = AnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
					if (!AnimLevelLink)
					{
						AnimLevelLink = NewObject<UAnimSequenceLevelSequenceLink>(AnimSequence, NAME_None, RF_Public | RF_Transactional);
						AnimAssetUserData->AddAssetUserData(AnimLevelLink);
					}
					
					AnimLevelLink->SetLevelSequence(LevelSequence);
					AnimLevelLink->SkelTrackGuid = Binding;
				}
				if (IInterface_AssetUserData* AssetUserDataInterface = Cast< IInterface_AssetUserData >(LevelSequence))
				{
					bool bAddItem = true;
					ULevelSequenceAnimSequenceLink* LevelAnimLink = AssetUserDataInterface->GetAssetUserData< ULevelSequenceAnimSequenceLink >();
					if (LevelAnimLink)
					{
						for (FLevelSequenceAnimSequenceLinkItem& LevelAnimLinkItem : LevelAnimLink->AnimSequenceLinks)
						{
							if (LevelAnimLinkItem.SkelTrackGuid == Binding)
							{
								bAddItem = false;
								UAnimSequence* OtherAnimSequence = LevelAnimLinkItem.ResolveAnimSequence();
								
								if (OtherAnimSequence != AnimSequence)
								{
									if (IInterface_AssetUserData* OtherAnimAssetUserData = Cast< IInterface_AssetUserData >(OtherAnimSequence))
									{
										UAnimSequenceLevelSequenceLink* OtherAnimLevelLink = OtherAnimAssetUserData->GetAssetUserData< UAnimSequenceLevelSequenceLink >();
										if (OtherAnimLevelLink)
										{
											OtherAnimAssetUserData->RemoveUserDataOfClass(UAnimSequenceLevelSequenceLink::StaticClass());
										}
									}
								}
								LevelAnimLinkItem.PathToAnimSequence = FSoftObjectPath(AnimSequence);
								LevelAnimLinkItem.bExportMorphTargets = AnimSeqExportOption->bExportMorphTargets;
								LevelAnimLinkItem.bExportAttributeCurves = AnimSeqExportOption->bExportAttributeCurves;
								LevelAnimLinkItem.bExportMaterialCurves = AnimSeqExportOption->bExportMaterialCurves;
								LevelAnimLinkItem.bExportTransforms = AnimSeqExportOption->bExportTransforms;
								LevelAnimLinkItem.bRecordInWorldSpace = AnimSeqExportOption->bRecordInWorldSpace;
								LevelAnimLinkItem.bEvaluateAllSkeletalMeshComponents = AnimSeqExportOption->bEvaluateAllSkeletalMeshComponents;
								LevelAnimLinkItem.Interpolation = AnimSeqExportOption->Interpolation;
								LevelAnimLinkItem.CurveInterpolation = AnimSeqExportOption->CurveInterpolation;

								break;
							}
						}
					}
					else
					{
						LevelAnimLink = NewObject<ULevelSequenceAnimSequenceLink>(LevelSequence, NAME_None, RF_Public | RF_Transactional);
						
					}
					if (bAddItem == true)
					{
						FLevelSequenceAnimSequenceLinkItem LevelAnimLinkItem;
						LevelAnimLinkItem.SkelTrackGuid = Binding;
						LevelAnimLinkItem.PathToAnimSequence = FSoftObjectPath(AnimSequence);
						LevelAnimLinkItem.bExportMorphTargets = AnimSeqExportOption->bExportMorphTargets;
						LevelAnimLinkItem.bExportAttributeCurves = AnimSeqExportOption->bExportAttributeCurves;
						LevelAnimLinkItem.bExportMaterialCurves = AnimSeqExportOption->bExportMaterialCurves;
						LevelAnimLinkItem.bExportTransforms = AnimSeqExportOption->bExportTransforms;
						LevelAnimLinkItem.bRecordInWorldSpace = AnimSeqExportOption->bRecordInWorldSpace;
						LevelAnimLinkItem.bEvaluateAllSkeletalMeshComponents = AnimSeqExportOption->bEvaluateAllSkeletalMeshComponents;
						LevelAnimLinkItem.Interpolation = AnimSeqExportOption->Interpolation;
						LevelAnimLinkItem.CurveInterpolation = AnimSeqExportOption->CurveInterpolation;

						LevelAnimLink->AnimSequenceLinks.Add(LevelAnimLinkItem);
						AssetUserDataInterface->AddAssetUserData(LevelAnimLink);
					}

					/*
					FText Name = SequencerPtr->GetDisplayName(Binding);
					FString StringName = Name.ToString();
					if (bAddItem == false)
					{
						//ok already had a name added so need to remove the old one..
						TArray<FString> Strings;
						StringName.ParseIntoArray(Strings,TEXT(" --> "));
						if (Strings.Num() > 0)
						{
							StringName = Strings[0];
						}
					}
	
					FString AnimName = AnimSequence->GetName();
					StringName = StringName + FString(TEXT(" --> ")) + AnimName;
					SequencerPtr->SetDisplayName(Binding, FText::FromString(StringName));
					*/

				}
			}
		}
		// if it contains error, warn them
		if (bResult)
		{
			FText NotificationText;
			if (NewAssets.Num() == 1)
			{
				NotificationText = FText::Format(LOCTEXT("NumAnimSequenceAssetsCreated", "{0} Anim Sequence  assets created."), NewAssets.Num());
			}
			else
			{
				NotificationText = FText::Format(LOCTEXT("AnimSequenceAssetsCreated", "Anim Sequence asset created: '{0}'."), FText::FromString(NewAssets[0]->GetName()));
			}

			FNotificationInfo Info(NotificationText);
			Info.ExpireDuration = 8.0f;
			Info.bUseLargeFont = false;
			Info.Hyperlink = FSimpleDelegate::CreateLambda([NewAssets]()
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(NewAssets);
			});
			Info.HyperlinkText = FText::Format(LOCTEXT("OpenNewPoseAssetHyperlink", "Open {0}"), FText::FromString(NewAssets[0]->GetName()));

			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if (Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Success);
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToCreateAsset", "Failed to create asset"));
		}
	}
	return bResult;
}

void FSkeletalAnimationTrackEditor::HandleCreateAnimationSequence(USkeletalMeshComponent* SkelMeshComp, USkeleton* Skeleton, FGuid Binding, bool bCreateSoftLink)
{
	if (SkelMeshComp)
	{
		TArray<TSoftObjectPtr<UObject>> Skels;
		if (SkelMeshComp->GetSkeletalMeshAsset())
		{
			Skels.Add(SkelMeshComp->GetSkeletalMeshAsset());
		}
		else
		{
			Skels.Add(Skeleton);
		}
	
		const bool bDoNotShowNameDialog = false;
		const bool bAllowReplaceExisting = true;
		AnimationEditorUtils::ExecuteNewAnimAsset<UAnimSequenceFactory, UAnimSequence>(Skels, FString("_Sequence"), FAnimAssetCreated::CreateSP(this, &FSkeletalAnimationTrackEditor::CreateAnimationSequence, SkelMeshComp, Binding, bCreateSoftLink), bDoNotShowNameDialog, bAllowReplaceExisting);
	}
}

void FSkeletalAnimationTrackEditor::OpenLinkedAnimSequence(FGuid Binding)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid())
	{
		return;
	}
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(SequencerPtr->GetFocusedMovieSceneSequence());
	if (LevelSequence && LevelSequence->GetClass()->ImplementsInterface(UInterface_AssetUserData::StaticClass()))
	{
		if (IInterface_AssetUserData* AssetUserDataInterface = Cast< IInterface_AssetUserData >(LevelSequence))
		{
			ULevelSequenceAnimSequenceLink* LevelAnimLink = AssetUserDataInterface->GetAssetUserData< ULevelSequenceAnimSequenceLink >();
			if (LevelAnimLink)
			{
				
				for (FLevelSequenceAnimSequenceLinkItem& Item : LevelAnimLink->AnimSequenceLinks)
				{
					if (Item.SkelTrackGuid == Binding)
					{
						UAnimSequence* AnimSequence = Item.ResolveAnimSequence();
						if (AnimSequence)
						{
							GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AnimSequence);
						}
					}
				}
			}
		}
	}
}

bool FSkeletalAnimationTrackEditor::CanOpenLinkedAnimSequence(FGuid Binding)
{

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid())
	{
		return false;
	}
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(SequencerPtr->GetFocusedMovieSceneSequence());
	if (LevelSequence && LevelSequence->GetClass()->ImplementsInterface(UInterface_AssetUserData::StaticClass()))
	{
		if (IInterface_AssetUserData* AssetUserDataInterface = Cast< IInterface_AssetUserData >(LevelSequence))
		{
			ULevelSequenceAnimSequenceLink* LevelAnimLink = AssetUserDataInterface->GetAssetUserData< ULevelSequenceAnimSequenceLink >();
			if (LevelAnimLink)
			{

				for (FLevelSequenceAnimSequenceLinkItem& Item : LevelAnimLink->AnimSequenceLinks)
				{
					if (Item.SkelTrackGuid == Binding)
					{
						UAnimSequence* AnimSequence = Item.ResolveAnimSequence();
						if (AnimSequence)
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

void FSkeletalAnimationTrackEditor::ConstructObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings)
{
	if(ObjectBindings.Num() > 0)
	{
		USkeletalMeshComponent* SkelMeshComp =  AcquireSkeletalMeshFromObjectGuid(ObjectBindings[0], GetSequencer());

		if (SkelMeshComp)
		{

			MenuBuilder.BeginSection("Create Animation Assets", LOCTEXT("CreateAnimationAssetsName", "Create Animation Assets"));
			USkeleton* Skeleton = GetSkeletonFromComponent(SkelMeshComp);
			//todo do we not link if alreadhy linked???

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CreateLinkAnimSequence", "Create Linked Animation Sequence"),
				LOCTEXT("CreateLinkAnimSequenceTooltip", "Create Animation Sequence for this Skeletal Mesh and have this Track Own that Anim Sequence. Note it will create it based upon the Sequencer Display Range and Display Frame Rate"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FSkeletalAnimationTrackEditor::HandleCreateAnimationSequence, SkelMeshComp, Skeleton, ObjectBindings[0], true)),
				NAME_None,
				EUserInterfaceActionType::Button);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("OpenAnimSequence", "Open Linked Animation Sequence"),
				LOCTEXT("OpenAnimSequenceTooltip", "Open Animation Sequence that this Animation Track is Driving."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FSkeletalAnimationTrackEditor::OpenLinkedAnimSequence, ObjectBindings[0]),
					FCanExecuteAction::CreateRaw(this, &FSkeletalAnimationTrackEditor::CanOpenLinkedAnimSequence, ObjectBindings[0])
				),
				NAME_None,
				EUserInterfaceActionType::Button);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CreateAnimSequence", "Bake Animation Sequence"),
				LOCTEXT("PasteCreateAnimSequenceTooltip", "Bake an Animation Sequence for this Skeletal Mesh. Note it will create it based upon the Sequencer Display Range and Display Frame Rate"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FSkeletalAnimationTrackEditor::HandleCreateAnimationSequence, SkelMeshComp,Skeleton, ObjectBindings[0], false)),
				NAME_None,
				EUserInterfaceActionType::Button);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CreatePoseAsset", "Bake Pose Asset"),
				LOCTEXT("CreatePoseAsset_ToolTip", "Bake Animation from current Pose"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FSkeletalAnimationTrackEditor::HandleCreatePoseAsset, ObjectBindings[0])),
				NAME_None,
				EUserInterfaceActionType::Button);


			MenuBuilder.EndSection();
		}
	}
}

void FSkeletalAnimationTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) || ObjectClass->IsChildOf(AActor::StaticClass()) || ObjectClass->IsChildOf(UChildActorComponent::StaticClass()))
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBindings[0], GetSequencer());

		if (Skeleton)
		{
			// Load the asset registry module
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			// Collect a full list of assets with the specified class
			TArray<FAssetData> AssetDataList;
			AssetRegistryModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetClassPathName(), AssetDataList, true);

			if (AssetDataList.Num())
			{
				UMovieSceneTrack* Track = nullptr;

				MenuBuilder.AddSubMenu(
					LOCTEXT("AddAnimation", "Animation"), NSLOCTEXT("Sequencer", "AddAnimationTooltip", "Adds an animation track."),
					FNewMenuDelegate::CreateRaw(this, &FSkeletalAnimationTrackEditor::AddAnimationSubMenu, ObjectBindings, Skeleton, Track)
				);
			}
		}
	}
}

TSharedRef<SWidget> FSkeletalAnimationTrackEditor::BuildAnimationSubMenu(FGuid ObjectBinding, USkeleton* Skeleton, UMovieSceneTrack* Track)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	TArray<FGuid> ObjectBindings;
	ObjectBindings.Add(ObjectBinding);

	AddAnimationSubMenu(MenuBuilder, ObjectBindings, Skeleton, Track);

	return MenuBuilder.MakeWidget();
}

bool FSkeletalAnimationTrackEditor::ShouldFilterAsset(const FAssetData& AssetData)
{
	// we don't want montage
	if (AssetData.AssetClassPath == UAnimMontage::StaticClass()->GetClassPathName())
	{
		return true;
	}

	const FString EnumString = AssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType));
	if (EnumString.IsEmpty())
	{
		return false;
	}

	UEnum* AdditiveTypeEnum = StaticEnum<EAdditiveAnimationType>();
	return ((EAdditiveAnimationType)AdditiveTypeEnum->GetValueByName(*EnumString) == AAT_RotationOffsetMeshSpace);
}

void FSkeletalAnimationTrackEditor::AddAnimationSubMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings, USkeleton* Skeleton, UMovieSceneTrack* Track)
{
	UMovieSceneSequence* Sequence = GetSequencer() ? GetSequencer()->GetFocusedMovieSceneSequence() : nullptr;

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw( this, &FSkeletalAnimationTrackEditor::OnAnimationAssetSelected, ObjectBindings, Track);
		AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw( this, &FSkeletalAnimationTrackEditor::OnAnimationAssetEnterPressed, ObjectBindings, Track);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bAddFilterUI = true;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.Filter.ClassPaths.Add(UAnimSequenceBase::StaticClass()->GetClassPathName());
		AssetPickerConfig.OnShouldFilterAsset.BindRaw(this, &FSkeletalAnimationTrackEditor::FilterAnimSequences, Skeleton);
		AssetPickerConfig.SaveSettingsName = TEXT("SequencerAssetPicker");
		AssetPickerConfig.AdditionalReferencingAssets.Add(FAssetData(Sequence));
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TSharedPtr<SBox> MenuEntry = SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
}

bool FSkeletalAnimationTrackEditor::FilterAnimSequences(const FAssetData& AssetData, USkeleton* Skeleton)
{
	if (ShouldFilterAsset(AssetData))
	{
		return true;
	}

	if (Skeleton && Skeleton->IsCompatibleForEditor(AssetData) == false)
	{
		return true;
	}

	return false;
}

void FSkeletalAnimationTrackEditor::OnAnimationAssetSelected(const FAssetData& AssetData, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	if (SelectedObject && SelectedObject->IsA(UAnimSequenceBase::StaticClass()) && SequencerPtr.IsValid())
	{
		UAnimSequenceBase* AnimSequence = CastChecked<UAnimSequenceBase>(AssetData.GetAsset());

		const FScopedTransaction Transaction(LOCTEXT("AddAnimation_Transaction", "Add Animation"));

		for (FGuid ObjectBinding : ObjectBindings)
		{
			UObject* Object = SequencerPtr->FindSpawnedObjectOrTemplate(ObjectBinding);
			int32 RowIndex = INDEX_NONE;
			AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSkeletalAnimationTrackEditor::AddKeyInternal, Object, AnimSequence, Track, RowIndex));
		}
	}
}

void FSkeletalAnimationTrackEditor::OnAnimationAssetEnterPressed(const TArray<FAssetData>& AssetData, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track)
{
	if (AssetData.Num() > 0)
	{
		OnAnimationAssetSelected(AssetData[0].GetAsset(), ObjectBindings, Track);
	}
}


FKeyPropertyResult FSkeletalAnimationTrackEditor::AddKeyInternal( FFrameNumber KeyTime, UObject* Object, class UAnimSequenceBase* AnimSequence, UMovieSceneTrack* Track, int32 RowIndex )
{
	FKeyPropertyResult KeyPropertyResult;

	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object );
	FGuid ObjectHandle = HandleResult.Handle;
	KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
	if (ObjectHandle.IsValid())
	{
		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
		UMovieSceneSkeletalAnimationTrack* SkelAnimTrack = Cast<UMovieSceneSkeletalAnimationTrack>(Track);
		FMovieSceneBinding* Binding = MovieScene->FindBinding(ObjectHandle);

		// Add a track if no track was specified or if the track specified doesn't belong to the tracks of the targeted guid
		if (!SkelAnimTrack || (Binding && !Binding->GetTracks().Contains(SkelAnimTrack)))
		{
			SkelAnimTrack = CastChecked<UMovieSceneSkeletalAnimationTrack>(AddTrack(MovieScene, ObjectHandle, UMovieSceneSkeletalAnimationTrack::StaticClass(), NAME_None), ECastCheckedType::NullAllowed);
			KeyPropertyResult.bTrackCreated = true;
		}

		if (ensure(SkelAnimTrack))
		{
			SkelAnimTrack->Modify();

			UMovieSceneSkeletalAnimationSection* NewSection = Cast<UMovieSceneSkeletalAnimationSection>(SkelAnimTrack->AddNewAnimationOnRow(KeyTime, AnimSequence, RowIndex));
			KeyPropertyResult.bTrackModified = true;
			KeyPropertyResult.SectionsCreated.Add(NewSection);

			// Init the slot name on the new section if necessary
			if (USkeletalMeshComponent* SkeletalMeshComponent = AcquireSkeletalMeshFromObjectGuid(ObjectHandle, GetSequencer()))
			{
				if (TSubclassOf<UAnimInstance> AnimInstanceClass = SkeletalMeshComponent->GetAnimClass())
				{
					if (UAnimInstance* AnimInstance = AnimInstanceClass->GetDefaultObject<UAnimInstance>())
					{
						if (AnimInstance->Implements<USequencerAnimationOverride>())
						{
							TScriptInterface<ISequencerAnimationOverride> SequencerAnimOverride = AnimInstance;
							if (SequencerAnimOverride.GetObject())
							{
								TArray<FName> SlotNameOptions = ISequencerAnimationOverride::Execute_GetSequencerAnimSlotNames(SequencerAnimOverride.GetObject());
								if (SlotNameOptions.Num() > 0)
								{
									NewSection->Params.SlotName = SlotNameOptions[0];
								}
							}
						}
					}
				}
			}

			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewSection);
			GetSequencer()->ThrobSectionSelection();
		}
	}

	return KeyPropertyResult;
}

void FSkeletalAnimationTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	//there's a bug with a section being open already, so we end it.

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack = Cast<UMovieSceneSkeletalAnimationTrack>( Track );
	/** Put this back when and if it works
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("MotionBlendingOptions", "Motion Blending Options"));
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "AutoMatchClipsRootMotions", "Auto Match Clips Root Motions"),
			NSLOCTEXT("Sequencer", "AutoMatchClipsRootMotionsTooltip", "Preceeding clips will auto match to the preceding clips root bones position. You can override this behavior per clip in it's section options."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]()->void {
					SkeletalAnimationTrack->ToggleAutoMatchClipsRootMotions(); 
					SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

					}),
				FCanExecuteAction::CreateLambda([=]()->bool { return SequencerPtr && SkeletalAnimationTrack != nullptr; }),
				FIsActionChecked::CreateLambda([=]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->bAutoMatchClipsRootMotions; })),
			NAME_None, 
			EUserInterfaceActionType::ToggleButton
		);
		MenuBuilder.EndSection();
	}
	*/

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("SkelAnimRootMOtion", "Root Motion"));
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "BlendFirstChildOfRoot", "Blend First Child Of Root"),
			NSLOCTEXT("Sequencer", "BlendFirstChildOfRootTooltip", "If True, do not blend and match the root bones but instead the first child bone of the root. Toggle this on when the matched sequences in the track have no motion on the root."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SequencerPtr,SkeletalAnimationTrack]()->void {
					SkeletalAnimationTrack->bBlendFirstChildOfRoot = SkeletalAnimationTrack->bBlendFirstChildOfRoot ? false : true;
		SkeletalAnimationTrack->SetRootMotionsDirty();
		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

					}),
				FCanExecuteAction::CreateLambda([SequencerPtr, SkeletalAnimationTrack]()->bool { return SequencerPtr && SkeletalAnimationTrack != nullptr; }),
						FIsActionChecked::CreateLambda([SequencerPtr, SkeletalAnimationTrack]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->bBlendFirstChildOfRoot; })),
			NAME_None,
						EUserInterfaceActionType::ToggleButton
						);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "ShowRootMotionTrails", "Show Root Motion Trail"),
			NSLOCTEXT("Sequencer", "ShowRootMotionTrailsTooltip", "Show the Root Motion Trail for all Animation Clips."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SequencerPtr, SkeletalAnimationTrack]()->void {
					SkeletalAnimationTrack->ToggleShowRootMotionTrail();
		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

					}),
				FCanExecuteAction::CreateLambda([SequencerPtr, SkeletalAnimationTrack]()->bool { return SequencerPtr && SkeletalAnimationTrack != nullptr; }),
						FIsActionChecked::CreateLambda([SequencerPtr, SkeletalAnimationTrack]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->bShowRootMotionTrail; })),
			NAME_None,
						EUserInterfaceActionType::ToggleButton
						);


		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "SwapRootBoneNone", "Swap Root Bone None"),
			NSLOCTEXT("Sequencer", "SwapRootBoneNoneTooltip", "Do not swap root bone for all sections."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SkeletalAnimationTrack]()->void {
					SkeletalAnimationTrack->SetSwapRootBone(ESwapRootBone::SwapRootBone_None);
					}),
				FCanExecuteAction::CreateLambda([SkeletalAnimationTrack]()->bool { return  SkeletalAnimationTrack != nullptr; }),
				FIsActionChecked::CreateLambda([SkeletalAnimationTrack]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->SwapRootBone == ESwapRootBone::SwapRootBone_None; })),
				NAME_None,
				EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "SwapRootBoneActor", "Swap Root Bone Actor"),
			NSLOCTEXT("Sequencer", "SwapRootBoneActorTooltip", "Swap root bone on root actor component for all sections."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SkeletalAnimationTrack]()->void {
					SkeletalAnimationTrack->SetSwapRootBone(ESwapRootBone::SwapRootBone_Actor);
					}),
				FCanExecuteAction::CreateLambda([SkeletalAnimationTrack]()->bool { return  SkeletalAnimationTrack != nullptr; }),
				FIsActionChecked::CreateLambda([SkeletalAnimationTrack]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->SwapRootBone == ESwapRootBone::SwapRootBone_Actor; })),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "SwapRootBoneComponent", "Swap Root Bone Component"),
			NSLOCTEXT("Sequencer", "SwapRootBoneComponentTooltip", "Swap root bone on current component for all sections."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SkeletalAnimationTrack]()->void {
					SkeletalAnimationTrack->SetSwapRootBone(ESwapRootBone::SwapRootBone_Component);
					}),
				FCanExecuteAction::CreateLambda([SkeletalAnimationTrack]()->bool { return  SkeletalAnimationTrack != nullptr; }),
				FIsActionChecked::CreateLambda([SkeletalAnimationTrack]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->SwapRootBone == ESwapRootBone::SwapRootBone_Component; })),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
	}
	MenuBuilder.EndSection();
	MenuBuilder.AddSeparator();
}

TSharedPtr<SWidget> FSkeletalAnimationTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBinding, GetSequencer());

	if (Skeleton)
	{
		return UE::Sequencer::MakeAddButton(LOCTEXT("AnimationText", "Animation"), FOnGetContent::CreateSP(this, &FSkeletalAnimationTrackEditor::BuildAnimationSubMenu, ObjectBinding, Skeleton, Track), Params.ViewModel);
	}
	else
	{
		return TSharedPtr<SWidget>();
	}
}

bool FSkeletalAnimationTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams)
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	  
	if (!Operation.IsValid() || !Operation->IsOfType<FAssetDragDropOp>() )
	{
		return false;
	}
	
	if (!DragDropParams.TargetObjectGuid.IsValid())
	{
		return false;
	}

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr)
	{
		return false;
	}

	UMovieSceneSequence* FocusedSequence = SequencerPtr->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return false;
	}

	TArray<USkeletalMeshComponent*> SkeletalMeshComponents = AcquireSkeletalMeshComponentsFromObjectGuid(DragDropParams.TargetObjectGuid, SequencerPtr, false);

	TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( Operation );

	for (const FAssetData& AssetData : DragDropOp->GetAssets())
	{
		if (!MovieSceneToolHelpers::IsValidAsset(FocusedSequence, AssetData))
		{
			continue;
		}

		UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(AssetData.GetAsset());

		const bool bValidAnimSequence = AnimSequence && AnimSequence->CanBeUsedInComposition();

		for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
		{
			USkeleton* Skeleton = GetSkeletonFromComponent(SkeletalMeshComponent);
			if (bValidAnimSequence && Skeleton && Skeleton->IsCompatibleForEditor(AnimSequence->GetSkeleton()))
			{
				FFrameRate TickResolution = SequencerPtr->GetFocusedTickResolution();
				FFrameNumber LengthInFrames = TickResolution.AsFrameNumber(AnimSequence->GetPlayLength());
				DragDropParams.FrameRange = TRange<FFrameNumber>(DragDropParams.FrameNumber, DragDropParams.FrameNumber + LengthInFrames);
				return true;
			}
		}
	}

	return false;
}


FReply FSkeletalAnimationTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams)
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (!Operation.IsValid() || !Operation->IsOfType<FAssetDragDropOp>() )
	{
		return FReply::Unhandled();
	}
	
	if (!DragDropParams.TargetObjectGuid.IsValid())
	{
		return FReply::Unhandled();
	}

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr)
	{
		return FReply::Unhandled();
	}

	UMovieSceneSequence* FocusedSequence = SequencerPtr->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return FReply::Unhandled();
	}

	TArray<USkeletalMeshComponent*> SkeletalMeshComponents = AcquireSkeletalMeshComponentsFromObjectGuid(DragDropParams.TargetObjectGuid, SequencerPtr, false);

	const FScopedTransaction Transaction(LOCTEXT("DropAssets", "Drop Assets"));

	TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( Operation );

	FMovieSceneTrackEditor::BeginKeying(DragDropParams.FrameNumber);

	bool bAnyDropped = false;
	for (const FAssetData& AssetData : DragDropOp->GetAssets())
	{
		if (!MovieSceneToolHelpers::IsValidAsset(FocusedSequence, AssetData))
		{
			continue;
		}

		UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(AssetData.GetAsset());
		const bool bValidAnimSequence = AnimSequence && AnimSequence->CanBeUsedInComposition();

		for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
		{
			USkeleton* Skeleton = GetSkeletonFromComponent(SkeletalMeshComponent);

			if (bValidAnimSequence && Skeleton && Skeleton->IsCompatibleForEditor(AnimSequence->GetSkeleton()))
			{
				UObject* BoundObject = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(DragDropParams.TargetObjectGuid) : nullptr;

				AnimatablePropertyChanged( FOnKeyProperty::CreateRaw(this, &FSkeletalAnimationTrackEditor::AddKeyInternal, BoundObject, AnimSequence, DragDropParams.Track.Get(), DragDropParams.RowIndex));

				bAnyDropped = true;
			}
		}
	}

	FMovieSceneTrackEditor::EndKeying();

	return bAnyDropped ? FReply::Handled() : FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE
