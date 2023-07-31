// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaPlateCustomization.h"

#include "CineCameraSettings.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailGroup.h"
#include "LevelEditorViewport.h"
#include "MediaPlate.h"
#include "MediaPlateComponent.h"
#include "MediaPlateEditorModule.h"
#include "MediaPlayer.h"
#include "MediaPlaylist.h"
#include "MediaSource.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SFilePathPicker.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SMediaPlateEditorMediaDetails.h"

#define LOCTEXT_NAMESPACE "FMediaPlateCustomization"

/* IDetailCustomization interface
 *****************************************************************************/


FMediaPlateCustomization::FMediaPlateCustomization()
	: bIsMediaSourceAsset(false)
{
}

FMediaPlateCustomization::~FMediaPlateCustomization()
{
}

void FMediaPlateCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Is this the media plate editor window?
	bool bIsMediaPlateWindow = false;

	if (const IDetailsView* DetailsView = DetailBuilder.GetDetailsView())
	{
		TSharedPtr<FTabManager> HostTabManager = DetailsView->GetHostTabManager();
		bIsMediaPlateWindow = (HostTabManager.IsValid() == false);
	}

	// Get style.
	const ISlateStyle* Style = nullptr;
	FMediaPlateEditorModule* EditorModule = FModuleManager::LoadModulePtr<FMediaPlateEditorModule>("MediaPlateEditor");
	if (EditorModule != nullptr)
	{
		Style = EditorModule->GetStyle().Get();
	}
	if (Style == nullptr)
	{
		Style = &FAppStyle::Get();
	}

	// Hide the static mesh.
	IDetailCategoryBuilder& StaticMeshCategory = DetailBuilder.EditCategory("StaticMesh");
	StaticMeshCategory.SetCategoryVisibility(false);

	IDetailCategoryBuilder& MediaPlateCategory = DetailBuilder.EditCategory("MediaPlate");
	// Place this at the top.
	MediaPlateCategory.SetSortOrder(0);

	// Get objects we are editing.
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	MediaPlatesList.Reserve(Objects.Num());
	for (TWeakObjectPtr<UObject>& Obj : Objects)
	{
		TWeakObjectPtr<UMediaPlateComponent> MediaPlate = Cast<UMediaPlateComponent>(Obj.Get());
		if (MediaPlate.IsValid())
		{
			MediaPlatesList.Add(MediaPlate);
			MeshMode = MediaPlate->GetVisibleMipsTilesCalculations();
		}
	}

	// Set up media source.
	UpdateIsMediaSourceAsset();
	UpdateMediaPath();

	// Add mesh customization.
	AddMeshCustomization(MediaPlateCategory);
	
	// Create playlist group.
	IDetailGroup& PlaylistGroup = MediaPlateCategory.AddGroup(TEXT("Playlist"),
		LOCTEXT("Playlist", "Playlist"));

	// Add playlist.
	PlaylistGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("PlaylistAsset", "Playlist Asset"))
				.ToolTipText(LOCTEXT("PlaylistAsset_Tooltip",
					"The playlist asset to use. If none then an internal asset will be used."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SObjectPropertyEntryBox)
				.AllowedClass(UMediaPlaylist::StaticClass())
				.ObjectPath(this, &FMediaPlateCustomization::GetPlaylistPath)
				.OnObjectChanged(this, &FMediaPlateCustomization::OnPlaylistChanged)
		];

	// Add media source type.
	TAttribute<EVisibility> MediaSourceAssetVisibility(this, &FMediaPlateCustomization::ShouldShowMediaSourceAsset);
	TAttribute<EVisibility> MediaSourceFileVisibility(this, &FMediaPlateCustomization::ShouldShowMediaSourceFile);
	PlaylistGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("FirstItemInPlaylist", "First Item In Playlist"))
				.ToolTipText(LOCTEXT("FirstItemInPlaylist_ToolTip",
					"The type of the first item in the playlist.\n"
					"File lets you select a media file like a .mp4.\n"
					"Asset lets you select a Media Source asset."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SSegmentedControl<bool>)
				.Value_Lambda([this]()
				{
					return bIsMediaSourceAsset;
				})
				.OnValueChanged(this, &FMediaPlateCustomization::SetIsMediaSourceAsset)

			+ SSegmentedControl<bool>::Slot(false)
				.Text(LOCTEXT("File", "File"))
				.ToolTip(LOCTEXT("File_ToolTip",
					"Select this if you want to use a file like a .mp4."))

			+ SSegmentedControl<bool>::Slot(true)
				.Text(LOCTEXT("Asset", "Asset"))
				.ToolTip(LOCTEXT("Asset_ToolTip",
					"Select this if you want to use a Media Source asset."))
		];

	// Add media asset.
	PlaylistGroup.AddWidgetRow()
		.Visibility(MediaSourceAssetVisibility)
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("MediaAsset", "Media Source Asset"))
				.ToolTipText(LOCTEXT("MediaAsset_ToolTip", "The Media Source to play."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SObjectPropertyEntryBox)
				.AllowedClass(UMediaSource::StaticClass())
				.ObjectPath(this, &FMediaPlateCustomization::GetMediaSourcePath)
				.OnObjectChanged(this, &FMediaPlateCustomization::OnMediaSourceChanged)
		];

	// Add media path.
	FString FileTypeFilter = TEXT("All files (*.*)|*.*");
	PlaylistGroup.AddWidgetRow()
		.Visibility(MediaSourceFileVisibility)
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("MediaPath", "Media Path"))
				.ToolTipText(LOCTEXT("MediaPath_ToolTip", "The path to a media file like a .mp4."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SFilePathPicker)
				.BrowseButtonImage(FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
				.BrowseButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.BrowseButtonToolTip(LOCTEXT("FileButtonToolTipText", "Choose a file from this computer"))
				.BrowseTitle(LOCTEXT("PropertyEditorTitle", "File picker..."))
				.FilePath(this, &FMediaPlateCustomization::HandleMediaPath)
				.FileTypeFilter(FileTypeFilter)
				.OnPathPicked(this, &FMediaPlateCustomization::HandleMediaPathPicked)
		];

	// Add media control buttons.
	MediaPlateCategory.AddCustomRow(LOCTEXT("MediaPlateControls", "MediaPlate Controls"))
		[
			SNew(SHorizontalBox)

			// Rewind button.
			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
						.VAlign(VAlign_Center)
						.IsEnabled_Lambda([this]
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
									if (MediaPlayer != nullptr)
									{
										return MediaPlayer->IsReady() &&
											MediaPlayer->SupportsSeeking() &&
											MediaPlayer->GetTime() > FTimespan::Zero();
									}
								}
							}
							return false;
						})
						.OnClicked_Lambda([this]() -> FReply
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
									if (MediaPlayer != nullptr)
									{
										MediaPlayer->Rewind();
									}
								}
							}
							return FReply::Handled();
						})
						[
							SNew(SImage)
								.ColorAndOpacity(FSlateColor::UseForeground())
								.Image(Style->GetBrush("MediaPlateEditor.RewindMedia.Small"))
								.ToolTipText(LOCTEXT("Rewind", "Rewind the media to the beginning"))
						]
				]

			// Reverse button.
			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
						.VAlign(VAlign_Center)
						.IsEnabled_Lambda([this] {
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
									if (MediaPlayer != nullptr)
									{
										return MediaPlayer->IsReady() && MediaPlayer->SupportsRate(GetReverseRate(MediaPlayer), false);
									}
								}
							}
							return false;
						})
						.OnClicked_Lambda([this]() -> FReply
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
									if (MediaPlayer != nullptr)
									{
										MediaPlayer->SetRate(GetReverseRate(MediaPlayer));
									}
								}
							}
							return FReply::Handled();
						})
						[
							SNew(SImage)
								.ColorAndOpacity(FSlateColor::UseForeground())
								.Image(Style->GetBrush("MediaPlateEditor.ReverseMedia.Small"))
								.ToolTipText(LOCTEXT("Reverse", "Reverse media playback"))
						]
				]

			// Play button.
			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
						.VAlign(VAlign_Center)
						.IsEnabled_Lambda([this]
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									// Is the player paused or fast forwarding/rewinding?
									UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
									return ((MediaPlayer != nullptr) &&
										(MediaPlayer->IsReady()) &&
										((!MediaPlayer->IsPlaying() || (MediaPlayer->GetRate() != 1.0f))));
								}
							}
							return false;
						})
						.OnClicked_Lambda([this]() -> FReply
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									MediaPlate->Play();
								}
							}
							return FReply::Handled();
						})
						[
							SNew(SImage)
								.ColorAndOpacity(FSlateColor::UseForeground())
								.Image(Style->GetBrush("MediaPlateEditor.PlayMedia.Small"))
								.ToolTipText(LOCTEXT("Play", "Start media playback"))
						]
				]

			// Pause button.
			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
						.VAlign(VAlign_Center)
						.IsEnabled_Lambda([this]
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
									if (MediaPlayer != nullptr)
									{
										return (MediaPlayer->CanPause()) && (!MediaPlayer->IsPaused());
									}
								}
							}
							return false;
						})
						.OnClicked_Lambda([this]() -> FReply
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{							
									MediaPlate->Pause();
								}
							}
							return FReply::Handled();
						})
						[
							SNew(SImage)
								.ColorAndOpacity(FSlateColor::UseForeground())
								.Image(Style->GetBrush("MediaPlateEditor.PauseMedia.Small"))
								.ToolTipText(LOCTEXT("Pause", "Pause media playback"))
						]
				]

			// Forward button.
			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.IsEnabled_Lambda([this] {
						for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
						{
							UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
							if (MediaPlate != nullptr)
							{
								UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
								if (MediaPlayer != nullptr)
								{
									return MediaPlayer->IsReady() && MediaPlayer->SupportsRate(GetForwardRate(MediaPlayer), false);
								}
							}
						}
						return false;
					})
					.OnClicked_Lambda([this]() -> FReply
					{
						for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
						{
							UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
							if (MediaPlate != nullptr)
							{
								UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
								if (MediaPlayer != nullptr)
								{
									MediaPlayer->SetRate(GetForwardRate(MediaPlayer));
								}
							}
						}
						return FReply::Handled();
					})
					[
						SNew(SImage)
							.ColorAndOpacity(FSlateColor::UseForeground())
							.Image(Style->GetBrush("MediaPlateEditor.ForwardMedia.Small"))
							.ToolTipText(LOCTEXT("Forward", "Fast forward media playback"))
					]
				]

			// Open button.
			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
						.VAlign(VAlign_Center)
						.OnClicked_Lambda([this]() -> FReply
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									// Tell the editor module that this media plate is playing.
									FMediaPlateEditorModule* EditorModule = FModuleManager::LoadModulePtr<FMediaPlateEditorModule>("MediaPlateEditor");
									if (EditorModule != nullptr)
									{
										EditorModule->MediaPlateStartedPlayback(MediaPlate);
									}

									// Play the media.
									MediaPlate->Open();
								}
							}
							return FReply::Handled();
						})
						[
							SNew(SImage)
								.ColorAndOpacity(FSlateColor::UseForeground())
								.Image(Style->GetBrush("MediaPlateEditor.OpenMedia.Small"))
								.ToolTipText(LOCTEXT("Open", "Open the current media"))
						]
				]

			// Close button.
			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
						.VAlign(VAlign_Center)
						.IsEnabled_Lambda([this]
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
									if (MediaPlayer != nullptr)
									{
										return !MediaPlayer->GetUrl().IsEmpty();
									}
								}
							}
							return false;
						})
						.OnClicked_Lambda([this]() -> FReply
						{
							for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
							{
								UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
								if (MediaPlate != nullptr)
								{
									MediaPlate->Close();
								}
							}
							return FReply::Handled();
						})
						[
							SNew(SImage)
								.ColorAndOpacity(FSlateColor::UseForeground())
								.Image(Style->GetBrush("MediaPlateEditor.CloseMedia.Small"))
								.ToolTipText(LOCTEXT("Close", "Close the currently opened media"))
						]
				]
		];


	// Add button to open the media plate editor.
	if (bIsMediaPlateWindow == false)
	{
		MediaPlateCategory.AddCustomRow(LOCTEXT("OpenMediaPlate", "Open Media Plate"))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.Padding(0, 5, 10, 5)
					[
						SNew(SButton)
							.ContentPadding(3)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.OnClicked(this, &FMediaPlateCustomization::OnOpenMediaPlate)
							.Text(LOCTEXT("OpenMediaPlate", "Open Media Plate"))
					]
			];

		// Get the first media plate.
		UMediaPlateComponent* FirstMediaPlate = nullptr;
		for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
		{
			UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
			if (MediaPlate != nullptr)
			{
				FirstMediaPlate = MediaPlate;
				break;
			}
		}

		if (FirstMediaPlate != nullptr)
		{
			// Create media details group.
			IDetailGroup& MediaDetailsGroup = MediaPlateCategory.AddGroup(TEXT("MediaDetails"),
				LOCTEXT("MediaDetails", "Media Details"));

			MediaDetailsGroup.AddWidgetRow()
			[
				SNew(SMediaPlateEditorMediaDetails, *FirstMediaPlate)
			];
		}
	}
}

