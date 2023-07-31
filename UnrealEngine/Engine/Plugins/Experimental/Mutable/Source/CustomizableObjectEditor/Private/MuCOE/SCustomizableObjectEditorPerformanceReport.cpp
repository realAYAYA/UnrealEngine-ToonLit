// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuCOE/SCustomizableObjectEditorPerformanceReport.h"

#include "AssetThumbnail.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Delegates/Delegate.h"
#include "DetailsViewArgs.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Math/Color.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/SCustomizableObjectEditorTextureAnalyzer.h"
#include "MuCOE/StatePerformanceTesting.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "Types/SlateStructs.h"
#include "UObject/Package.h"
#include "UObject/UnrealNames.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class ITableRow;
class STableViewBase;
class SWidget;
struct FGeometry;
struct FSlateBrush;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

static const FName ColumnID_PR_LOD_Label("LOD");
static const FName ColumnID_PR_InstanceName_Label("InstanceName");
static const FName ColumnID_PR_TexturesTotalSize_Label("TexturesTotalSize");
static const FName ColumnID_PR_NumberOfTriangles_Label("NumberOfTriangles");
static const FName ColumnID_PR_Name_Label("ParameterName");
static const FName ColumnID_PR_UpdateTime_Label("UpdateTimeState");
static const FName ColumnID_PR_Delta_Label("WorstCaseDelta");
static const FName ColumnID_PR_Average_Label("Average");

void UWorstCasePerformanceReportInstance::SetInstance(UCustomizableObjectInstance* UpdatedInstance)
{
	if (UpdatedInstance)
	{
		if (!WorstCaseInstance)
		{
			/* Uncommment next line if we need the report to have the valid time entries for state parameters expanded by default */
			//SubVisibility = EVisibility::Visible;
		}
		WorstCaseInstance = UpdatedInstance->Clone();
	}
}

void SCustomizableObjecEditorPerformanceReport::Construct(const FArguments& InArgs)
{
	ReportSubject = InArgs._CustomizableObject;
	StopTests();
	ResetSavedData();
	ResetPerformanceReportOptions();

	SAssignNew(PerformanceReportTextureAnalyzer, SCustomizableObjecEditorTextureAnalyzer)
		.CustomizableObjectEditor(nullptr)
		.CustomizableObjectInstanceEditor(nullptr);

	FPropertyEditorModule& PropPlugin = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::HideNameArea;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bShowObjectLabel = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;

	float WidgetSize = 128.0f;
	int ThumbnailSize = 128;

	ChildSlot
	[
		SNew(SSplitter)

		// Performance report status and dynamic results (left section)
		+ SSplitter::Slot()
		[
			SNew(SBox)
			.Padding(5.0f)
			.MinDesiredWidth(300)
			[
				SNew(SVerticalBox)

				// Button with settings dropdown
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				.Padding(5.0f)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &SCustomizableObjecEditorPerformanceReport::GenerateOptionsMenuContent)
					.IsEnabled(ReportSubject != nullptr)
					.ButtonContent()
					[
						SNew(SButton)
						.Text(LOCTEXT("GeneratePerformanceReport", "Generate Performance Report"))
						.OnClicked(this, &SCustomizableObjecEditorPerformanceReport::RunPerformanceReport)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
						.ForegroundColor(FLinearColor::White)
					]
				]

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				.Padding(5.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("GetWorstTimeInstance", "Select the currently slowest instance"))
					.OnClicked(this, &SCustomizableObjecEditorPerformanceReport::SelectSlowestInstance)
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Info")
					.IsEnabled(this, &SCustomizableObjecEditorPerformanceReport::IsGetWorstInstanceButtonEnabled)
					// This button target changes fast, I think it's better if it triggers on down instead of on up.
					.ClickMethod(EButtonClickMethod::MouseDown)
					.TouchMethod(EButtonTouchMethod::Down)
					.PressMethod(EButtonPressMethod::ButtonPress)
				]

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				.Padding(5.0f)
				[
					SAssignNew(StatusText, STextBlock)
				]

				// Info on Texture Size worst cases
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(5.0f, 10.0f, 5.0f, 5.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MutableLargestTextures", "Instances with largest mutable textures:"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.Padding(5.0f)
					[
						SAssignNew(TextureWorstCasesView, SVerticalBox)
						.Visibility_Lambda([this]() { return WorstTextureInstances.Num() > 0 && WorstTextureInstances[0] && WorstTextureInstances[0]->WorstCaseInstance ? EVisibility::Visible : EVisibility::Collapsed; })
					]
				]

				// Info on Geometry Amount worst cases
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(5.0f, 20.0f, 5.0f, 5.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MutableMostGeometry", "Instances with most triangles:"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.Padding(5.0f)
					.Visibility_Lambda([this]() { return WorstGeometryInstances.Num() > 0 && WorstGeometryInstances[0] && WorstGeometryInstances[0]->WorstCaseInstance ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SAssignNew(GeometryWorstCasesView, SVerticalBox)
					]
				]

				// Info on Update Time worst cases detailed by state and parameter
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoHeight()
				.Padding(5.0f, 20.0f, 5.0f, 5.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MutableLongestUpdateTimesDetailed", "Instances with longest updates by state and parameter:"))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0)
				[
					SNew(SBorder)
					.Padding(5.0f)
					.Visibility_Lambda([this]() { return WorstTimeStateInstances.Num() > 0
						&& WorstTimeStateInstances[0].Num() > 0
						&& WorstTimeStateInstances[0][0]
						&& WorstTimeStateInstances[0][0]->WorstCaseInstance
						? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SAssignNew(TimeWorstCasesDetailedView, SScrollBox)
					]
				]
			]
		]

		// Currently selected worst instance and its texture analyzer (right section)
		+ SSplitter::Slot()
		[
			BuildAnalyzer()
		]
	];

	BuildTextureWorstCasesView();
	BuildGeometryWorstCasesView();
	BuildTimeWorstCasesDetailedView();
	StatusText->SetText(LOCTEXT("PerformanceReportStatusNone","Generate Performance Report to collect test data."));

	HelperCallback = NewObject<UPerformanceReportHelper>(GetTransientPackage());
	HelperCallback->PerformanceReport = this;
}

SCustomizableObjecEditorPerformanceReport::~SCustomizableObjecEditorPerformanceReport()
{
	if (CurrentReportWorstCase && CurrentReportWorstCase->WorstCaseInstance)
	{
		CurrentReportWorstCase->WorstCaseInstance->UpdatedDelegate.RemoveDynamic(HelperCallback, &UPerformanceReportHelper::DelegatedCallback);
	}
	HelperCallback = nullptr;
	WorstTimeInstance = nullptr;
	CurrentReportWorstCase = nullptr;
	CurrentReportInstance = nullptr;
}

