// Copyright Epic Games, Inc. All Rights Reserved.

// Movie Pipeline Includes
#include "Widgets/SMoviePipelineQueuePanel.h"
#include "Widgets/MoviePipelineWidgetConstants.h"
#include "SMoviePipelineQueueEditor.h"
#include "SMoviePipelineConfigPanel.h"
#include "MovieRenderPipelineSettings.h"
#include "MovieRenderPipelineStyle.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineQueueSubsystem.h"

// Slate Includes
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SPrimaryButton.h"

// ContentBrowser Includes
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

// Misc
#include "Editor.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"

// UnrealEd Includes
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "AssetToolsModule.h"
#include "Misc/FileHelper.h"


#define LOCTEXT_NAMESPACE "SMoviePipelineQueuePanel"

UE_DISABLE_OPTIMIZATION_SHIP
void SMoviePipelineQueuePanel::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.ColumnWidth = 0.7f;

	JobDetailsPanelWidget = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	// Create the child widgets that need to know about our pipeline
	PipelineQueueEditorWidget = SNew(SMoviePipelineQueueEditor)
		.OnEditConfigRequested(this, &SMoviePipelineQueuePanel::OnEditJobConfigRequested)
		.OnPresetChosen(this, &SMoviePipelineQueuePanel::OnJobPresetChosen)
		.OnJobSelectionChanged(this, &SMoviePipelineQueuePanel::OnSelectionChanged);


	{
		// Automatically select the first job in the queue
		UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
		check(Subsystem);

		TArray<UMoviePipelineExecutorJob*> Jobs;
		if (Subsystem->GetQueue()->GetJobs().Num() > 0)
		{
			Jobs.Add(Subsystem->GetQueue()->GetJobs()[0]);
		}

		// Go through the UI so it updates the UI selection too and then this will loop back
		// around to OnSelectionChanged to update ourself.
		PipelineQueueEditorWidget->SetSelectedJobs(Jobs);
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		// Create the toolbar for adding new items to the queue
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.f, 1.0f))
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			.BorderBackgroundColor(FLinearColor(.6, .6, .6, 1.0f))
			[
				SNew(SHorizontalBox)

				// Add a Level Sequence to the queue 
				+ SHorizontalBox::Slot()
				.Padding(MoviePipeline::ButtonOffset)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					PipelineQueueEditorWidget->MakeAddSequenceJobButton()
				]

				// Remove a job (potentially already processed) from the the queue 
				+ SHorizontalBox::Slot()
				.Padding(MoviePipeline::ButtonOffset)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					PipelineQueueEditorWidget->RemoveSelectedJobButton()
				]	

				// Spacer
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillWidth(1.f)
				[
					SNullWidget::NullWidget
				]

				// Presets Management Button
				+ SHorizontalBox::Slot()
				.Padding(MoviePipeline::ButtonOffset)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(SComboButton)
					.ToolTipText(LOCTEXT("QueueManagementButton_Tooltip", "Export the current queue to an asset, or load a previously saved queue."))
					.ContentPadding(MoviePipeline::ButtonPadding)
					.ComboButtonStyle(FMovieRenderPipelineStyle::Get(), "ComboButton")
					.OnGetMenuContent(this, &SMoviePipelineQueuePanel::OnGenerateSavedQueuesMenu)
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(0, 1, 4, 0)
						.AutoWidth()
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("AssetEditor.SaveAsset"))
						]

						+ SHorizontalBox::Slot()
						.Padding(0, 1, 0, 0)
						[
							SNew(STextBlock)
							.Text(this, &SMoviePipelineQueuePanel::GetQueueMenuButtonText)
						]
					]
				]
			]
		]
	
		// Main Queue Body
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(3)
			[
				PipelineQueueEditorWidget.ToSharedRef()

			]
			+ SSplitter::Slot()
			.Value(1)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(1.f, 1.0f))
				.Content()
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex(this, &SMoviePipelineQueuePanel::GetDetailsViewWidgetIndex)
					.IsEnabled(this, &SMoviePipelineQueuePanel::IsDetailsViewEnabled)
					+ SWidgetSwitcher::Slot()
					[
						JobDetailsPanelWidget.ToSharedRef()
					]
					+ SWidgetSwitcher::Slot()
					.Padding(2.0f, 24.0f, 2.0f, 2.0f)
					[
						SNew(SBox)
						.HAlign(EHorizontalAlignment::HAlign_Center)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("NoJobSelected", "Select a job to view details."))
						]
					]
				]
			]
		]

		// Footer Bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			.BorderBackgroundColor(FLinearColor(.6, .6, .6, 1.0f))
			.Padding(FMargin(0, 2, 0, 2))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.FillWidth(1.f)
				[
					SNullWidget::NullWidget
				]

				// Render Local in Process
				+ SHorizontalBox::Slot()
				.Padding(MoviePipeline::ButtonOffset)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(SPrimaryButton)
					.Text(LOCTEXT("RenderQueueLocal_Text", "Render (Local)"))
					.ToolTipText(LOCTEXT("RenderQueueLocal_Tooltip", "Renders the current queue in the current process using Play in Editor."))
					.IsEnabled(this, &SMoviePipelineQueuePanel::IsRenderLocalEnabled)
					.OnClicked(this, &SMoviePipelineQueuePanel::OnRenderLocalRequested)
				]

				// Render Remotely (Separate Process or Farm)
				+ SHorizontalBox::Slot()
				.Padding(MoviePipeline::ButtonOffset)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(SPrimaryButton)
					.Text(LOCTEXT("RenderQueueRemote_Text", "Render (Remote)"))
					.ToolTipText(LOCTEXT("RenderQueueRemote_Tooltip", "Renders the current queue in a separate process."))
					.IsEnabled(this, &SMoviePipelineQueuePanel::IsRenderRemoteEnabled)
					.OnClicked(this, &SMoviePipelineQueuePanel::OnRenderRemoteRequested)
				]
			]
		]
	];
}