void FMediaPlateCustomization::AddMeshCustomization(IDetailCategoryBuilder& MediaPlateCategory)
{
	// Create detail group.
	IDetailGroup& DetailGroup = MediaPlateCategory.AddGroup(TEXT("Mesh"),
		LOCTEXT("Mesh", "Mesh"));

	// Add radio buttons for mesh type.
	DetailGroup.AddWidgetRow()
		[
			SNew(SSegmentedControl<EMediaTextureVisibleMipsTiles>)
				.Value_Lambda([this]()
				{
					return MeshMode;
				})
				.OnValueChanged(this, &FMediaPlateCustomization::SetMeshMode)

			+ SSegmentedControl<EMediaTextureVisibleMipsTiles>::Slot(EMediaTextureVisibleMipsTiles::Plane)
				.Text(LOCTEXT("Plane", "Plane"))
				.ToolTip(LOCTEXT("Plane_ToolTip",
					"Select this if you want to use a standard plane for the mesh."))

			+ SSegmentedControl<EMediaTextureVisibleMipsTiles>::Slot(EMediaTextureVisibleMipsTiles::Sphere)
				.Text(LOCTEXT("Sphere", "Sphere"))
				.ToolTip(LOCTEXT("Sphere_ToolTip",
					"Select this if you want to use a spherical object for the mesh."))

			+ SSegmentedControl<EMediaTextureVisibleMipsTiles>::Slot(EMediaTextureVisibleMipsTiles::None)
				.Text(LOCTEXT("Custom", "Custom"))
				.ToolTip(LOCTEXT("Custom_ToolTip",
					"Select this if you want to provide your own mesh."))
		];

	// Visibility attributes.
	TAttribute<EVisibility> MeshCustomVisibility(this, &FMediaPlateCustomization::ShouldShowMeshCustomWidgets);
	TAttribute<EVisibility> MeshPlaneVisibility(this, &FMediaPlateCustomization::ShouldShowMeshPlaneWidgets);
	TAttribute<EVisibility> MeshSphereVisibility(this, &FMediaPlateCustomization::ShouldShowMeshSphereWidgets);

	// Add aspect ratio.
	DetailGroup.AddWidgetRow()
		.Visibility(MeshPlaneVisibility)
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("AspectRatio", "Aspect Ratio"))
				.ToolTipText(LOCTEXT("AspectRatio_ToolTip",
				"Sets the aspect ratio of the plane showing the media.\nChanging this will change the scale of the mesh component."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)

			// Presets button.
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SComboButton)
						.OnGetMenuContent(this, &FMediaPlateCustomization::OnGetAspectRatios)
						.ContentPadding(2)
						.ButtonContent()
						[
							SNew(STextBlock)
								.ToolTipText(LOCTEXT("Presets_ToolTip", "Select one of the presets for the aspect ratio."))
								.Text(LOCTEXT("Presets", "Presets"))
						]
				]

			// Numeric entry box.
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SNumericEntryBox<float>)
						.Value(this, &FMediaPlateCustomization::GetAspectRatio)
						.MinValue(0.0f)
						.OnValueChanged(this, &FMediaPlateCustomization::SetAspectRatio)
				]
		];

		// Add letterbox aspect ratio.
		DetailGroup.AddWidgetRow()
			.Visibility(MeshPlaneVisibility)
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("LetterboxAspectRatio", "Letterbox Aspect Ratio"))
					.ToolTipText(LOCTEXT("LetterboxAspectRatio_ToolTip",
						"Sets the aspect ratio of the whole screen.\n"
						"If the screen is larger than the media then letterboxes will be added."))
					.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)

				// Presets button.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SComboButton)
						.OnGetMenuContent(this, &FMediaPlateCustomization::OnGetLetterboxAspectRatios)
						.ContentPadding(2)
						.ButtonContent()
						[
							SNew(STextBlock)
								.ToolTipText(LOCTEXT("Presets_ToolTip", "Select one of the presets for the aspect ratio."))
								.Text(LOCTEXT("Presets", "Presets"))
						]
			]

		// Numeric entry box.
		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SNumericEntryBox<float>)
					.Value(this, &FMediaPlateCustomization::GetLetterboxAspectRatio)
					.MinValue(0.0f)
					.OnValueChanged(this, &FMediaPlateCustomization::SetLetterboxAspectRatio)
			]
		];

	// Add auto aspect ratio.
	DetailGroup.AddWidgetRow()
		.Visibility(MeshPlaneVisibility)
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("AutoAspectRatio", "Auto Aspect Ratio"))
				.ToolTipText(LOCTEXT("AutoAspectRatio_ToolTip",
					"Sets the aspect ratio to match the media."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SCheckBox)
				.IsChecked(this, &FMediaPlateCustomization::IsAspectRatioAuto)
				.OnCheckStateChanged(this, &FMediaPlateCustomization::SetIsAspectRatioAuto)
		];

	// Add sphere horizontal arc.
	DetailGroup.AddWidgetRow()
		.Visibility(MeshSphereVisibility)
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("HorizontalArc", "Horizontal Arc"))
				.ToolTipText(LOCTEXT("HorizontalArc_ToolTip",
				"Sets the horizontal arc size of the sphere in degrees.\nFor example 360 for a full circle, 180 for a half circle."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SNumericEntryBox<float>)
				.Value(this, &FMediaPlateCustomization::GetMeshHorizontalRange)
				.OnValueChanged(this, &FMediaPlateCustomization::SetMeshHorizontalRange)
		];

	// Add sphere vertical arc.
	DetailGroup.AddWidgetRow()
		.Visibility(MeshSphereVisibility)
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("VerticalArc", "Vertical Arc"))
				.ToolTipText(LOCTEXT("VerticalArc_ToolTip",
				"Sets the vertical arc size of the sphere in degrees.\nFor example 180 for a half circle, 90 for a quarter circle."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SNumericEntryBox<float>)
				.Value(this, &FMediaPlateCustomization::GetMeshVerticalRange)
				.OnValueChanged(this, &FMediaPlateCustomization::SetMeshVerticalRange)
		];

	// Add static mesh.
	DetailGroup.AddWidgetRow()
		.Visibility(MeshCustomVisibility)
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("StaticMesh", "Static Mesh"))
				.ToolTipText(LOCTEXT("StaticMesh_Tooltip", "The static mesh to use."))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SObjectPropertyEntryBox)
				.AllowedClass(UStaticMesh::StaticClass())
				.ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool())
				.ObjectPath(this, &FMediaPlateCustomization::GetStaticMeshPath)
				.OnObjectChanged(this, &FMediaPlateCustomization::OnStaticMeshChanged)
		];
	
}