TSharedRef<SWidget> SCustomizableObjecEditorPerformanceReport::GenerateOptionsMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = false;
	FMenuBuilder MenuBuilder(false, nullptr);

	// settings
	MenuBuilder.BeginSection("ResetOptions", LOCTEXT("MutableResetPerformanceReportOptions", "Reset Options"));
	{
		SAssignNew(PerformanceReportResetOptionsEntry, SButton)
			.Text(LOCTEXT("MutableGeneratePerformanceReport", "Reset Performance Report Options"))
			.OnClicked(this, &SCustomizableObjecEditorPerformanceReport::ResetPerformanceReportOptions);
		MenuBuilder.AddWidget(PerformanceReportResetOptionsEntry.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("StressTestParameters", LOCTEXT("MutablePerformanceReportStressTestParameters", "Stress Test Parameters"));
	{
		SAssignNew(StressTestOptionInstanceCountEntry, SNumericEntryBox<int32>)
			.AllowSpin(true)
			.MinValue(0)
			.MaxValue(1000000)
			.MinSliderValue(1)
			.MaxSliderValue(1000)
			.Value(this, &SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceCount)
			.OnValueChanged(this, &SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceCountChanged);
		MenuBuilder.AddWidget(StressTestOptionInstanceCountEntry.ToSharedRef(), LOCTEXT("MutableStressTestInstanceCount", "Instance Count"));

		SAssignNew(StressTestOptionCreateInstanceTimeEntry, SNumericEntryBox<int32>)
			.AllowSpin(true)
			.MinValue(1)
			.MaxValue(1000000)
			.MinSliderValue(1)
			.MaxSliderValue(1000)
			.Value(this, &SCustomizableObjecEditorPerformanceReport::GetStressTestOptionCreateInstanceTime)
			.OnValueChanged(this, &SCustomizableObjecEditorPerformanceReport::OnStressTestOptionCreateInstanceTimeChanged);
		MenuBuilder.AddWidget(StressTestOptionCreateInstanceTimeEntry.ToSharedRef(), LOCTEXT("MutableStressTestCreateInstanceTime", "Instance Create Time (ms)"));

		SAssignNew(StressTestOptionCreateInstanceTimeVarEntry, SNumericEntryBox<int32>)
			.AllowSpin(true)
			.MinValue(1)
			.MaxValue(1000000)
			.MinSliderValue(1)
			.MaxSliderValue(10000)
			.Value(this, &SCustomizableObjecEditorPerformanceReport::GetStressTestOptionCreateInstanceTimeVar)
			.OnValueChanged(this, &SCustomizableObjecEditorPerformanceReport::OnStressTestOptionCreateInstanceTimeVarChanged);
		MenuBuilder.AddWidget(StressTestOptionCreateInstanceTimeVarEntry.ToSharedRef(), LOCTEXT("MutableStressTestCreateInstanceTimeVar", "Instance Create Time variation (ms)"));

		SAssignNew(StressTestOptionInstanceUpdateTimeEntry, SNumericEntryBox<int32>)
			.AllowSpin(true)
			.MinValue(0)
			.MaxValue(1000000)
			.MinSliderValue(1)
			.MaxSliderValue(10000)
			.Value(this, &SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceUpdateTime)
			.OnValueChanged(this, &SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceUpdateTimeChanged);
		MenuBuilder.AddWidget(StressTestOptionInstanceUpdateTimeEntry.ToSharedRef(), LOCTEXT("MutableStressTestInstanceUpdateTime", "Instance Update Time (ms)"));

		SAssignNew(StressTestOptionInstanceUpdateTimeVarEntry, SNumericEntryBox<int32>)
			.AllowSpin(true)
			.MinValue(1)
			.MaxValue(1000000)
			.MinSliderValue(1)
			.MaxSliderValue(10000)
			.Value(this, &SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceUpdateTimeVar)
			.OnValueChanged(this, &SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceUpdateTimeVarChanged);
		MenuBuilder.AddWidget(StressTestOptionInstanceUpdateTimeVarEntry.ToSharedRef(), LOCTEXT("MutableStressTestInstanceUpdateTimeVar", "Instance Update Time variation (ms)"));

		SAssignNew(StressTestOptionInstanceLifeTimeEntry, SNumericEntryBox<int32>)
			.AllowSpin(true)
			.MinValue(0)
			.MaxValue(1000000)
			.MinSliderValue(1)
			.MaxSliderValue(10000)
			.Value(this, &SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceLifeTime)
			.OnValueChanged(this, &SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceLifeTimeChanged);
		MenuBuilder.AddWidget(StressTestOptionInstanceLifeTimeEntry.ToSharedRef(), LOCTEXT("MutableStressTestInstanceLifeTime", "Instance Life Time (ms)"));

		SAssignNew(StressTestOptionInstanceLifeTimeVarEntry, SNumericEntryBox<int32>)
			.AllowSpin(true)
			.MinValue(1)
			.MaxValue(1000000)
			.MinSliderValue(1)
			.MaxSliderValue(10000)
			.Value(this, &SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceLifeTimeVar)
			.OnValueChanged(this, &SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceLifeTimeVarChanged);
		MenuBuilder.AddWidget(StressTestOptionInstanceLifeTimeVarEntry.ToSharedRef(), LOCTEXT("MutableStressTestInstanceLifeTimeMs", "Instance Life Time variation (ms)"));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SCustomizableObjecEditorPerformanceReport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ReportSubject);
	Collector.AddReferencedObject(HelperCallback);
	Collector.AddReferencedObject(WorstTimeInstance);
	Collector.AddReferencedObject(CurrentReportWorstCase);
	Collector.AddReferencedObject(CurrentReportInstance);
	Collector.AddReferencedObjects(WorstTextureInstances);
	Collector.AddReferencedObjects(WorstGeometryInstances);
	for (TArray<UWorstCasePerformanceReportInstance*>& Arr : WorstTimeStateInstances)
	{
		Collector.AddReferencedObjects(Arr);
	}
	for (TArray<TArray<UWorstCasePerformanceReportInstance*>>& ArrArr : WorstTimeParameterInstances)
	{
		for (TArray<UWorstCasePerformanceReportInstance*>& Arr : ArrArr)
		{
			Collector.AddReferencedObjects(Arr);
		}
	}
}

void SCustomizableObjecEditorPerformanceReport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SCustomizableObjecEditorPerformanceReport::StopTests()
{
	if (StressTest.IsValid() && StressTest->GetTestInCourse())
	{
		StressTest->WorstFacesDelegate.Unbind();
		StressTest->WorstTextureDelegate.Unbind();
		StressTest->StressTestEnded.Unbind();
		StressTest->FinishTest();
		StressTest.Reset();
	}

	if (StateTest.IsValid())
	{
		StateTest->WorstTimeDelegate.Unbind();
		StateTest->StateTestEnded.Unbind();
		StateTest->FinishTest();
		StateTest.Reset();
	}
}

FReply SCustomizableObjecEditorPerformanceReport::RunPerformanceReport()
{
	return RunPerformanceReport(nullptr);
}

FReply SCustomizableObjecEditorPerformanceReport::RunPerformanceReport(UCustomizableObject* ObjectToTest)
{
	if (ObjectToTest)
	{
		ReportSubject = ObjectToTest;
	}

	if (ReportSubject)
	{
		StopTests();

		if (CurrentReportInstance || CurrentReportWorstCase)
		{
			CurrentReportInstance = nullptr;
			CurrentReportWorstCase = nullptr;
			UpdateSelectedInstanceData();
		}

		ResetSavedData();

		StressTest = MakeShareable(new FRunningStressTest);
		StressTest->SetCustomizableObject(ReportSubject);
		StressTest->SetVerifyInstancesThreshold(5.0f);
		StressTest->SetCreateInstanceTimeMs(StressTestOptionCreateInstanceTime);
		StressTest->SetCreateInstanceTimeMsVar(StressTestOptionCreateInstanceTimeVar);
		StressTest->SetInstanceLifeTimeMs(StressTestOptionInstanceLifeTime);
		StressTest->SetInstanceLifeTimeMsVar(StressTestOptionInstanceLifeTimeVar);
		StressTest->SetInstanceUpdateTimeMs(StressTestOptionInstanceUpdateTime);
		StressTest->SetInstanceUpdateTimeMsVar(StressTestOptionInstanceUpdateTimeVar);
		StressTest->SetPendingInstanceCount(StressTestOptionInstanceCount);
		StressTest->SetNextInstanceTimeMs(0);
		StressTest->WorstFacesDelegate.BindRaw(this, &SCustomizableObjecEditorPerformanceReport::WorstFacesFound);
		StressTest->WorstTextureDelegate.BindRaw(this, &SCustomizableObjecEditorPerformanceReport::WorstTextureFound);
		StressTest->StressTestEnded.BindRaw(this, &SCustomizableObjecEditorPerformanceReport::StressTestEnded);
		StressTest->InitMeasureTest();

		StatusText->SetText(LOCTEXT("PerformanceReportStatusStressRunning", "Running Stress test."));
	}
	else
	{
		StatusText->SetText(LOCTEXT("PerformanceReportStatusNoObject", "Generate Performance Report found no valid Customizable Object."));
	}

	return FReply::Handled();
}

FReply SCustomizableObjecEditorPerformanceReport::SelectSlowestInstance()
{
	check(WorstTimeInstance);
	UWorstCasePerformanceReportInstance* SlowestInstanceInStructure = nullptr;
	if (WorstTimeInstance)
	{
		switch (WorstTimeInstance->WorstCaseType)
		{
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE:
		{
			SlowestInstanceInStructure = GetWorstCaseStructureFromIndexes(WorstTimeInstance->WorstCaseType, WorstTimeInstance->LOD, WorstTimeInstance->LongestUpdateTimeStateIndex);
			break;
		}
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE:
		{
			if (UWorstCasePerformanceReportInstance* SlowestInstanceInStructureParent = GetWorstCaseStructureFromIndexes(
				UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE,
				WorstTimeInstance->LOD,
				WorstTimeInstance->LongestUpdateTimeStateIndex))
			{
				SlowestInstanceInStructureParent->SubVisibility = EVisibility::Visible;
			}
			SlowestInstanceInStructure = GetWorstCaseStructureFromIndexes(WorstTimeInstance->WorstCaseType, WorstTimeInstance->LOD, WorstTimeInstance->LongestUpdateTimeStateIndex, WorstTimeInstance->LongestUpdateTimeParameterIndexInCO);
			break;
		}
		default:
		{
			break;
		}
		}
	}
	if (SlowestInstanceInStructure)
	{
		SelectInstance(SlowestInstanceInStructure);
	}
	return FReply::Handled();
}

bool SCustomizableObjecEditorPerformanceReport::IsGetWorstInstanceButtonEnabled() const
{
	return WorstTimeInstance && WorstTimeInstance != CurrentReportWorstCase;
}

FReply SCustomizableObjecEditorPerformanceReport::ResetPerformanceReportOptions()
{
	StressTestOptionInstanceCount = 30;
	StressTestOptionCreateInstanceTime = 200;
	StressTestOptionCreateInstanceTimeVar = 200;
	StressTestOptionInstanceLifeTime = 5000;
	StressTestOptionInstanceLifeTimeVar = 5000;
	StressTestOptionInstanceUpdateTime = 500;
	StressTestOptionInstanceUpdateTimeVar = 500;
	return FReply::Handled();
}

TOptional<int32> SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceCount() const
{
	return StressTestOptionInstanceCount;
}

void SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceCountChanged(int Value)
{
	StressTestOptionInstanceCount = Value;
}

TOptional<int32> SCustomizableObjecEditorPerformanceReport::GetStressTestOptionCreateInstanceTime() const
{
	return StressTestOptionCreateInstanceTime;
}

void SCustomizableObjecEditorPerformanceReport::OnStressTestOptionCreateInstanceTimeChanged(int Value)
{
	StressTestOptionCreateInstanceTime = Value;
}

TOptional<int32> SCustomizableObjecEditorPerformanceReport::GetStressTestOptionCreateInstanceTimeVar() const
{
	return StressTestOptionCreateInstanceTimeVar;
}

void SCustomizableObjecEditorPerformanceReport::OnStressTestOptionCreateInstanceTimeVarChanged(int Value)
{
	StressTestOptionCreateInstanceTimeVar = Value;
}

TOptional<int32> SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceUpdateTime() const
{
	return StressTestOptionInstanceUpdateTime;
}

void SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceUpdateTimeChanged(int Value)
{
	StressTestOptionInstanceUpdateTime = Value;
}

TOptional<int32> SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceUpdateTimeVar() const
{
	return StressTestOptionInstanceUpdateTimeVar;
}

void SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceUpdateTimeVarChanged(int Value)
{
	StressTestOptionInstanceUpdateTimeVar = Value;
}

TOptional<int32> SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceLifeTime() const
{
	return StressTestOptionInstanceLifeTime;
}

void SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceLifeTimeChanged(int Value)
{
	StressTestOptionInstanceLifeTime = Value;
}

TOptional<int32> SCustomizableObjecEditorPerformanceReport::GetStressTestOptionInstanceLifeTimeVar() const
{
	return StressTestOptionInstanceLifeTimeVar;
}

void SCustomizableObjecEditorPerformanceReport::OnStressTestOptionInstanceLifeTimeVarChanged(int Value)
{
	StressTestOptionInstanceLifeTimeVar = Value;
}

void SCustomizableObjecEditorPerformanceReport::ResetSavedData()
{
	if (ReportSubject)
	{
		NumLOD = ReportSubject->GetNumLODs();
		NumStates = ReportSubject->GetStateCount();


		WorstTextureInstances.Empty(NumLOD);
		WorstTextureInstances.SetNum(NumLOD);
		for (uint32 i = 0; i < NumLOD; ++i)
		{
			WorstTextureInstances[i] = NewObject<UWorstCasePerformanceReportInstance>(GetTransientPackage(), NAME_None, RF_Transient);
			WorstTextureInstances[i]->LOD = i;
			WorstTextureInstances[i]->WorstCaseType = UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::LARGEST_TEXTURE_SIZE;
		}

		WorstGeometryInstances.Empty(NumLOD);
		WorstGeometryInstances.SetNum(NumLOD);
		for (uint32 i = 0; i < NumLOD; ++i)
		{
			WorstGeometryInstances[i] = NewObject<UWorstCasePerformanceReportInstance>(GetTransientPackage(), NAME_None, RF_Transient);
			WorstGeometryInstances[i]->LOD = i;
			WorstGeometryInstances[i]->WorstCaseType = UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::MOST_TRIANGLES;
		}

		WorstTimeInstance = nullptr;

		// TArray<TArray<UWorstCasePerformanceReportInstance*>> WorstTimeStateInstances;	// By LOD then by state index
		WorstTimeStateInstances.Empty(NumLOD);
		WorstTimeStateInstances.SetNum(NumLOD);
		for (uint32 i = 0; i < NumLOD; ++i)
		{
			WorstTimeStateInstances[i].Empty(NumStates);
			WorstTimeStateInstances[i].SetNum(NumStates);
			for (uint32 j = 0; j < NumStates; ++j)
			{
				WorstTimeStateInstances[i][j] = NewObject<UWorstCasePerformanceReportInstance>(GetTransientPackage(), NAME_None, RF_Transient);
				WorstTimeStateInstances[i][j]->LOD = i;
				WorstTimeStateInstances[i][j]->WorstCaseType = UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE;
			}
		}

		// TArray<TArray<TArray<UWorstCasePerformanceReportInstance*>>> WorstTimeParameterInstances;	// By LOD then by state index then by parameter index
		WorstTimeParameterInstances.Empty(NumLOD);
		WorstTimeParameterInstances.SetNum(NumLOD);
		for (uint32 i = 0; i < NumLOD; ++i)
		{
			WorstTimeParameterInstances[i].Empty(NumStates);
			WorstTimeParameterInstances[i].SetNum(NumStates);
			for (uint32 j = 0; j < NumStates; ++j)
			{
				WorstTimeParameterInstances[i][j].Reset();
			}
		}
	}
}

void SCustomizableObjecEditorPerformanceReport::WorstFacesFound(uint32 CurrentLODNumFaces, uint32 LOD, UCustomizableObjectInstance* Instance, MeasuredData* data)
{
	// If any of there checks fails, we are receiving updates from a different CO than this was initialized with.
	check(WorstGeometryInstances.Num() > (int32)LOD);
	check(WorstGeometryInstances[LOD]);
	check(WorstGeometryInstances[LOD]->LOD == LOD);
	check(WorstGeometryInstances[LOD]->WorstCaseType == UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::MOST_TRIANGLES);

	bool bSomethingUpdated = false;
	UWorstCasePerformanceReportInstance* WorstCase = WorstGeometryInstances[LOD];
	if ((int32)CurrentLODNumFaces > WorstGeometryInstances[LOD]->NumFaces)
	{
		WorstCase->SetInstance(Instance);
		WorstCase->NumFaces = CurrentLODNumFaces;
		bSomethingUpdated = true;
	}

	if (data)
	{
		WorstCase->MeasuredDataForThisWorstCase = *data;
		bSomethingUpdated = true;
	}

	if (bSomethingUpdated)
	{
		BuildGeometryWorstCasesView();
	}
}

void SCustomizableObjecEditorPerformanceReport::WorstTextureFound(MaterialInfoMap MapMaterialParam, uint32 CurrentLODTextureSize, uint32 LOD, UCustomizableObjectInstance* Instance, MeasuredData* data)
{
	// If any of there checks fails, we are receiving updates from a different CO than this was initialized with.
	check(WorstTextureInstances.Num() > (int32)LOD);
	check(WorstTextureInstances[LOD]);
	check(WorstTextureInstances[LOD]->LOD == LOD);
	check(WorstTextureInstances[LOD]->WorstCaseType == UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::LARGEST_TEXTURE_SIZE);

	bool bSomethingUpdated = false;
	UWorstCasePerformanceReportInstance* WorstCase = WorstTextureInstances[LOD];
	if ((int32)CurrentLODTextureSize > WorstTextureInstances[LOD]->TextureBytes)
	{
		WorstCase->SetInstance(Instance);
		WorstCase->TextureBytes = CurrentLODTextureSize;
		bSomethingUpdated = true;
	}

	if (data)
	{
		WorstCase->MeasuredDataForThisWorstCase = *data;
		bSomethingUpdated = true;
	}

	if (bSomethingUpdated)
	{
		BuildTextureWorstCasesView();
	}
}

void SCustomizableObjecEditorPerformanceReport::WorstTimeFound(float CurrentInstanceGenerationTime, int32 LongesTimeStateIndex, int32 LongesTimeParameterIndexInCO, uint32 LOD, class UCustomizableObjectInstance* Instance, MeasuredData* data)
{
	UWorstCasePerformanceReportInstance* WorstCase = nullptr;
	bool bIsWorstCaseState = LongesTimeParameterIndexInCO == INDEX_NONE;
	bool IsNewPArameter = false;

	if (bIsWorstCaseState)
	{
		// If any of there checks fails, we are receiving updates from a different CO than this was initialized with.
		check(WorstTimeStateInstances.Num() > (int32)LOD);
		check(WorstTimeStateInstances[LOD].Num() > LongesTimeStateIndex);
		check(WorstTimeStateInstances[LOD][LongesTimeStateIndex]);
		check(WorstTimeStateInstances[LOD][LongesTimeStateIndex]->LOD == LOD);
		check(WorstTimeStateInstances[LOD][LongesTimeStateIndex]->WorstCaseType == UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE);

		if (CurrentInstanceGenerationTime > WorstTimeStateInstances[LOD][LongesTimeStateIndex]->UpdateTime)
		{
			WorstCase = WorstTimeStateInstances[LOD][LongesTimeStateIndex];
		}
	}
	else
	{
		// If any of there checks fails, we are receiving updates from a different CO than this was initialized with.
		check(WorstTimeParameterInstances.Num() > (int32)LOD);
		check(WorstTimeParameterInstances[LOD].Num() > LongesTimeStateIndex);
		UWorstCasePerformanceReportInstance* WorstCaseParameterTime = nullptr;

		for (UWorstCasePerformanceReportInstance*& WCP : WorstTimeParameterInstances[LOD][LongesTimeStateIndex])
		{
			if (LongesTimeParameterIndexInCO == WCP->LongestUpdateTimeParameterIndexInCO)
			{
				WorstCaseParameterTime = WCP;
				// If any of there checks fails, we are receiving updates from a different CO than this was initialized with.
				check(WorstCaseParameterTime);
				check(WorstCaseParameterTime->LOD == LOD);
				check(WorstCaseParameterTime->WorstCaseType == UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE);
				break;
			}
		}

		if (!WorstCaseParameterTime)
		{
			WorstCaseParameterTime = NewObject<UWorstCasePerformanceReportInstance>(GetTransientPackage(), NAME_None, RF_Transient);
			WorstCaseParameterTime->LOD = LOD;
			WorstCaseParameterTime->WorstCaseType = UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE;
			WorstTimeParameterInstances[LOD][LongesTimeStateIndex].Push(WorstCaseParameterTime);
			IsNewPArameter = true;
		}

		if (WorstCaseParameterTime && CurrentInstanceGenerationTime > WorstCaseParameterTime->UpdateTime)
		{
			WorstCase = WorstCaseParameterTime;
		}
	}

	if (WorstCase)
	{
		WorstCase->SetInstance(Instance);
		if (WorstCase->WorstCaseInstance)
		{
			WorstCase->WorstCaseInstance->SetState(LongesTimeStateIndex);
		}
		WorstCase->LongestUpdateTimeStateIndex = LongesTimeStateIndex;
		WorstCase->LongestUpdateTimeParameterIndexInCO = LongesTimeParameterIndexInCO;
		WorstCase->UpdateTime = CurrentInstanceGenerationTime;
		if (data)
		{
			WorstCase->MeasuredDataForThisWorstCase = *data;
		}
		UpdateWorstTimeInstance(WorstCase);
		BuildTimeWorstCasesDetailedView();
	}
}

void SCustomizableObjecEditorPerformanceReport::UpdateWorstTimeInstance(const UWorstCasePerformanceReportInstance* EvenWorseTime)
{
	if (!WorstTimeInstance || (EvenWorseTime && (EvenWorseTime->UpdateTime > WorstTimeInstance->UpdateTime)))
	{
		WorstTimeInstance = EvenWorseTime;
	}
}

void SCustomizableObjecEditorPerformanceReport::StressTestEnded()
{
	if (ReportSubject)
	{
		StateTest = MakeShareable(new FRuntimeTest);
		StateTest->WorstTimeDelegate.BindRaw(this, &SCustomizableObjecEditorPerformanceReport::WorstTimeFound);
		StateTest->StateTestEnded.BindRaw(this, &SCustomizableObjecEditorPerformanceReport::StateTestEnded);
		StateTest->StartTest(ReportSubject);
		StatusText->SetText(LOCTEXT("PerformanceReportStatusStateRunning", "Running State test."));
	}
}

void SCustomizableObjecEditorPerformanceReport::StateTestEnded()
{
	StatusText->SetText(LOCTEXT("PerformanceReportStatusFinished", "Tests completed."));
}

UWorstCasePerformanceReportInstance* SCustomizableObjecEditorPerformanceReport::GetWorstCaseStructureFromIndexes(UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType WorstCaseType, uint32 LOD, int32 StateID, int32 ParamID) const
{
	if (LOD >= 0 && LOD < NumLOD)
	{
		switch (WorstCaseType)
		{
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::LARGEST_TEXTURE_SIZE:
		{
			check((uint32)WorstTextureInstances.Num() > LOD);
			return WorstTextureInstances[LOD];
		}
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::MOST_TRIANGLES:
		{
			check((uint32)WorstGeometryInstances.Num() > LOD);
			return WorstGeometryInstances[LOD];
		}
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE:
		{
			if (StateID != INDEX_NONE)
			{
				check((uint32)WorstTimeStateInstances.Num() > LOD);
				check(StateID >= 0);
				check(WorstTimeStateInstances[LOD].Num() > StateID);
				return WorstTimeStateInstances[LOD][StateID];
			}
		}
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE:
		{
			if (StateID != INDEX_NONE && ParamID != INDEX_NONE)
			{
				check((uint32)WorstTimeParameterInstances.Num() > LOD);
				check(StateID >= 0);
				check(WorstTimeParameterInstances[LOD].Num() > StateID);
				const int32 NumParams = WorstTimeParameterInstances[LOD][StateID].Num();
				for (int i = 0; i < NumParams; ++i)
				{
					if (WorstTimeParameterInstances[LOD][StateID][i] && WorstTimeParameterInstances[LOD][StateID][i]->LongestUpdateTimeParameterIndexInCO == ParamID)
					{
						return WorstTimeParameterInstances[LOD][StateID][i];
					}
				}
			}
		}
		}
	}
	return nullptr;
}

TSharedRef<SWidget> SCustomizableObjecEditorPerformanceReport::BuildAnalyzer()
{
	AssetThumbnailPool = MakeShareable(new FAssetThumbnailPool(32));

	TSharedRef<SVerticalBox> ResultVerticalBox = SNew(SVerticalBox)
		.Visibility_Lambda([this]() { return CurrentReportInstance ? EVisibility::Visible : EVisibility::Collapsed; });

	ResultVerticalBox->AddSlot()
	.HAlign(HAlign_Left)
	.AutoHeight()
	[
		SNew(SBorder)
		.Padding(5.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoHeight()
			.Padding(5.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SelectTextureAnalyzerInstance", "Selected Instance: "))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SelectedTextureAnalyzerText, STextBlock)
				]
			]

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5.0f)
				[
					SAssignNew(InstanceThumbnail, SObjectPropertyEntryBox)
					.AllowedClass(UCustomizableObjectInstance::StaticClass())
					.ObjectPath(this, &SCustomizableObjecEditorPerformanceReport::OnGetInstancePath)
					.AllowClear(false)
					.DisplayUseSelected(false)
					.DisplayBrowse(false)
					.EnableContentPicker(false)
					.DisplayThumbnail(true)
					.ThumbnailPool(AssetThumbnailPool)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(5.0f)
				[
					SAssignNew(PerformanceReportRefreshSelectedInstance, SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Danger")
					.IsEnabled(this, &SCustomizableObjecEditorPerformanceReport::IsSelectedInstanceOutdated)
					.OnClicked(this, &SCustomizableObjecEditorPerformanceReport::RefreshSelectedInstance)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RefreshSelectedInstanceButton", "Refresh"))
					]
				]
			]
		]
	];

	ResultVerticalBox->AddSlot()
	.HAlign(HAlign_Fill)
	[
		SNew(SBorder)
		.Padding(5.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				PerformanceReportTextureAnalyzer.ToSharedRef()
			]
		]
	];

	UpdateSelectedInstanceData();

	return ResultVerticalBox;
}