UE_ENABLE_OPTIMIZATION_SHIP

FReply SMoviePipelineQueuePanel::OnRenderLocalRequested()
{
	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);

	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
	TSubclassOf<UMoviePipelineExecutorBase> ExecutorClass = ProjectSettings->DefaultLocalExecutor.TryLoadClass<UMoviePipelineExecutorBase>();

	// OnRenderLocalRequested should only get called if IsRenderLocalEnabled() returns true, meaning there's a valid class.
	check(ExecutorClass != nullptr);
	Subsystem->RenderQueueWithExecutor(ExecutorClass);
	return FReply::Handled();
}

bool SMoviePipelineQueuePanel::IsRenderLocalEnabled() const
{
	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);

	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
	const bool bHasExecutor = ProjectSettings->DefaultLocalExecutor.TryLoadClass<UMoviePipelineExecutorBase>() != nullptr;
	const bool bNotRendering = !Subsystem->IsRendering();
	const bool bConfigWindowIsOpen = WeakEditorWindow.IsValid();

	bool bAtLeastOneJobAvailable = false;
	for (UMoviePipelineExecutorJob* Job : Subsystem->GetQueue()->GetJobs())
	{
		if (!Job->IsConsumed() && Job->IsEnabled())
		{
			bAtLeastOneJobAvailable = true;
			break;
		}
	}

	const bool bWorldIsActive = GEditor->IsPlaySessionInProgress();
	return bHasExecutor && bNotRendering && bAtLeastOneJobAvailable && !bWorldIsActive && !bConfigWindowIsOpen;
}

FReply SMoviePipelineQueuePanel::OnRenderRemoteRequested()
{
	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);

	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
	TSubclassOf<UMoviePipelineExecutorBase> ExecutorClass = ProjectSettings->DefaultRemoteExecutor.TryLoadClass<UMoviePipelineExecutorBase>();

	// OnRenderRemoteRequested should only get called if IsRenderRemoteEnabled() returns true, meaning there's a valid class.
	check(ExecutorClass != nullptr);

	Subsystem->RenderQueueWithExecutor(ExecutorClass);
	return FReply::Handled();
}

bool SMoviePipelineQueuePanel::IsRenderRemoteEnabled() const
{
	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);

	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
	const bool bHasExecutor = ProjectSettings->DefaultRemoteExecutor.TryLoadClass<UMoviePipelineExecutorBase>() != nullptr;
	const bool bNotRendering = !Subsystem->IsRendering();
	const bool bConfigWindowIsOpen = WeakEditorWindow.IsValid();

	bool bAtLeastOneJobAvailable = false;
	for (UMoviePipelineExecutorJob* Job : Subsystem->GetQueue()->GetJobs())
	{
		if (!Job->IsConsumed() && Job->IsEnabled())
		{
			bAtLeastOneJobAvailable = true;
			break;
		}
	}

	return bHasExecutor && bNotRendering && bAtLeastOneJobAvailable && !bConfigWindowIsOpen;
}