EVisibility FMediaPlateCustomization::ShouldShowMeshCustomWidgets() const
{
	return (MeshMode == EMediaTextureVisibleMipsTiles::None) ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility FMediaPlateCustomization::ShouldShowMeshPlaneWidgets() const
{
	return (MeshMode == EMediaTextureVisibleMipsTiles::Plane) ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility FMediaPlateCustomization::ShouldShowMeshSphereWidgets() const
{
	return (MeshMode == EMediaTextureVisibleMipsTiles::Sphere) ? EVisibility::Visible : EVisibility::Hidden;
}

void FMediaPlateCustomization::SetMeshMode(EMediaTextureVisibleMipsTiles InMode)
{
	if (MeshMode != InMode)
	{
		MeshMode = InMode;
		for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
		{
			UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
			if (MediaPlate != nullptr)
			{
				// Update the setting in the media plate.
				MediaPlate->SetVisibleMipsTilesCalculations(MeshMode);

				// Set the appropriate mesh.
				if (MeshMode == EMediaTextureVisibleMipsTiles::Plane)
				{
					MeshCustomization.SetPlaneMesh(MediaPlate);
				}
				else
				{
					// Letterboxes are only for planes.
					SetLetterboxAspectRatio(0.0f);

					if (MeshMode == EMediaTextureVisibleMipsTiles::Sphere)
					{
						SetSphereMesh(MediaPlate);
					}
				}
			}
		}
	}
}

void FMediaPlateCustomization::SetSphereMesh(UMediaPlateComponent* MediaPlate)
{
	MeshCustomization.SetSphereMesh(MediaPlate);
}

ECheckBoxState FMediaPlateCustomization::IsAspectRatioAuto() const
{
	ECheckBoxState State = ECheckBoxState::Undetermined;

	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			ECheckBoxState NewState = MediaPlate->bIsAspectRatioAuto ? ECheckBoxState::Checked :
				ECheckBoxState::Unchecked;
			if (State == ECheckBoxState::Undetermined)
			{
				State = NewState;
			}
			else if (State != NewState)
			{
				// If the media plates have different states then return undetermined.
				State = ECheckBoxState::Undetermined;
				break;
			}
		}
	}

	return State;
}