void SCustomizableObjecEditorPerformanceReport::UpdateSelectedInstanceData()
{
	// Refresh Identification text
	if (!SelectedTextureAnalyzerText)
	{
		return;
	}

	FText Result = LOCTEXT("NoInstanceCurrentlySelected", "Nonee currently selected. Click on one of the rows on the left to view it's details.");
	const FNumberFormattingOptions BytesFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(2).SetMinimumFractionalDigits(2);
	const FNumberFormattingOptions TimeFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(2).SetMinimumFractionalDigits(2);
	const FNumberFormattingOptions PolygonsFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(0);
	const FNumberFormattingOptions PercentageFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(0).SetAlwaysSign(true);

	if (CurrentReportInstance && CurrentReportWorstCase)
	{
		const FText TextLOD = FText::AsNumber(CurrentReportWorstCase->LOD, &FNumberFormattingOptions::DefaultNoGrouping());
		FText TextReason;
		FText TextAmount;
		switch (CurrentReportWorstCase->WorstCaseType)
		{
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::LARGEST_TEXTURE_SIZE:
		{
			TextReason = FText::FromString("Largest mutable textures size");
			const float Bytes = CurrentReportWorstCase->TextureBytes;
			const float Value = (Bytes > 1048576) ? float(Bytes) / 1048576.0f : float(Bytes) / 1024.0f;

			TextAmount = FText::Format(LOCTEXT("MutablePerformanceTextureSize", "{0} {1}"), FText::AsNumber(Value, &BytesFormat), FText::FromString((Bytes > 1048576) ? "MB" : "KB"));
			break;
		}
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::MOST_TRIANGLES:
		{
			TextReason = FText::FromString("Most triangles");
			TextAmount = FText::AsNumber(CurrentReportWorstCase->NumFaces, &PolygonsFormat);
			break;
		}
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE:
		{
			TextReason = FText::FromString("Slowest update when entering the state");
			TextAmount = FText::AsNumber(CurrentReportWorstCase->UpdateTime, &TimeFormat);
			break;
		}
		case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE:
		{
			TextReason = FText::FromString("Slowest update when changing parameter");
			TextAmount = FText::AsNumber(CurrentReportWorstCase->UpdateTime, &TimeFormat);
			break;
		}
		default:
		{
			break;
		}
		}

		Result = FText::Format(LOCTEXT("PerformanceReportReason", "{0} for LOD {1} ({2})"), TextReason, TextLOD, TextAmount);
	}

	SelectedTextureAnalyzerText->SetText(Result);

	// Refresh Texture Analyzer
	if (CurrentReportInstance && CurrentReportInstance->IsValidLowLevelFast())
	{
		// If we have at least one skeletal mesh in the instance, we know it's generated (at least once)
		if (CurrentReportInstance->SkeletalMeshes.Num() > 0 && CurrentReportInstance->SkeletalMeshes[0])
		{
			PerformanceReportTextureAnalyzer->RefreshTextureAnalyzerTable(CurrentReportInstance);
		}
		else
		{
			CurrentReportInstance->UpdatedDelegate.AddUniqueDynamic(HelperCallback, &UPerformanceReportHelper::DelegatedCallback);
			CurrentReportInstance->UpdateSkeletalMeshAsync(true, true);
		}
	}
}