void SMoviePipelineQueuePanel::OnJobPresetChosen(TWeakObjectPtr<UMoviePipelineExecutorJob> InJob, TWeakObjectPtr<UMoviePipelineExecutorShot> InShot)
{
	UMovieRenderPipelineProjectSettings* ProjectSettings = GetMutableDefault<UMovieRenderPipelineProjectSettings>();
	if (!InShot.IsValid())
	{
		// Store the preset so the next job they make will use it.
		ProjectSettings->LastPresetOrigin = InJob->GetPresetOrigin();
	}
	ProjectSettings->SaveConfig();
}

void SMoviePipelineQueuePanel::OnEditJobConfigRequested(TWeakObjectPtr<UMoviePipelineExecutorJob> InJob, TWeakObjectPtr<UMoviePipelineExecutorShot> InShot)
{
	// Only allow one editor open at once for now.
	if (WeakEditorWindow.IsValid())
	{
		FWidgetPath ExistingWindowPath;
		if (FSlateApplication::Get().FindPathToWidget(WeakEditorWindow.Pin().ToSharedRef(), ExistingWindowPath, EVisibility::All))
		{
			WeakEditorWindow.Pin()->BringToFront();
			FSlateApplication::Get().SetAllUserFocus(ExistingWindowPath, EFocusCause::SetDirectly);
		}

		return;
	}

	TSubclassOf<UMoviePipelineConfigBase> ConfigType;
	UMoviePipelineConfigBase* BasePreset = nullptr;
	UMoviePipelineConfigBase* BaseConfig = nullptr;
	if (InShot.IsValid())
	{
		ConfigType = UMoviePipelineShotConfig::StaticClass();
		BasePreset = InShot->GetShotOverridePresetOrigin();
		BaseConfig = InShot->GetShotOverrideConfiguration();
	}
	else
	{
		ConfigType = UMoviePipelinePrimaryConfig::StaticClass();
		BasePreset = InJob->GetPresetOrigin();
		BaseConfig = InJob->GetConfiguration();
	}

	TSharedRef<SWindow> EditorWindow =
		SNew(SWindow)
		.ClientSize(FVector2D(700, 600));

	TSharedRef<SMoviePipelineConfigPanel> ConfigEditorPanel =
		SNew(SMoviePipelineConfigPanel, ConfigType)
		.Job(InJob)
		.Shot(InShot)
		.OnConfigurationModified(this, &SMoviePipelineQueuePanel::OnConfigUpdatedForJob)
		.OnConfigurationSetToPreset(this, &SMoviePipelineQueuePanel::OnConfigUpdatedForJobToPreset)
		.BasePreset(BasePreset)
		.BaseConfig(BaseConfig);

	EditorWindow->SetContent(ConfigEditorPanel);


	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(EditorWindow, ParentWindow.ToSharedRef());
	}

	WeakEditorWindow = EditorWindow;
}

void SMoviePipelineQueuePanel::OnConfigWindowClosed()
{
	if (WeakEditorWindow.IsValid())
	{
		WeakEditorWindow.Pin()->RequestDestroyWindow();
	}
}

void SMoviePipelineQueuePanel::OnConfigUpdatedForJob(TWeakObjectPtr<UMoviePipelineExecutorJob> InJob, TWeakObjectPtr<UMoviePipelineExecutorShot> InShot, UMoviePipelineConfigBase* InConfig)
{
	if (InJob.IsValid())
	{
		if (InShot.IsValid())
		{
			if (UMoviePipelineShotConfig* ShotConfig = Cast<UMoviePipelineShotConfig>(InConfig))
			{
				InShot->SetShotOverrideConfiguration(ShotConfig);
			}
		}
		else
		{
			if (UMoviePipelinePrimaryConfig* PrimaryConfig = Cast<UMoviePipelinePrimaryConfig>(InConfig))
			{
				InJob->SetConfiguration(PrimaryConfig);
			}
		}
	}

	OnConfigWindowClosed();
}

void SMoviePipelineQueuePanel::OnConfigUpdatedForJobToPreset(TWeakObjectPtr<UMoviePipelineExecutorJob> InJob, TWeakObjectPtr<UMoviePipelineExecutorShot> InShot, UMoviePipelineConfigBase* InConfig)
{
	if (InJob.IsValid())
	{
		if (InShot.IsValid())
		{
			if (UMoviePipelineShotConfig* ShotConfig = Cast<UMoviePipelineShotConfig>(InConfig))
			{
				InShot->SetShotOverridePresetOrigin(ShotConfig);
			}
		}
		else
		{
			if (UMoviePipelinePrimaryConfig* PrimaryConfig = Cast<UMoviePipelinePrimaryConfig>(InConfig))
			{
				InJob->SetPresetOrigin(PrimaryConfig);
			}
		}
	}

	// Store the preset they used as the last set one
	OnJobPresetChosen(InJob, InShot);

	OnConfigWindowClosed();
}