void FMediaPlateCustomization::SetIsAspectRatioAuto(ECheckBoxState State)
{
	bool bEnable = (State == ECheckBoxState::Checked);

	// Loop through all our objects.
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			MediaPlate->bIsAspectRatioAuto = bEnable;
		}
	}
}

TSharedRef<SWidget> FMediaPlateCustomization::OnGetAspectRatios()
{
	FMenuBuilder MenuBuilder(true, NULL);
	AddAspectRatiosToMenuBuilder(MenuBuilder, &FMediaPlateCustomization::SetAspectRatio);

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FMediaPlateCustomization::OnGetLetterboxAspectRatios()
{
	FMenuBuilder MenuBuilder(true, NULL);
	AddAspectRatiosToMenuBuilder(MenuBuilder, &FMediaPlateCustomization::SetLetterboxAspectRatio);
	
	FUIAction Action = FUIAction(FExecuteAction::CreateSP(this, &FMediaPlateCustomization::SetLetterboxAspectRatio, 0.0f));
	MenuBuilder.AddMenuEntry(LOCTEXT("Disable", "Disable"), FText(), FSlateIcon(), Action);

	return MenuBuilder.MakeWidget();
}

void FMediaPlateCustomization::AddAspectRatiosToMenuBuilder(FMenuBuilder& MenuBuilder,
	void (FMediaPlateCustomization::* Func)(float))
{
	TArray<FNamedFilmbackPreset> const& Presets = UCineCameraSettings::GetFilmbackPresets();

	for (const FNamedFilmbackPreset& Preset : Presets)
	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(this,
			Func, Preset.FilmbackSettings.SensorAspectRatio));
		MenuBuilder.AddMenuEntry(FText::FromString(Preset.Name), FText(), FSlateIcon(), Action);
	}
}