void SCustomizableObjecEditorPerformanceReport::BuildTextureWorstCasesView()
{
	const FNumberFormattingOptions BytesFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(2).SetMinimumFractionalDigits(2);

	TextureWorstCasesView->ClearChildren();

	TextureWorstCasesView->AddSlot()
	.HAlign(HAlign_Fill)
	.AutoHeight()
	[
		SNew(SListView<UWorstCasePerformanceReportInstance*>)
		.ListItemsSource(&WorstTextureInstances)
		.OnGenerateRow(this, &SCustomizableObjecEditorPerformanceReport::GenerateRow)
		.OnMouseButtonClick(this, &SCustomizableObjecEditorPerformanceReport::SelectInstance)
		.SelectionMode(ESelectionMode::Single)
		.IsFocusable(true)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnID_PR_LOD_Label)
			.DefaultLabel(LOCTEXT("LOD", "LOD"))
			.FillWidth(0.2)

			+SHeaderRow::Column(ColumnID_PR_TexturesTotalSize_Label)
			.DefaultLabel(LOCTEXT("TexturesTotalSize", "Textures total size"))
			.FillWidth(0.2)

			+ SHeaderRow::Column(ColumnID_PR_Delta_Label)
			.DefaultLabel(LOCTEXT("Delta", "Difference"))
			.FillWidth(0.1)

			+ SHeaderRow::Column(ColumnID_PR_Average_Label)
			.DefaultLabel(LOCTEXT("Average", "Average"))
			.FillWidth(0.1)
		)
	];
}