void SMoviePipelineQueuePanel::OnSelectionChanged(const TArray<UMoviePipelineExecutorJob*>& InSelectedJobs)
{
	TArray<UObject*> Jobs;
	for (UMoviePipelineExecutorJob* Job : InSelectedJobs)
	{
		Jobs.Add(Job);
	}
	
	JobDetailsPanelWidget->SetObjects(Jobs);
	NumSelectedJobs = InSelectedJobs.Num();
}

int32 SMoviePipelineQueuePanel::GetDetailsViewWidgetIndex() const
{
	return NumSelectedJobs == 0;
}

bool SMoviePipelineQueuePanel::IsDetailsViewEnabled() const
{
	TArray<TWeakObjectPtr<UObject>> OutObjects= JobDetailsPanelWidget->GetSelectedObjects();

	bool bAllEnabled = true;
	for (TWeakObjectPtr<UObject> Object : OutObjects)
	{
		const UMoviePipelineExecutorJob* Job = Cast<UMoviePipelineExecutorJob>(Object);
		if (Job && Job->IsConsumed())
		{
			bAllEnabled = false;
			break;
		}
	}

	return bAllEnabled;
}

TSharedRef<SWidget> SMoviePipelineQueuePanel::OnGenerateSavedQueuesMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

	static const FText NoLoadedQueueText = LOCTEXT("NoLoadedQueue", "None");
	static const FText LoadedQueueFormatText = LOCTEXT("CurrentQueueFormat", "Current Queue: {0}");
	const FString QueueName = GetQueueOriginName();
	const FText LoadedQueueName = FText::Format(
		LoadedQueueFormatText, !QueueName.IsEmpty() ? FText::FromString(QueueName) : NoLoadedQueueText);

	MenuBuilder.AddMenuEntry(
		LoadedQueueName,
		LoadedQueueName,
		FSlateIcon(),
		FUIAction(
			FExecuteAction(),
			FCanExecuteAction::CreateLambda([]() { return false; })),
		NAME_None,
		EUserInterfaceActionType::None
	);

	MenuBuilder.AddMenuSeparator();
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("SaveQueue_Text", "Save Queue"),
		LOCTEXT("SaveQueue_Tip", "Save the current configuration in its existing preset which can be shared between multiple jobs, or imported later as the base of a new configuration."),
		FSlateIcon(FAppStyle::Get().GetStyleSetName(), "AssetEditor.SaveAsset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &SMoviePipelineQueuePanel::OnSaveAsset),
			FCanExecuteAction::CreateSP(this, &SMoviePipelineQueuePanel::IsQueueDirty)
		)
	);
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("SaveAsQueue_Text", "Save Queue As"),
		LOCTEXT("SaveAsQueue_Tip", "Save the current configuration as a new preset that can be shared between multiple jobs, or imported later as the base of a new configuration."),
		FSlateIcon(FAppStyle::Get().GetStyleSetName(), "AssetEditor.SaveAssetAs"),
		FUIAction(FExecuteAction::CreateSP(this, &SMoviePipelineQueuePanel::OnSaveAsAsset))
	);

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bShowBottomToolbar = true;
		AssetPickerConfig.bAutohideSearchBar = false;
		AssetPickerConfig.bAllowDragging = false;
		AssetPickerConfig.bCanShowClasses = false;
		AssetPickerConfig.bShowPathInColumnView = true;
		AssetPickerConfig.bShowTypeInColumnView = false;
		AssetPickerConfig.bSortByPathInColumnView = false;

		AssetPickerConfig.AssetShowWarningText = LOCTEXT("NoQueueAssets_Warning", "No Queues Found");
		AssetPickerConfig.Filter.ClassPaths.Add(UMoviePipelineQueue::StaticClass()->GetClassPathName());
		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SMoviePipelineQueuePanel::OnImportSavedQueueAsset);
	}

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("LoadQueue_MenuSection", "Import Queue"));
	{
		TSharedRef<SWidget> PresetPicker = SNew(SBox)
			.MinDesiredWidth(400.f)
			.MinDesiredHeight(400.f)
			[
				ContentBrowser.CreateAssetPicker(AssetPickerConfig)
			];

		MenuBuilder.AddWidget(PresetPicker, FText(), true, false);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FText SMoviePipelineQueuePanel::GetQueueMenuButtonText() const
{
	const FString QueueName = GetQueueOriginName();
	if (!QueueName.IsEmpty())
	{
		return FText::Format(FText::FromString(TEXT("{0}{1}")),
			FText::FromString(QueueName),
			FText::FromString(IsQueueDirty() ? TEXT(" *") : TEXT("")));
	}
	
	return LOCTEXT("QueueSaveMenuUnsavedConfig_Text", "Unsaved Queue *");
}

bool SMoviePipelineQueuePanel::OpenSaveDialog(const FString& InDefaultPath, const FString& InNewNameSuggestion, FString& OutPackageName)
{
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	{
		SaveAssetDialogConfig.DefaultPath = InDefaultPath;
		SaveAssetDialogConfig.DefaultAssetName = InNewNameSuggestion;
		SaveAssetDialogConfig.AssetClassNames.Add(UMoviePipelineQueue::StaticClass()->GetClassPathName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveQueueAssetDialogTitle", "Save Queue Asset");
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!SaveObjectPath.IsEmpty())
	{
		OutPackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		return true;
	}

	return false;
}

bool SMoviePipelineQueuePanel::GetSavePresetPackageName(const FString& InExistingName, FString& OutName)
{
	UMovieRenderPipelineProjectSettings* ConfigSettings = GetMutableDefault<UMovieRenderPipelineProjectSettings>();

	// determine default package path
	const FString DefaultSaveDirectory = ConfigSettings->PresetSaveDir.Path;

	FString DialogStartPath;
	FPackageName::TryConvertFilenameToLongPackageName(DefaultSaveDirectory, DialogStartPath);
	if (DialogStartPath.IsEmpty())
	{
		DialogStartPath = TEXT("/Game");
	}

	// determine default asset name
	FString DefaultName = InExistingName;

	FString UniquePackageName;
	FString UniqueAssetName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(DialogStartPath / DefaultName, TEXT(""), UniquePackageName, UniqueAssetName);

	FString DialogStartName = FPaths::GetCleanFilename(UniqueAssetName);

	FString UserPackageName;
	FString NewPackageName;

	// get destination for asset
	bool bFilenameValid = false;
	while (!bFilenameValid)
	{
		if (!OpenSaveDialog(DialogStartPath, DialogStartName, UserPackageName))
		{
			return false;
		}

		NewPackageName = UserPackageName;

		FText OutError;
		bFilenameValid = FFileHelper::IsFilenameValidForSaving(NewPackageName, OutError);
	}

	// Update to the last location they saved to so it remembers their settings next time.
	ConfigSettings->PresetSaveDir.Path = FPackageName::GetLongPackagePath(UserPackageName);
	ConfigSettings->SaveConfig();
	OutName = MoveTemp(NewPackageName);
	return true;
}

void SMoviePipelineQueuePanel::OnSaveAsset()
{
	UMoviePipelineQueue* QueueOrigin = GetQueueOrigin();
	
	// If the queue origin is not known, then treat the "Save" as a "Save As"
	if (!QueueOrigin)
	{
		OnSaveAsAsset();
		return;
	}
	
	SaveTransientQueueToAsset(QueueOrigin);
}

void SMoviePipelineQueuePanel::OnSaveAsAsset()
{
	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);
	UMoviePipelineQueue* CurrentQueue = Subsystem->GetQueue();

	FString PackageName;
	if (!GetSavePresetPackageName(CurrentQueue->GetName(), PackageName))
	{
		return;
	}
	
	// Saving into a new package
	const FString NewAssetName = FPackageName::GetLongPackageAssetName(PackageName);
	UPackage* NewPackage = CreatePackage(*PackageName);
	NewPackage->MarkAsFullyLoaded();
	UMoviePipelineQueue* DuplicateQueue = DuplicateObject<UMoviePipelineQueue>(CurrentQueue, NewPackage, *NewAssetName);

	SaveTransientQueueToAsset(DuplicateQueue);
}

void SMoviePipelineQueuePanel::SaveTransientQueueToAsset(UMoviePipelineQueue* DestinationQueue) const
{
	if (!DestinationQueue)
	{
		return;
	}

	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);
	UMoviePipelineQueue* CurrentQueue = Subsystem->GetQueue();

	DestinationQueue->CopyFrom(CurrentQueue);
	DestinationQueue->SetQueueOrigin(nullptr);
	DestinationQueue->MarkPackageDirty();
	DestinationQueue->SetFlags(RF_Public | RF_Standalone | RF_Transactional);

	FAssetRegistryModule::AssetCreated(DestinationQueue);

	const bool bCheckDirty = false;
	const bool bPromptToSave = false;
	FEditorFileUtils::EPromptReturnCode PromptReturnCode = FEditorFileUtils::PromptForCheckoutAndSave({ DestinationQueue->GetPackage() }, bCheckDirty, bPromptToSave);
	if (PromptReturnCode == FEditorFileUtils::EPromptReturnCode::PR_Success)
	{
		LoadQueue(DestinationQueue);
	}
}

