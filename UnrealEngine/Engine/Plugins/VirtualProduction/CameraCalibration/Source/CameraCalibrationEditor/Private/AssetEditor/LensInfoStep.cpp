// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensInfoStep.h"

#include "CameraCalibrationStepsController.h"
#include "IStructureDetailsView.h"
#include "LensFile.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "SResetToDefaultMenu.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "LensInfoStep"

void ULensInfoStep::Initialize(TWeakPtr<FCameraCalibrationStepsController> InCameraCalibrationStepController)
{
	CameraCalibrationStepsController = InCameraCalibrationStepController;

	const ULensFile* LensFile = CameraCalibrationStepsController.Pin()->GetLensFile();
	OriginalLensInfo = LensFile->LensInfo;
}

TSharedRef<SWidget> ULensInfoStep::BuildUI()
{
	FStructureDetailsViewArgs LensInfoStructDetailsView;
	FDetailsViewArgs DetailArgs;
	DetailArgs.bAllowSearch = false;
	DetailArgs.bShowScrollBar = true;

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	ULensFile* LensFile = CameraCalibrationStepsController.Pin()->GetLensFile();
	TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FLensInfo::StaticStruct(), reinterpret_cast<uint8*>(&LensFile->LensInfo));

	TSharedPtr<IStructureDetailsView> LensInfoStructureDetailsView = PropertyEditor.CreateStructureDetailView(DetailArgs, LensInfoStructDetailsView, StructOnScope);

	LensInfoStructureDetailsView->GetOnFinishedChangingPropertiesDelegate().AddLambda([&](const FPropertyChangedEvent& PropertyChangedEvent)
	{
		// Ignore temporary interaction (dragging sliders, etc.)
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
		{
			OnSaveLensInformation();
		}
	});

	TSharedPtr<SWidget> StepWidget = 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot() // Lens information structure
		.AutoHeight()
		[ 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[ LensInfoStructureDetailsView->GetWidget().ToSharedRef() ]
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SResetToDefaultMenu)
				.OnResetToDefault(FSimpleDelegate::CreateUObject(this, &ULensInfoStep::ResetToDefault))
				.DiffersFromDefault(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(this, &ULensInfoStep::DiffersFromDefault)))
			]
		];
	

	return StepWidget.ToSharedRef();
}

bool ULensInfoStep::DependsOnStep(UCameraCalibrationStep* Step) const
{
	return false;
}

void ULensInfoStep::Activate()
{
	// Nothing to do if it is already active.
	if (bIsActive)
	{
		return;
	}

	ULensFile* LensFile = CameraCalibrationStepsController.Pin()->GetLensFile();
	if(LensFile)
	{
		// Initialize lens info to current value of the LensFile
		CachedLensInfo = LensFile->LensInfo;
	}

	bIsActive = true;
}

void ULensInfoStep::Deactivate()
{
	bIsActive = false;
}

void ULensInfoStep::OnSaveLensInformation()
{
	if (!CameraCalibrationStepsController.IsValid())
	{
		return;
	}

	ULensFile* LensFile = CameraCalibrationStepsController.Pin()->GetLensFile();

	// Validate sensor dimensions
	constexpr float MinimumSize = 1.0f; // Limit sensor dimension to 1mm
	if(LensFile->LensInfo.SensorDimensions.X < MinimumSize || LensFile->LensInfo.SensorDimensions.Y < MinimumSize)
	{
		const FText ErrorMessage = LOCTEXT("InvalidSensorDimensions", "Invalid sensor dimensions. Can't have dimensions smaller than 1mm");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		
		// Reset sensor dimensions if the new value was invalid
		LensFile->LensInfo.SensorDimensions = CachedLensInfo.SensorDimensions;
		return;	
	}

	// Before beginning the transaction, briefly restore the LensInfo to its previously saved state
	const FLensInfo NewLensInfo = LensFile->LensInfo;
	LensFile->LensInfo = CachedLensInfo;
	{
		FScopedTransaction Transaction(LOCTEXT("ModifyingLensInfo", "Modifying LensFile information"));
		this->Modify();
		LensFile->Modify();

		// After beginning the transaction, reapply the modified LensInfo to the LensFile
		LensFile->LensInfo = NewLensInfo;

		// If lens model has changed and distortion table has data, notify the user that calibration data will be lost
		const bool bHasModelChanged = CachedLensInfo.LensModel != LensFile->LensInfo.LensModel;
		if (bHasModelChanged)
		{
			if (LensFile->HasSamples(ELensDataCategory::Distortion)
				|| LensFile->HasSamples(ELensDataCategory::ImageCenter)
				|| LensFile->HasSamples(ELensDataCategory::Zoom))
			{
				const FText Message = LOCTEXT("DataChangeDataLoss", "Lens model change detected. Distortion, ImageCenter and FocalLength data samples will be lost. Do you wish to continue?");
				if (FMessageDialog::Open(EAppMsgType::OkCancel, Message) != EAppReturnType::Ok)
				{
					Transaction.Cancel();
					LensFile->LensInfo.LensModel = CachedLensInfo.LensModel;
					return;
				}
			}

			// Clear table when disruptive change is detected and user proceeds
			LensFile->ClearData(ELensDataCategory::Distortion);
			LensFile->ClearData(ELensDataCategory::ImageCenter);
			LensFile->ClearData(ELensDataCategory::Zoom);
		}

		// Cache the newly modified LensInfo as the most recently saved state
		CachedLensInfo = LensFile->LensInfo;
	}
}

bool ULensInfoStep::IsActive() const
{
	return bIsActive;
}

void ULensInfoStep::ResetToDefault()
{
	ULensFile* LensFile = CameraCalibrationStepsController.Pin()->GetLensFile();
	LensFile->LensInfo = OriginalLensInfo;
	OnSaveLensInformation();
}

bool ULensInfoStep::DiffersFromDefault() const
{
	const ULensFile* LensFile = CameraCalibrationStepsController.Pin()->GetLensFile();
	return !(OriginalLensInfo == LensFile->LensInfo);
}



#undef LOCTEXT_NAMESPACE
