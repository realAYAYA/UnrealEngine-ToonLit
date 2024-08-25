// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaPlateCustomization.h"

#include "CineCameraSettings.h"
#include "Components/StaticMeshComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IMediaAssetsModule.h"
#include "LevelEditorViewport.h"
#include "MediaPlate.h"
#include "MediaPlateComponent.h"
#include "MediaPlateEditorModule.h"
#include "MediaPlateEditorStyle.h"
#include "MediaPlayer.h"
#include "MediaPlayerEditorModule.h"
#include "MediaPlaylist.h"
#include "MediaSource.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
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
	TWeakPtr<FMediaPlateCustomization> WeakSelf = StaticCastWeakPtr<FMediaPlateCustomization>(AsWeak());

	// Is this the media plate editor window?
	bool bIsMediaPlateWindow = false;

	if (const IDetailsView* DetailsView = DetailBuilder.GetDetailsView())
	{
		TSharedPtr<FTabManager> HostTabManager = DetailsView->GetHostTabManager();
		bIsMediaPlateWindow = (HostTabManager.IsValid() == false);
	}

	// Get style.
	const ISlateStyle* Style = &FMediaPlateEditorStyle::Get().Get();

	CustomizeCategories(DetailBuilder);

	IDetailCategoryBuilder& ControlCategory = DetailBuilder.EditCategory("Control");
	IDetailCategoryBuilder& PlaylistCategory = DetailBuilder.EditCategory("Playlist");
	IDetailCategoryBuilder& GeometryCategory = DetailBuilder.EditCategory("Geometry");
	IDetailCategoryBuilder& MediaDetailsCategory = DetailBuilder.EditCategory("MediaDetails");

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
	AddMeshCustomization(GeometryCategory);

	// Add playlist.
	PlaylistCategory.AddCustomRow(FText::FromString("Playlist Asset"))
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
	PlaylistCategory.AddCustomRow(FText::FromString("First Item In Playlist"))
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
				.Value_Lambda([WeakSelf]()
				{
					const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
					return Self.IsValid() ? Self->bIsMediaSourceAsset : false;
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
	PlaylistCategory.AddCustomRow(FText::FromString("Media Source Asset"))
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
	PlaylistCategory.AddCustomRow(FText::FromString("Media Path"))
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

	// Add media player playback slider
	if (IMediaPlayerEditorModule* MediaPlayerEditorModule = FModuleManager::LoadModulePtr<IMediaPlayerEditorModule>("MediaPlayerEditor"))
	{
		const TSharedRef<IMediaPlayerSlider> MediaPlayerSlider =
			MediaPlayerEditorModule->CreateMediaPlayerSliderWidget(GetMediaPlayer());

		MediaPlayerSlider->SetSliderHandleColor(FSlateColor(EStyleColor::AccentBlue));
		MediaPlayerSlider->SetVisibleWhenInactive(EVisibility::Visible);

		ControlCategory.AddCustomRow(LOCTEXT("MediaPlatePlaybackPosition", "Playback Position"))
		[
			MediaPlayerSlider
		];
	}

	// Add media control buttons.
	ControlCategory.AddCustomRow(LOCTEXT("MediaPlateControls", "MediaPlate Controls"))
		[
			SNew(SHorizontalBox)
			// Rewind button.
			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
						.VAlign(VAlign_Center)
						.IsEnabled_Lambda([WeakSelf]
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								if (const UMediaPlayer* MediaPlayer = Self->GetMediaPlayer())
								{
									return MediaPlayer->IsReady() &&
										MediaPlayer->SupportsSeeking() &&
										MediaPlayer->GetTime() > FTimespan::Zero();
								}
							}

							return false;
						})
						.OnClicked_Lambda([WeakSelf]() -> FReply
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								Self->OnButtonEvent(EMediaPlateEventState::Rewind);
								return FReply::Handled();
							}
							return FReply::Unhandled();
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
						.IsEnabled_Lambda([WeakSelf]
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								if (UMediaPlayer* MediaPlayer = Self->GetMediaPlayer())
								{
									return MediaPlayer->IsReady() && MediaPlayer->SupportsRate(UMediaPlateComponent::GetReverseRate(MediaPlayer), false);
								}
							}

							return false;
						})
						.OnClicked_Lambda([WeakSelf]() -> FReply
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								Self->OnButtonEvent(EMediaPlateEventState::Reverse);
								return FReply::Handled();
							}
							return FReply::Unhandled();
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
						.IsEnabled_Lambda([WeakSelf]
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								// Is the player paused or fast forwarding/rewinding?
								if (const UMediaPlayer* MediaPlayer = Self->GetMediaPlayer())
								{
									return MediaPlayer->IsReady()
										&& (!MediaPlayer->IsPlaying() || (MediaPlayer->GetRate() != 1.0f));
								}
							}

							return false;
						})
						.Visibility_Lambda([WeakSelf]
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								if (const UMediaPlayer* MediaPlayer = Self->GetMediaPlayer())
								{
									return MediaPlayer->IsPlaying() ? EVisibility::Collapsed : EVisibility::Visible;
								}
							}

							return EVisibility::Visible;
						})
						.OnClicked_Lambda([WeakSelf]() -> FReply
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								Self->OnButtonEvent(EMediaPlateEventState::Play);
								return FReply::Handled();
							}

							return FReply::Unhandled();
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
						.IsEnabled_Lambda([WeakSelf]
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								if (const UMediaPlayer* MediaPlayer = Self->GetMediaPlayer())
								{
									return MediaPlayer->CanPause() && !MediaPlayer->IsPaused();
								}
							}

							return false;
						})
						.Visibility_Lambda([WeakSelf]
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								if (const UMediaPlayer* MediaPlayer = Self->GetMediaPlayer())
								{
									const bool bIsVisible = MediaPlayer->CanPause() && !MediaPlayer->IsPaused();
									return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
								}
							}

							return EVisibility::Collapsed;
						})
						.OnClicked_Lambda([WeakSelf]() -> FReply
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								Self->OnButtonEvent(EMediaPlateEventState::Pause);
								return FReply::Handled();
							}
							return FReply::Unhandled();
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
					.IsEnabled_Lambda([WeakSelf]
					{
						const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
						if (Self.IsValid())
						{
							if (UMediaPlayer* MediaPlayer = Self->GetMediaPlayer())
							{
								return MediaPlayer->IsReady() && MediaPlayer->SupportsRate(UMediaPlateComponent::GetForwardRate(MediaPlayer), false);
							}
						}

						return false;
					})
					.OnClicked_Lambda([WeakSelf]() -> FReply
					{
						const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
						if (Self.IsValid())
						{
							Self->OnButtonEvent(EMediaPlateEventState::Forward);
							return FReply::Handled();
						}
						return FReply::Unhandled();
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
						.OnClicked_Lambda([WeakSelf]() -> FReply
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								Self->OnButtonEvent(EMediaPlateEventState::Open);
								return FReply::Handled();
							}
							return FReply::Unhandled();
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
						.IsEnabled_Lambda([WeakSelf]
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								if (UMediaPlayer* MediaPlayer = Self->GetMediaPlayer())
								{
									return !MediaPlayer->GetUrl().IsEmpty();
								}
							}

							return false;
						})
						.OnClicked_Lambda([WeakSelf]() -> FReply
						{
							const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
							if (Self.IsValid())
							{
								Self->OnButtonEvent(EMediaPlateEventState::Close);
								return FReply::Handled();
							}
							return FReply::Unhandled();
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
		PlaylistCategory.AddCustomRow(LOCTEXT("OpenMediaPlate", "Open Media Plate"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.Padding(0, 5, 10, 5)
					[
						SNew(SButton)
							.ContentPadding(3.0f)
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
			MediaDetailsCategory.AddCustomRow(FText::FromString(TEXT("Media Details")))
			[
				SNew(SMediaPlateEditorMediaDetails, *FirstMediaPlate)
			];
		}
	}
}

void FMediaPlateCustomization::AddMeshCustomization(IDetailCategoryBuilder& InParentCategory)
{
	TWeakPtr<FMediaPlateCustomization> WeakSelf = StaticCastWeakPtr<FMediaPlateCustomization>(AsWeak());

	// Add radio buttons for mesh type.
	InParentCategory.AddCustomRow(FText::FromString("Mesh Selection"))
	[
		SNew(SSegmentedControl<EMediaTextureVisibleMipsTiles>)
			.Value_Lambda([WeakSelf]()
			{
				const TSharedPtr<FMediaPlateCustomization> Self = WeakSelf.Pin();
				return Self.IsValid() ? Self->MeshMode : EMediaTextureVisibleMipsTiles::None;
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
	InParentCategory.AddCustomRow(FText::FromString("Mesh Selection"))
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
						.ContentPadding(2.0f)
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
					SNew(SSpinBox<float>)
						.Value(this, &FMediaPlateCustomization::GetAspectRatio)
						.MinValue(0.0f)
						.OnValueChanged(this, &FMediaPlateCustomization::SetAspectRatio)
				]
		];

		// Add letterbox aspect ratio.
		InParentCategory.AddCustomRow(FText::FromString("Aspect Ratio"))
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
						.ContentPadding(2.0f)
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
				SNew(SSpinBox<float>)
					.Value(this, &FMediaPlateCustomization::GetLetterboxAspectRatio)
					.MinValue(0.0f)
					.OnValueChanged(this, &FMediaPlateCustomization::SetLetterboxAspectRatio)
			]
		];

	// Add auto aspect ratio.
	InParentCategory.AddCustomRow(FText::FromString("Aspect Ratio"))
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
	InParentCategory.AddCustomRow(FText::FromString("Horizontal Arc"))
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
	InParentCategory.AddCustomRow(FText::FromString("Vertical Arc"))
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
	InParentCategory.AddCustomRow(FText::FromString("Static Mesh"))
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
		const FScopedTransaction Transaction(LOCTEXT("SetMeshMode", "Media Plate Mesh Changed"));

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
			ECheckBoxState NewState = MediaPlate->GetIsAspectRatioAuto() ? ECheckBoxState::Checked :
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
			MediaPlate->SetIsAspectRatioAuto(bEnable);
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

float FMediaPlateCustomization::GetAspectRatio() const
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

	return 1.0f;
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


float FMediaPlateCustomization::GetLetterboxAspectRatio() const
{
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			return MediaPlate->GetLetterboxAspectRatio();
		}
	}

	return 0.0f;
}