void SCustomizableObjecEditorPerformanceReport::BuildGeometryWorstCasesView()
{
	GeometryWorstCasesView->ClearChildren();

	GeometryWorstCasesView->AddSlot()
	.HAlign(HAlign_Fill)
	.AutoHeight()
	[
		SNew(SListView<UWorstCasePerformanceReportInstance*>)
		.ListItemsSource(&WorstGeometryInstances)
		.OnGenerateRow(this, &SCustomizableObjecEditorPerformanceReport::GenerateRow)
		.OnMouseButtonClick(this, &SCustomizableObjecEditorPerformanceReport::SelectInstance)
		.SelectionMode(ESelectionMode::Single)
		.IsFocusable(true)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnID_PR_LOD_Label)
			.DefaultLabel(LOCTEXT("LOD", "LOD"))
			.FillWidth(0.2)

			+ SHeaderRow::Column(ColumnID_PR_NumberOfTriangles_Label)
			.DefaultLabel(LOCTEXT("NumberOfTriangles", "Number of triangles"))
			.FillWidth(0.2)

			+ SHeaderRow::Column(ColumnID_PR_Delta_Label)
			.DefaultLabel(LOCTEXT("Delta", "Difference"))
			.FillWidth(0.1)

			+ SHeaderRow::Column(ColumnID_PR_Average_Label)
			.DefaultLabel(LOCTEXT("Average", "Average"))
			.FillWidth(0.1)
		)
	];
}