void FMediaPlateCustomization::SetAspectRatio(float AspectRatio)
{
	// Loop through all our objects.
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			MediaPlate->SetAspectRatio(AspectRatio);
		}
	}

	// Invalidate the viewport so we can see the mesh change.
	if (GCurrentLevelEditingViewportClient != nullptr)
	{
		GCurrentLevelEditingViewportClient->Invalidate();
	}
}


TOptional<float> FMediaPlateCustomization::GetAspectRatio() const
{
	// Loop through our objects.
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			return MediaPlate->GetAspectRatio();
		}
	}

	return TOptional<float>();
}

void FMediaPlateCustomization::SetLetterboxAspectRatio(float AspectRatio)
{
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			MediaPlate->SetLetterboxAspectRatio(AspectRatio);
		}
	}

	// Invalidate the viewport so we can see the mesh change.
	if (GCurrentLevelEditingViewportClient != nullptr)
	{
		GCurrentLevelEditingViewportClient->Invalidate();
	}
}


TOptional<float> FMediaPlateCustomization::GetLetterboxAspectRatio() const
{
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			return MediaPlate->GetLetterboxAspectRatio();
		}
	}

	return TOptional<float>();
}

void FMediaPlateCustomization::SetMeshHorizontalRange(float HorizontalRange)
{
	HorizontalRange = FMath::Clamp(HorizontalRange, 0.0f, 360.0f);
	TOptional VerticalRange = GetMeshVerticalRange();
	if (VerticalRange.IsSet())
	{
		FVector2D MeshRange = FVector2D(HorizontalRange, VerticalRange.GetValue());
		SetMeshRange(MeshRange);
	}
}