void SMoviePipelineQueuePanel::OnImportSavedQueueAsset(const FAssetData& InPresetAsset) const
{
	FSlateApplication::Get().DismissAllMenus();

	if (IsQueueDirty())
	{
		const FText TitleText = LOCTEXT("UnsavedQueueWarningTitle", "Unsaved Changes to Queue");
		const FText MessageText = LOCTEXT("UnsavedQueueWarningMessage", "The changes made to the current queue will be lost by importing another queue. Do you want to continue with this import?");

		if (FMessageDialog::Open(EAppMsgType::YesNo, MessageText, &TitleText) == EAppReturnType::No)
		{
			return;
		}
	}

	UMoviePipelineQueue* SavedQueue = CastChecked<UMoviePipelineQueue>(InPresetAsset.GetAsset());
	LoadQueue(SavedQueue);
}

void SMoviePipelineQueuePanel::LoadQueue(UMoviePipelineQueue* SavedQueue) const
{
	if (SavedQueue)
	{
		UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
		check(Subsystem);

		UMoviePipelineQueue* CurrentQueue = Subsystem->GetQueue();
		CurrentQueue->CopyFrom(SavedQueue);
		CurrentQueue->SetQueueOrigin(SavedQueue);
		CurrentQueue->SetIsDirty(false);

		// Update the shot list in case the stored queue being copied is out of date with the sequence
		for (UMoviePipelineExecutorJob* Job : Subsystem->GetQueue()->GetJobs())
		{
			ULevelSequence* LoadedSequence = Cast<ULevelSequence>(Job->Sequence.TryLoad());
			if (LoadedSequence)
			{
				bool bShotsChanged = false;
				UMoviePipelineBlueprintLibrary::UpdateJobShotListFromSequence(LoadedSequence, Job, bShotsChanged);

				if (bShotsChanged)
				{
					FNotificationInfo Info(LOCTEXT("QueueShotsUpdated", "Shots have changed since the queue was saved, please resave the queue"));
					Info.ExpireDuration = 5.0f;
					FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
				}
			}
		}

		// Automatically select the first job in the queue
		TArray<UMoviePipelineExecutorJob*> Jobs;
		if (Subsystem->GetQueue()->GetJobs().Num() > 0)
		{
			Jobs.Add(Subsystem->GetQueue()->GetJobs()[0]);
		}

		// Go through the UI so it updates the UI selection too and then this will loop back
		// around to OnSelectionChanged to update ourself.
		PipelineQueueEditorWidget->SetSelectedJobs(Jobs);
	}
}

bool SMoviePipelineQueuePanel::IsQueueDirty() const
{
	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);

	const UMoviePipelineQueue* Queue = Subsystem->GetQueue();

	// The queue is considered dirty if the current queue has no origin (ie, it has never been saved) or it has been
	// modified since it was loaded
	return !GetQueueOrigin() || (Queue && Queue->IsDirty());
}

UMoviePipelineQueue* SMoviePipelineQueuePanel::GetQueueOrigin() const
{
	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);

	const UMoviePipelineQueue* Queue = Subsystem->GetQueue();

	return Queue ? Queue->GetQueueOrigin() : nullptr;
}

FString SMoviePipelineQueuePanel::GetQueueOriginName() const
{
	const UMoviePipelineQueue* QueueOrigin = GetQueueOrigin();
	if (!QueueOrigin)
	{
		return FString();
	}

	if (const UPackage* Package = QueueOrigin->GetPackage())
	{
		FString OutPackageRoot, OutPackagePath, OutPackageName;
		if (FPackageName::SplitLongPackageName(Package->GetName(), OutPackageRoot, OutPackagePath, OutPackageName))
		{
			return OutPackageName;
		}
	}

	return FString();
}

#undef LOCTEXT_NAMESPACE // SMoviePipelineQueuePanel