void SCustomizableObjecEditorPerformanceReport::BuildTimeWorstCasesDetailedView()
{
	const FNumberFormattingOptions TimeFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(2).SetMinimumFractionalDigits(2);

	TimeWorstCasesDetailedView->ClearChildren();

	for (int32 LOD = 0; LOD < WorstTimeStateInstances.Num(); ++LOD)
	{
		// Currently, states test only returns LOD 0 data
		if (LOD != 0)
		{
			continue;
		}

		const FText TextLOD = FText::AsNumber(LOD, &FNumberFormattingOptions::DefaultNoGrouping());

		TimeWorstCasesDetailedView->AddSlot()
		.HAlign(HAlign_Left)
		.Padding(5.0f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("MutableLODNTitle", "LOD {0}:"), TextLOD))
		];

		TimeWorstCasesDetailedView->AddSlot()
		.HAlign(HAlign_Fill)
		[
			SNew(SListView<UWorstCasePerformanceReportInstance*>)
			.ListItemsSource(&WorstTimeStateInstances[LOD])
			.OnGenerateRow(this, &SCustomizableObjecEditorPerformanceReport::GenerateRow)
			.OnMouseButtonClick(this, &SCustomizableObjecEditorPerformanceReport::SelectInstance)
			.SelectionMode(ESelectionMode::Single)
			.IsFocusable(true)
			.HeaderRow
			(
				SAssignNew(TimeWorstCasesViewHeader, SHeaderRow)
				+ SHeaderRow::Column(ColumnID_PR_Name_Label)
				.DefaultLabel(LOCTEXT("Name", "Name"))
				.FillWidth(0.2)

				+ SHeaderRow::Column(ColumnID_PR_UpdateTime_Label)
				.DefaultLabel(LOCTEXT("StateEnterUpdateTime", "Update time (ms)"))
				.FillWidth(0.2)

				+ SHeaderRow::Column(ColumnID_PR_Delta_Label)
				.DefaultLabel(LOCTEXT("Delta", "Difference"))
				.FillWidth(0.1)

				+ SHeaderRow::Column(ColumnID_PR_Average_Label)
				.DefaultLabel(LOCTEXT("Average", "Average"))
				.FillWidth(0.1)
			)
		];
	}

	TimeWorstCasesDetailedView->Invalidate(EInvalidateWidget::Layout);
}