void FMediaPlateCustomization::SetMeshHorizontalRange(float HorizontalRange)
{
	HorizontalRange = FMath::Clamp(HorizontalRange, 1.0f, 360.0f);
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
			return static_cast<float>(MediaPlate->GetMeshRange().X);
		}
	}

	return TOptional<float>();
}

void FMediaPlateCustomization::SetMeshVerticalRange(float VerticalRange)
{
	VerticalRange = FMath::Clamp(VerticalRange, 1.0f, 180.0f);
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
			return static_cast<float>(MediaPlate->GetMeshRange().Y);
		}
	}

	return TOptional<float>();
}

void FMediaPlateCustomization::SetMeshRange(FVector2D Range)
{
	const FScopedTransaction Transaction(LOCTEXT("SetMeshRange", "Media Plate Set Mesh Range"));

	// Loop through all our objects.
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
		if (MediaPlate != nullptr)
		{
			if (MediaPlate->GetMeshRange() != Range)
			{
				MediaPlate->Modify();
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
	const FScopedTransaction Transaction(LOCTEXT("OnStaticMeshChanged", "Media Plate Custom Mesh Changed"));

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

		{
			const FScopedTransaction Transaction(LOCTEXT("SetIsMediaSourceAsset", "Media Source Playlist Changed"));

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
							Playlist->Modify();
							Playlist->Replace(0, nullptr);
						}
					}
					break;
				}
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

	{
		const FScopedTransaction Transaction(LOCTEXT("OnPlaylistChanged", "Media Playlist Changed"));
		
		// Update the playlist.
		UMediaPlaylist* Playlist = Cast<UMediaPlaylist>(AssetData.GetAsset());
		for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
		{
			UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
			if (MediaPlate != nullptr)
			{
				MediaPlate->Modify();
				MediaPlate->MediaPlaylist = Playlist;
			}
		}
	}

	UpdateIsMediaSourceAsset();
	UpdateMediaPath();
}