TOptional<float> FMediaPlateCustomization::GetMeshHorizontalRange() const
{
	// Loop through our objects.
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			return MediaPlate->GetMeshRange().X;
		}
	}

	return TOptional<float>();
}

void FMediaPlateCustomization::SetMeshVerticalRange(float VerticalRange)
{
	VerticalRange = FMath::Clamp(VerticalRange, 0.0f, 180.0f);
	TOptional HorizontalRange = GetMeshHorizontalRange();
	if (HorizontalRange.IsSet())
	{
		FVector2D MeshRange = FVector2D(HorizontalRange.GetValue(), VerticalRange);
		SetMeshRange(MeshRange);
	}
}

TOptional<float> FMediaPlateCustomization::GetMeshVerticalRange() const
{
	// Loop through our objects.
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			return MediaPlate->GetMeshRange().Y;
		}
	}

	return TOptional<float>();
}

void FMediaPlateCustomization::SetMeshRange(FVector2D Range)
{
	// Loop through all our objects.
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			if (MediaPlate->GetMeshRange() != Range)
			{
				MediaPlate->SetMeshRange(Range);
				SetSphereMesh(MediaPlate);
			}
		}
	}
}


FString FMediaPlateCustomization::GetStaticMeshPath() const
{
	FString Path;

	// Get the first media plate.
	if (MediaPlatesList.Num() > 0)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatesList[0].Get();
		if (MediaPlate != nullptr)
		{
			UStaticMeshComponent* StaticMeshComponent = MediaPlate->StaticMeshComponent;
			if (StaticMeshComponent != nullptr)
			{
				UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
				if (StaticMesh != nullptr)
				{
					Path = StaticMesh->GetPathName();
				}
			}
		}
	}

	return Path;
}