void SCustomizableObjecEditorPerformanceReport::OnOpenAssetEditor(UObject* ObjectToEdit)
{
	if (ObjectToEdit)
	{
		GEditor->EditObject(ObjectToEdit);
	}
}

void SCustomizableObjecEditorPerformanceReport::ChangeFocusedReportInstance(UWorstCasePerformanceReportInstance* InReportInstance)
{
	CurrentReportWorstCase = InReportInstance;
}

FString SCustomizableObjecEditorPerformanceReport::OnGetInstancePath() const
{
	if (CurrentReportInstance && CurrentReportInstance->IsValidLowLevelFast(false))
	{
		return CurrentReportInstance->GetPathName();
	}

	return FString();
}

bool SCustomizableObjecEditorPerformanceReport::IsSelectedInstanceOutdated() const
{
	if (CurrentReportWorstCase)
	{
		return CurrentReportWorstCase->WorstCaseInstance != CurrentReportInstance;
		// // Alternative method to checking if a selected instance is outdated, if in the future, CurrentReportInstnce and CurrentReportWorstCase->WorstCaseInstance are kept synchronized
		//const UWorstCasePerformanceReportInstance* InstanceInData = GetWorstCaseStructureFromIndexes(CurrentReportWorstCase->WorstCaseType, CurrentReportWorstCase->LOD, CurrentReportWorstCase->LongestUpdateTimeStateIndex, CurrentReportWorstCase->LongestUpdateTimeParameterIndexInCO);
		//if (InstanceInData)
		//{
		//	switch (CurrentReportWorstCase->WorstCaseType)
		//	{
		//	case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::LARGEST_TEXTURE_SIZE:
		//	{
		//		if (InstanceInData->TextureBytes > CurrentReportWorstCase->TextureBytes)
		//		{
		//			return true;
		//		}
		//		break;
		//	}
		//	case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::MOST_TRIANGLES:
		//	{
		//		if (InstanceInData->NumFaces > CurrentReportWorstCase->NumFaces)
		//		{
		//			return true;
		//		}
		//		break;
		//	}
		//	case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE:
		//	case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE:
		//	{
		//		if (InstanceInData->UpdateTime > CurrentReportWorstCase->UpdateTime)
		//		{
		//			return true;
		//		}
		//		break;
		//	}
		//	}
		//}
	}
	return false;
}

FReply SCustomizableObjecEditorPerformanceReport::RefreshSelectedInstance()
{
	CurrentReportInstance = CurrentReportWorstCase->WorstCaseInstance;
	UpdateSelectedInstanceData();

	return FReply::Handled();
}

TSharedRef<ITableRow> SCustomizableObjecEditorPerformanceReport::GenerateRow(UWorstCasePerformanceReportInstance* ReportedInstance, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPerformanceReportWorstCaseRow, OwnerTable).ReportInstance(ReportedInstance).PerformanceReport(this).Style(&FCustomizableObjectEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("CustomizableObjectPerformanceReport.Row"));
}

const TArray<UWorstCasePerformanceReportInstance*>* SCustomizableObjecEditorPerformanceReport::GetWorstTimeParameters(uint32 LOD, uint32 StateIdx) const
{
	check((uint32)WorstTimeParameterInstances.Num() > LOD);
	//check((uint32)WorstTimeParameterInstances[LOD].Num() > StateIdx);

	if ((uint32)WorstTimeParameterInstances.Num() > LOD && (uint32)WorstTimeParameterInstances[LOD].Num() > StateIdx)
	{
		return &WorstTimeParameterInstances[LOD][StateIdx];
	}

	return nullptr;
}

void SCustomizableObjecEditorPerformanceReport::SelectInstance(UWorstCasePerformanceReportInstance* ClickedInstance)
{
	if (CurrentReportInstance)
	{
		CurrentReportInstance->UpdatedDelegate.RemoveDynamic(HelperCallback, &UPerformanceReportHelper::DelegatedCallback);
	}
	CurrentReportWorstCase = ClickedInstance;
	CurrentReportInstance = ClickedInstance->WorstCaseInstance;
	UpdateSelectedInstanceData();

	BuildTextureWorstCasesView();
	BuildGeometryWorstCasesView();
	BuildTimeWorstCasesDetailedView();
}

void SCustomizableObjecEditorPerformanceReport::SelectInstanceFinishedUpdate(UCustomizableObjectInstance* UpdatedInstance)
{
	if (CurrentReportInstance && UpdatedInstance == CurrentReportInstance)
	{
		PerformanceReportTextureAnalyzer->RefreshTextureAnalyzerTable(CurrentReportInstance);
	}
}

void SPerformanceReportWorstCaseRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	ReportInstance = InArgs._ReportInstance;
	PerformanceReport = InArgs._PerformanceReport;
	SMutableExpandableTableRow<UWorstCasePerformanceReportInstance*>::Construct(FSuperRowType::FArguments().Style(InArgs._Style), InOwnerTableView);
	Style = InArgs._Style;
}

void SPerformanceReportWorstCaseRow::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ReportInstance);
	Collector.AddReferencedObjects(SubWidgetData);
}