void FMediaPlateCustomization::OnMediaSourceChanged(const FAssetData& AssetData)
{
	// Update the playlist with the new media source.
	UMediaSource* MediaSource = Cast<UMediaSource>(AssetData.GetAsset());

	{
		const FScopedTransaction Transaction(LOCTEXT("OnMediaSourceChanged", "Media Source Playlist Changed"));

		for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
		{
			UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
			if (MediaPlate != nullptr)
			{
				// Get playlist.
				TObjectPtr<UMediaPlaylist>& Playlist = MediaPlate->MediaPlaylist;
				if (Playlist == nullptr)
				{
					Playlist = NewObject<UMediaPlaylist>(MediaPlate, NAME_None, RF_Transactional);
				}
				else
				{
					Playlist->Modify();
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
			}
		}
	}

	StopMediaPlates();
	UpdateMediaPath();
}

void FMediaPlateCustomization::OnButtonEvent(EMediaPlateEventState State)
{
	IMediaAssetsModule* MediaAssets = FModuleManager::LoadModulePtr<IMediaAssetsModule>("MediaAssets");
	TArray<FString> ActorsPathNames;
	ActorsPathNames.Reserve(MediaPlatesList.Num());
	for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		if (!MediaPlatePtr.IsValid())
		{
			continue;
		}

		ActorsPathNames.Add(MediaPlatePtr->GetOwner()->GetPathName());
			
		if (State == EMediaPlateEventState::Open)
		{
			// Tell the editor module that this media plate is playing.
			FMediaPlateEditorModule* EditorModule = FModuleManager::LoadModulePtr<FMediaPlateEditorModule>("MediaPlateEditor");
			if (EditorModule != nullptr)
			{
				EditorModule->MediaPlateStartedPlayback(MediaPlatePtr.Get());
			}
		}

		MediaPlatePtr->SwitchStates(State);
	}
	MediaAssets->BroadcastOnMediaStateChangedEvent(ActorsPathNames, (uint8)State);
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

		{
			const FScopedTransaction Transaction(LOCTEXT("OnMediaPathChanged", "Media Path Changed"));

			// Set up media source for our media plates.
			for (TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
			{
				UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get();
				if (MediaPlate != nullptr)
				{
					// Get playlist.
					TObjectPtr<UMediaPlaylist>& Playlist = MediaPlate->MediaPlaylist;
					if (Playlist == nullptr)
					{
						Playlist = NewObject<UMediaPlaylist>(MediaPlate, NAME_None, RF_Transactional);
					}
					else
					{
						Playlist->Modify();
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
					}
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
	OnButtonEvent(EMediaPlateEventState::Close);
}

UMediaPlayer* FMediaPlateCustomization::GetMediaPlayer() const
{
	for (const TWeakObjectPtr<UMediaPlateComponent>& MediaPlatePtr : MediaPlatesList)
	{
		if (UMediaPlateComponent* MediaPlate = MediaPlatePtr.Get())
		{
			if (UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer())
			{
				return MediaPlayer;
			}
		}
	}

	return nullptr;
}

void FMediaPlateCustomization::CustomizeCategories(IDetailLayoutBuilder& InDetailBuilder)
{
	static const FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	// Rearrange Categories

	const FName MediaPlateComponentName = UMediaPlateComponent::StaticClass()->GetFName();

	const TSharedRef<FPropertySection> GeneralSection = PropertyModule.FindOrCreateSection(MediaPlateComponentName, TEXT("General"), LOCTEXT("General", "General"));
	GeneralSection->AddCategory(TEXT("Control"));
	GeneralSection->AddCategory(TEXT("Geometry"));
	GeneralSection->AddCategory(TEXT("Playlist"));
	GeneralSection->AddCategory(TEXT("MediaDetails"));
	GeneralSection->AddCategory(TEXT("MediaTexture"));
	GeneralSection->AddCategory(TEXT("Materials"));
	GeneralSection->AddCategory(TEXT("EXR Tiles & Mips"));
	GeneralSection->AddCategory(TEXT("Media Cache"));
	GeneralSection->AddCategory(TEXT("Advanced"));

	const TSharedRef<FPropertySection> MediaSection = PropertyModule.FindOrCreateSection(MediaPlateComponentName, TEXT("Media"), LOCTEXT("Media", "Media"));
	MediaSection->AddCategory(TEXT("Playlist"));
	MediaSection->AddCategory(TEXT("MediaDetails"));
	MediaSection->AddCategory(TEXT("Media Cache"));

	const TSharedRef<FPropertySection> EXRSection = PropertyModule.FindOrCreateSection(MediaPlateComponentName, TEXT("EXR"), LOCTEXT("EXR", "EXR"));
	EXRSection->AddCategory(TEXT("MediaDetails"));
	EXRSection->AddCategory(TEXT("EXR Tiles & Mips"));
	EXRSection->AddCategory(TEXT("Media Cache"));

	const TSharedRef<FPropertySection> RenderingSection = PropertyModule.FindOrCreateSection(MediaPlateComponentName, TEXT("Rendering"), LOCTEXT("Rendering", "Rendering"));
	RenderingSection->AddCategory(TEXT("Geometry"));
	RenderingSection->AddCategory(TEXT("Materials"));
	RenderingSection->AddCategory(TEXT("MediaTexture"));
	RenderingSection->AddCategory(TEXT("Mobility"));
	RenderingSection->AddCategory(TEXT("Transform"));
	RenderingSection->AddCategory(TEXT("TransformCommon"));
	RenderingSection->RemoveCategory(TEXT("Lighting"));
	RenderingSection->AddCategory(TEXT("MediaTexture"));
	RenderingSection->RemoveCategory(TEXT("MaterialParameters"));
	RenderingSection->RemoveCategory(TEXT("Mobile"));
	RenderingSection->RemoveCategory(TEXT("RayTracing"));
	RenderingSection->RemoveCategory(TEXT("Rendering"));
	RenderingSection->RemoveCategory(TEXT("TextureStreaming"));
	RenderingSection->RemoveCategory(TEXT("VirtualTexture"));

	// Hide unwanted Categories

	const FName MediaPlateName = AMediaPlate::StaticClass()->GetFName();

	const TSharedRef<FPropertySection> MediaPlateMiscSection = PropertyModule.FindOrCreateSection(MediaPlateName, TEXT("Misc"), LOCTEXT("Misc", "Misc"));
	MediaPlateMiscSection->RemoveCategory("AssetUserData");
	MediaPlateMiscSection->RemoveCategory("Cooking");
	MediaPlateMiscSection->RemoveCategory("Input");
	MediaPlateMiscSection->RemoveCategory("Navigation");
	MediaPlateMiscSection->RemoveCategory("Replication");
	MediaPlateMiscSection->RemoveCategory("Tags");

	const TSharedRef<FPropertySection> MediaPlateStreamingSection = PropertyModule.FindOrCreateSection(MediaPlateName, TEXT("Streaming"), LOCTEXT("Streaming", "Streaming"));
	MediaPlateStreamingSection->RemoveCategory("Data Layers");
	MediaPlateStreamingSection->RemoveCategory("HLOD");
	MediaPlateStreamingSection->RemoveCategory("World Partition");

	const TSharedRef<FPropertySection> MediaPlateLODSection = PropertyModule.FindOrCreateSection(MediaPlateName, TEXT("LOD"), LOCTEXT("LOD", "LOD"));
	MediaPlateLODSection->RemoveCategory("HLOD");
	MediaPlateLODSection->RemoveCategory("LOD");

	const TSharedRef<FPropertySection> MediaPlatePhysicsSection = PropertyModule.FindOrCreateSection(MediaPlateName, TEXT("Physics"), LOCTEXT("Physics", "Physics"));
	MediaPlatePhysicsSection->RemoveCategory("Collision");
	MediaPlatePhysicsSection->RemoveCategory("Physics");

	// Hide the static mesh.
	IDetailCategoryBuilder& StaticMeshCategory = InDetailBuilder.EditCategory("StaticMesh");
	StaticMeshCategory.SetCategoryVisibility(false);

	IDetailCategoryBuilder& ControlCategory = InDetailBuilder.EditCategory("Control");
	IDetailCategoryBuilder& MediaDetailsCategory = InDetailBuilder.EditCategory("MediaDetails");
	IDetailCategoryBuilder& PlaylistCategory = InDetailBuilder.EditCategory("Playlist");
	IDetailCategoryBuilder& GeometryCategory = InDetailBuilder.EditCategory("Geometry");
	IDetailCategoryBuilder& MediaTextureCategory = InDetailBuilder.EditCategory("MediaTexture");
	IDetailCategoryBuilder& MaterialsCategory = InDetailBuilder.EditCategory("Materials");
	IDetailCategoryBuilder& TilesMipsCategory = InDetailBuilder.EditCategory("EXR Tiles & Mips");
	IDetailCategoryBuilder& MediaCacheCategory = InDetailBuilder.EditCategory("Media Cache");

	// Rename Media Cache category and look ahead property
	MediaCacheCategory.SetDisplayName(FText::FromString(TEXT("Cache")));

	const TSharedRef<IPropertyHandle> CacheSettingsProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMediaPlateComponent, CacheSettings));
	if (const auto LookAheadTimeProperty = CacheSettingsProperty->GetChildHandle(TEXT("TimeToLookAhead")))
	{
		LookAheadTimeProperty->SetPropertyDisplayName(FText::FromString("Look Ahead Time"));
	}

	// Start from a Priority value which places these categories after the Transform one
	uint32 Priority = 2010;
	ControlCategory.SetSortOrder(Priority++);
	GeometryCategory.SetSortOrder(Priority++);
	PlaylistCategory.SetSortOrder(Priority++);
	MediaDetailsCategory.SetSortOrder(Priority++);
	MediaTextureCategory.SetSortOrder(Priority++);
	MaterialsCategory.SetSortOrder(Priority++);
	TilesMipsCategory.SetSortOrder(Priority++);
	MediaCacheCategory.SetSortOrder(Priority);
}
#undef LOCTEXT_NAMESPACE