void FMediaPlateCustomization::OnStaticMeshChanged(const FAssetData& AssetData)
{
	// Update the static mesh.
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset());
	for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			MeshCustomization.SetCustomMesh(MediaPlate, StaticMesh);
		}
	}
}

void FMediaPlateCustomization::UpdateIsMediaSourceAsset()
{
	bIsMediaSourceAsset = false;

	// Get the first media plate.
	if (MediaPlatesList.Num() > 0)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatesList[0].Get();
		if (MediaPlate != nullptr)
		{
			UMediaPlaylist* Playlist = MediaPlate->MediaPlaylist;
			if (Playlist != nullptr)
			{
				// Get the first media source in the playlist.
				UMediaSource* MediaSource = Playlist->Get(0);
				if (MediaSource != nullptr)
				{
					bIsMediaSourceAsset = MediaSource->GetOuter() != MediaPlate;
				}
			}
		}
	}
}

void FMediaPlateCustomization::SetIsMediaSourceAsset(bool bIsAsset)
{
	if (bIsMediaSourceAsset != bIsAsset)
	{
		bIsMediaSourceAsset = bIsAsset;

		// Clear out asset.
		for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
		{
			UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
			if (MediaPlate != nullptr)
			{
				// Get playlist.
				UMediaPlaylist* Playlist = MediaPlate->MediaPlaylist;
				if (Playlist != nullptr)
				{
					// Update playlist.
					if (Playlist->Num() > 0)
					{
						Playlist->Replace(0, nullptr);
						Playlist->MarkPackageDirty();
					}
				}
				break;
			}
		}

		StopMediaPlates();
		UpdateMediaPath();
	}
}

EVisibility FMediaPlateCustomization::ShouldShowMediaSourceAsset() const
{
	return bIsMediaSourceAsset ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility FMediaPlateCustomization::ShouldShowMediaSourceFile() const
{
	return bIsMediaSourceAsset ? EVisibility::Hidden : EVisibility::Visible;
}

FString FMediaPlateCustomization::GetMediaSourcePath() const
{
	FString Path;

	// Get the first media plate.
	if (MediaPlatesList.Num() > 0)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatesList[0].Get();
		if (MediaPlate != nullptr)
		{
			UMediaPlaylist* Playlist = MediaPlate->MediaPlaylist;
			if (Playlist != nullptr)
			{
				// Get the first media source in the playlist.
				UMediaSource* MediaSource = Playlist->Get(0);
				if (MediaSource != nullptr)
				{
					Path = MediaSource->GetPathName();
				}
			}
		}
	}

	return Path;
}

FString FMediaPlateCustomization::GetPlaylistPath() const
{
	FString Path;

	// Get the first media plate.
	if (MediaPlatesList.Num() > 0)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatesList[0].Get();
		if (MediaPlate != nullptr)
		{
			UMediaPlaylist* Playlist = MediaPlate->MediaPlaylist;
			if (Playlist != nullptr)
			{
				if (Playlist->GetOuter() != MediaPlate)
				{
					Path = Playlist->GetPathName();
				}
			}
		}
	}

	return Path;
}