TSharedRef<SWidget> SPerformanceReportWorstCaseRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	check(ReportInstance);

	const FNumberFormattingOptions BytesFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(2).SetMinimumFractionalDigits(2);
	const FNumberFormattingOptions TimeFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(2).SetMinimumFractionalDigits(2);
	const FNumberFormattingOptions PolygonsFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(0);
	const FNumberFormattingOptions PercentageFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(0).SetAlwaysSign(true);
	const bool bIsSelected = PerformanceReport && ReportInstance == PerformanceReport->CurrentReportWorstCase;
	TSharedRef<SOverlay> ResultWidget = SNew(SOverlay);

	if (bIsSelected)
	{
		ResultWidget->AddSlot()
		[
			SNew(SBorder)
			.BorderImage(this, &SPerformanceReportWorstCaseRow::GetSelectedRowHighlightColor)
		];
	}

	if (ReportInstance->WorstCaseInstance)
	{
		if (ColumnName == ColumnID_PR_Delta_Label)
		{
			const float Mean = ReportInstance->MeasuredDataForThisWorstCase.MeanTime;
			if (Mean)
			{
				float CurrentMaximum = 0.0f;

				switch (ReportInstance->WorstCaseType)
				{
				case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::LARGEST_TEXTURE_SIZE:
				{
					CurrentMaximum = (float)ReportInstance->TextureBytes;
					break;
				}
				case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::MOST_TRIANGLES:
				{
					CurrentMaximum = (float)ReportInstance->NumFaces;
					break;
				}
				case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE:
				case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE:
				{
					CurrentMaximum = ReportInstance->UpdateTime;
					break;
				}
				default:
				{
					break;
				}
				}

				FText Percentage = FText::AsNumber((CurrentMaximum * 100.0f / Mean) - 100.0f, &PercentageFormat);

				ResultWidget->AddSlot()
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("MutablePerformanceDeltaFromAverage", "{0}%"), Percentage))
				];
			}
		}
		else if (ColumnName == ColumnID_PR_Average_Label)
		{
			const float Mean = ReportInstance->MeasuredDataForThisWorstCase.MeanTime;
			if (Mean)
			{
				switch (ReportInstance->WorstCaseType)
				{
				case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::LARGEST_TEXTURE_SIZE:
				{
					const float Value = (Mean > 1048576) ? float(Mean) / 1048576.0f : float(Mean) / 1024.0f;

					ResultWidget->AddSlot()
					[
						SNew(STextBlock).Text(FText::Format(LOCTEXT("MutablePerformanceTextureSize", "{0} {1}"), FText::AsNumber(Value, &BytesFormat), FText::FromString((Mean > 1048576) ? "MB" : "KB")))
					];
					break;
				}
				case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::MOST_TRIANGLES:
				{
					ResultWidget->AddSlot()
					[
						SNew(STextBlock).Text(FText::AsNumber(Mean, &PolygonsFormat))
					];
					break;
				}
				case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE:
				case UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE:
				{
					ResultWidget->AddSlot()
					[
						SNew(STextBlock).Text(FText::AsNumber(Mean, &TimeFormat))
					];
					break;
				}
				default:
				{
					break;
				}
				}
			}
		}
		else if (ColumnName == ColumnID_PR_Name_Label)
		{
			if (const UCustomizableObject* CustomizableObject = ReportInstance->WorstCaseInstance->GetCustomizableObject();
				CustomizableObject &&
				!CustomizableObject->IsLocked())
			{
				const bool bIsDetailParam = ReportInstance->WorstCaseType == UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE;
				const FText TextState = ReportInstance->WorstCaseType == UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE ?
					FText::FromString("State: " + CustomizableObject->GetStateName(ReportInstance->LongestUpdateTimeStateIndex))
					: FText::FromString((bIsDetailParam ? "		" : "") + CustomizableObject->GetParameterName(ReportInstance->LongestUpdateTimeParameterIndexInCO));

				ResultWidget->AddSlot()
				[
					SNew(STextBlock).Text(TextState)
				];
			}
		}
		else if (ColumnName == ColumnID_PR_UpdateTime_Label)
		{
			ResultWidget->AddSlot()
			[
				SNew(STextBlock).Text(FText::AsNumber(ReportInstance->UpdateTime, &TimeFormat))
			];
		}
		else if (ColumnName == ColumnID_PR_LOD_Label)
		{
			const FText TextLOD = FText::AsNumber(ReportInstance->LOD, &FNumberFormattingOptions::DefaultNoGrouping());

			ResultWidget->AddSlot()
			[
				SNew(STextBlock).Text(FText::Format(LOCTEXT("MutablePerformanceLOD", "{0}"), TextLOD))
			];
		}
		else if (ColumnName == ColumnID_PR_TexturesTotalSize_Label)
		{
			const float Bytes = ReportInstance->TextureBytes;
			const float Value = (Bytes > 1048576) ? float(Bytes) / 1048576.0f : float(Bytes) / 1024.0f;

			ResultWidget->AddSlot()
			[
				SNew(STextBlock).Text(FText::Format(LOCTEXT("MutablePerformanceTextureSize", "{0} {1}"), FText::AsNumber(Value, &BytesFormat), FText::FromString((Bytes > 1048576) ? "MB" : "KB")))
			];
		}
		else if (ColumnName == ColumnID_PR_NumberOfTriangles_Label)
		{
			ResultWidget->AddSlot()
			[
				SNew(STextBlock).Text(FText::AsNumber(ReportInstance->NumFaces, &PolygonsFormat))
			];
		}
		else if (ColumnName == ColumnID_PR_InstanceName_Label)
		{
			const FText InstanceName = FText::FromString(ReportInstance->GetName());

			ResultWidget->AddSlot()
			[
				SNew(STextBlock).Text(InstanceName)
			];
		}
	}

	return ResultWidget;
}

TSharedPtr<SWidget> SPerformanceReportWorstCaseRow::GenerateAdditionalWidgetForRow()
{
	if (ReportInstance && ReportInstance->WorstCaseType == UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType::SLOWEST_UPDATE_ENTER_STATE)
	{
		const TArray<UWorstCasePerformanceReportInstance*>* WorstTimeParameters = PerformanceReport->GetWorstTimeParameters(ReportInstance->LOD, ReportInstance->LongestUpdateTimeStateIndex);

		if (WorstTimeParameters)
		{
			SubWidgetData.Empty(WorstTimeParameters->Num());
			for (UWorstCasePerformanceReportInstance* elem : *WorstTimeParameters)
			{
				SubWidgetData.Push(elem);
			}
			return SNew(SListView<UWorstCasePerformanceReportInstance*>)
				.ListItemsSource(&SubWidgetData)
				.OnGenerateRow(this, &SPerformanceReportWorstCaseRow::GenerateStyledRow)
				.OnMouseButtonClick(PerformanceReport, &SCustomizableObjecEditorPerformanceReport::SelectInstance)
				.SelectionMode(ESelectionMode::Single)
				.IsFocusable(true)
				.HeaderRow(PerformanceReport->TimeWorstCasesViewHeader);
		}
	}

	SubWidgetData.Empty();
	return nullptr;
}

EVisibility SPerformanceReportWorstCaseRow::GetAdditionalWidgetDefaultVisibility() const
{
	return (ReportInstance) ? ReportInstance->SubVisibility : EVisibility::Collapsed;
}

void SPerformanceReportWorstCaseRow::SetAdditionalWidgetVisibility(const EVisibility InVisibility)
{
	if (ReportInstance)
	{
		ReportInstance->SubVisibility = InVisibility;
	}
	SMutableExpandableTableRow::SetAdditionalWidgetVisibility(InVisibility);
}

const FSlateBrush* SPerformanceReportWorstCaseRow::GetSelectedRowHighlightColor() const
{
	const bool bIsSelected = PerformanceReport && ReportInstance == PerformanceReport->CurrentReportWorstCase;
	const bool bIsUpToDate = bIsSelected && ReportInstance && ReportInstance->WorstCaseInstance == PerformanceReport->CurrentReportInstance;
	return bIsUpToDate? FAppStyle::GetBrush("Brushes.Highlight") : FAppStyle::GetBrush("Brushes.AccentGray");
}

TSharedRef<ITableRow> SPerformanceReportWorstCaseRow::GenerateStyledRow(UWorstCasePerformanceReportInstance* ReportedInstance, const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bEvenEntryIndex = (GetIndexInList() % 2 == 0);
	return SNew(SPerformanceReportWorstCaseRow, OwnerTable).ReportInstance(ReportedInstance).PerformanceReport(PerformanceReport).Style(
		bEvenEntryIndex ?
		&FCustomizableObjectEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("CustomizableObjectPerformanceReportAllEven.Row")
		: &FCustomizableObjectEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("CustomizableObjectPerformanceReportAllOdd.Row")
	);
}

void UPerformanceReportHelper::DelegatedCallback(UCustomizableObjectInstance* Instance)
{
	if (PerformanceReport)
	{
		PerformanceReport->SelectInstanceFinishedUpdate(Instance);
	}
}

#undef LOCTEXT_NAMESPACE