void FMediaPlateCustomization::OnPlaylistChanged(const FAssetData& AssetData)
{
	StopMediaPlates();

	// Update the playlist.
	UMediaPlaylist* Playlist = Cast<UMediaPlaylist>(AssetData.GetAsset());
	for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			MediaPlate->MediaPlaylist = Playlist;
		}
	}

	UpdateIsMediaSourceAsset();
	UpdateMediaPath();
}

void FMediaPlateCustomization::OnMediaSourceChanged(const FAssetData& AssetData)
{
	// Update the playlist with the new media source.
	UMediaSource* MediaSource = Cast<UMediaSource>(AssetData.GetAsset());
	for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			// Get playlist.
			UMediaPlaylist* Playlist = MediaPlate->MediaPlaylist;
			if (Playlist == nullptr)
			{
				Playlist = NewObject<UMediaPlaylist>(MediaPlate);
				MediaPlate->MediaPlaylist = Playlist;
			}
			
			// Update playlist.
			if (Playlist->Num() > 0)
			{
				Playlist->Replace(0, MediaSource);
			}
			else
			{
				Playlist->Add(MediaSource);
			}
			Playlist->MarkPackageDirty();
		}
	}

	StopMediaPlates();
	UpdateMediaPath();
}

void FMediaPlateCustomization::UpdateMediaPath()
{
	MediaPath.Empty();

	// Get the first media plate.
	if (MediaPlatesList.Num() > 0)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatesList[0].Get();
		if (MediaPlate != nullptr)
		{
			UMediaPlaylist* Playlist = MediaPlate->MediaPlaylist;
			if (Playlist != nullptr)
			{
				// Get the first media source in the playlist.
				UMediaSource* MediaSource = Playlist->Get(0);
				if (MediaSource != nullptr)
				{
					MediaPath = MediaSource->GetUrl();

					// Remove certain types.
					const FString FilePrefix(TEXT("file://"));
					const FString ImgPrefix(TEXT("img://"));
					if (MediaPath.StartsWith(FilePrefix))
					{
						MediaPath = MediaPath.RightChop(FilePrefix.Len());
					}
					else if (MediaPath.StartsWith(ImgPrefix))
					{
						MediaPath = MediaPath.RightChop(ImgPrefix.Len());
					}
				}
			}
		}
	}
}

FString FMediaPlateCustomization::HandleMediaPath() const
{
	return MediaPath;
}

void FMediaPlateCustomization::HandleMediaPathPicked(const FString& PickedPath)
{
	// Did we get something?
	if ((PickedPath.IsEmpty() == false) && (PickedPath != MediaPath))
	{
		// Stop playback.
		StopMediaPlates();

		// Set up media source for our media plates.
		for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
		{
			UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
			if (MediaPlate != nullptr)
			{
				// Get playlist.
				UMediaPlaylist* Playlist = MediaPlate->MediaPlaylist;
				if (Playlist == nullptr)
				{
					Playlist = NewObject<UMediaPlaylist>(MediaPlate);
					MediaPlate->MediaPlaylist = Playlist;
				}
				
				// Create media source for this path.
				UMediaSource* MediaSource = UMediaSource::SpawnMediaSourceForString(PickedPath, MediaPlate);
				if (MediaSource != nullptr)
				{
					if (Playlist->Num() > 0)
					{
						Playlist->Replace(0, MediaSource);
					}
					else
					{
						Playlist->Add(MediaSource);
					}
					Playlist->MarkPackageDirty();
				}
			}
		}

		// Update the media path.
		UpdateMediaPath();
	}
}

FReply FMediaPlateCustomization::OnOpenMediaPlate()
{
	// Get all our objects.
	TArray<UObject*> AssetArray;
	for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			AssetArray.Add(MediaPlate);
		}
	}

	// Open the editor.
	if (AssetArray.Num() > 0)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(AssetArray);
	}

	return FReply::Handled();
}

void FMediaPlateCustomization::StopMediaPlates()
{
	for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			MediaPlate->Close();
		}
	}
}

float FMediaPlateCustomization::GetForwardRate(UMediaPlayer* MediaPlayer) const
{
	float Rate = MediaPlayer->GetRate();

	if (Rate < 1.0f)
	{
		Rate = 1.0f;
	}

	return 2.0f * Rate;
}

float FMediaPlateCustomization::GetReverseRate(UMediaPlayer* MediaPlayer) const
{
	float Rate = MediaPlayer->GetRate();

	if (Rate > -1.0f)
	{
		return -1.0f;
	}

	return 2.0f * Rate;
}


#undef LOCTEXT_NAMESPACE
