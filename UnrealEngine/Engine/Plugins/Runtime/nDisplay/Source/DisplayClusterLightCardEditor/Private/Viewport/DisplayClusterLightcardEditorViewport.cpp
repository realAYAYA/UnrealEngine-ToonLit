// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterLightcardEditorViewport.h"

#include "DisplayClusterLightCardEditorCommands.h"
#include "DisplayClusterLightCardEditorStyle.h"
#include "DisplayClusterLightCardEditorUtils.h"
#include "DisplayClusterLightCardEditorViewportClient.h"
#include "DisplayClusterLightCardEditor.h"
#include "LightCardTemplates/DisplayClusterLightCardTemplate.h"
#include "LightCardTemplates/DisplayClusterLightCardTemplateDragDropOp.h"

#include "EditorViewportCommands.h"
#include "ScopedTransaction.h"
#include "SEditorViewportToolBarMenu.h"
#include "STransformViewportToolbar.h"
#include "Framework/Commands/GenericCommands.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet2/DebuggerCommands.h"
#include "Slate/SceneViewport.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "DisplayClusterLightcardEditorViewport"

class SDisplayClusterLightCardEditorViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SDisplayClusterLightCardEditorViewportToolBar){}
		SLATE_ARGUMENT(TWeakPtr<SDisplayClusterLightCardEditorViewport>, EditorViewport)
	SLATE_END_ARGS()

	/** Constructs this widget with the given parameters */
	void Construct(const FArguments& InArgs)
	{
		EditorViewport = InArgs._EditorViewport;
		static const FName DefaultForegroundName("DefaultForeground");

		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.ForegroundColor(FAppStyle::GetSlateColor(DefaultForegroundName))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew(SEditorViewportToolbarMenu)
					.ParentToolBar(SharedThis(this))
					.Cursor(EMouseCursor::Default)
					.Image("EditorViewportToolBar.OptionsDropdown")
					.OnGetMenuContent(this, &SDisplayClusterLightCardEditorViewportToolBar::GeneratePreviewMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew(SEditorViewportToolbarMenu)
					.ParentToolBar(SharedThis(this))
					.Cursor(EMouseCursor::Default)
					.Label(this, &SDisplayClusterLightCardEditorViewportToolBar::GetProjectionMenuLabel)
					.OnGetMenuContent(this, &SDisplayClusterLightCardEditorViewportToolBar::GenerateProjectionMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew(SEditorViewportToolbarMenu)
					.ParentToolBar(SharedThis(this))
					.Cursor(EMouseCursor::Default)
					.Label(LOCTEXT("LightCardEditorViewMenuLabel", "View"))
					.OnGetMenuContent(this, &SDisplayClusterLightCardEditorViewportToolBar::GenerateViewMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew(SEditorViewportToolbarMenu)
					.ParentToolBar(SharedThis(this))
					.Cursor(EMouseCursor::Default)
					.Label(LOCTEXT("LightCardEditorShowMenuLabel", "Show"))
					.OnGetMenuContent(this, &SDisplayClusterLightCardEditorViewportToolBar::GenerateShowMenu)
				]
				+ SHorizontalBox::Slot()
				.Padding(3.0f, 1.0f)
				.HAlign(HAlign_Right)
				[
					MakeDrawingToolBar()
				]
				+ SHorizontalBox::Slot()
				.Padding( 3.0f, 1.0f )
				.HAlign( HAlign_Right )
				[
					MakeTransformToolBar()
				]
			]
		];

		SViewportToolBar::Construct(SViewportToolBar::FArguments());
	}

	/** Creates the preview menu */
	TSharedRef<SWidget> GeneratePreviewMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid()? EditorViewport.Pin()->GetCommandList(): nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;

		FMenuBuilder PreviewOptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);
		{
			// TODO: Any advanced viewport options can be added here
		}

		return PreviewOptionsMenuBuilder.MakeWidget();
	}

	FText GetProjectionMenuLabel() const
	{
		FText Label = LOCTEXT("ProjectionMenuTitle_Default", "Projection");

		if (EditorViewport.IsValid())
		{
			TSharedRef<FDisplayClusterLightCardEditorViewportClient> ViewportClient = EditorViewport.Pin()->GetLightCardEditorViewportClient();
			switch (ViewportClient->GetProjectionMode())
			{
			case EDisplayClusterMeshProjectionType::Linear:
				if (ViewportClient->GetRenderViewportType() == ELevelViewportType::LVT_Perspective)
				{
					Label = LOCTEXT("ProjectionwMenuTitle_Perspective", "Perspective");
				}
				else
				{
					Label = LOCTEXT("ProjectionwMenuTitle_Orthographic", "Orthographic");
				}
				break;

			case EDisplayClusterMeshProjectionType::Azimuthal:
				Label = LOCTEXT("ProjectionwMenuTitle_Azimuthal", "Dome");
				break;

			case EDisplayClusterMeshProjectionType::UV:
				Label = LOCTEXT("ProjectionwMenuTitle_UV", "UV");
				break;
			}
		}

		return Label;
	}

	TSharedRef<SWidget> GenerateProjectionMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid() ? EditorViewport.Pin()->GetCommandList() : nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder ViewMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

		ViewMenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().PerspectiveProjection);
		ViewMenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().OrthographicProjection);
		ViewMenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().AzimuthalProjection);
		ViewMenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().UVProjection);

		return ViewMenuBuilder.MakeWidget();
	}

	TSharedRef<SWidget> GenerateViewMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid() ? EditorViewport.Pin()->GetCommandList() : nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;

		FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);
		{
			MenuBuilder.BeginSection("LightCardEditorViewOrientation", LOCTEXT("ViewOrientationMenuHeader", "View Orientation"));
			{
				MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().ViewOrientationTop);
				MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().ViewOrientationBottom);
				MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().ViewOrientationLeft);
				MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().ViewOrientationRight);
				MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().ViewOrientationFront);
				MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().ViewOrientationBack);
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("LightCardEditorViewOptions", LOCTEXT("ViewOptionsMenuHeader", "View Options"));
			{
				MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().ResetCamera);
			}
			MenuBuilder.EndSection();
		}

		return MenuBuilder.MakeWidget();
	}
	
	TSharedRef<SWidget> GenerateShowMenu() const
	{
		const TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid() ? EditorViewport.Pin()->GetCommandList() : nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;

		FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);
		{
			MenuBuilder.BeginSection("LightCardEditorShow", LOCTEXT("ShowMenuHeader", "Show Flags"));
			{
				MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().ToggleAllLabels);
				MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().ToggleIconVisibility);
			}
			MenuBuilder.EndSection();
		}

		return MenuBuilder.MakeWidget();
	}
	
	TSharedRef<SWidget> MakeTransformToolBar()
	{
		FSlimHorizontalToolBarBuilder ToolbarBuilder(EditorViewport.Pin()->GetCommandList(), FMultiBoxCustomization::None);

		const FName ToolBarStyle = TEXT("EditorViewportToolBar");
		ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
		ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

		ToolbarBuilder.SetIsFocusable(false);

		ToolbarBuilder.BeginSection("Transform");
		{
			ToolbarBuilder.BeginBlockGroup();

			//static FName SelectModeName = FName(TEXT("SelectMode"));
			//ToolbarBuilder.AddToolBarButton(FEditorViewportCommands::Get().SelectMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), SelectModeName);

			static FName TranslateModeName = FName(TEXT("TranslateMode"));
			ToolbarBuilder.AddToolBarButton(FEditorViewportCommands::Get().TranslateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TranslateModeName);

			static FName RotateModeName = FName(TEXT("RotateMode"));
			ToolbarBuilder.AddToolBarButton(FEditorViewportCommands::Get().RotateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), RotateModeName);

			static FName ScaleModeName = FName(TEXT("ScaleMode"));
			ToolbarBuilder.AddToolBarButton(FEditorViewportCommands::Get().ScaleMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), ScaleModeName);

			ToolbarBuilder.EndBlockGroup();
		}

		ToolbarBuilder.EndSection();
		ToolbarBuilder.AddSeparator();

		ToolbarBuilder.AddToolBarButton(FDisplayClusterLightCardEditorCommands::Get().CycleEditorWidgetCoordinateSystem,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>(this, &SDisplayClusterLightCardEditorViewportToolBar::GetLocalToWorldIcon),
			FName(TEXT("CycleTransformGizmoCoordSystem")),

			FNewMenuDelegate::CreateLambda([](FMenuBuilder& InMenuBuilder)
			{
				InMenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().SphericalCoordinateSystem);
				InMenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().CartesianCoordinateSystem);
			}
		));

		return ToolbarBuilder.MakeWidget();
	}

	FSlateIcon GetLocalToWorldIcon() const
	{
		if (EditorViewport.IsValid() && EditorViewport.Pin()->GetLightCardEditorViewportClient()->GetCoordinateSystem() == FDisplayClusterLightCardEditorHelper::ECoordinateSystem::Cartesian)
		{
			return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("EditorViewport.RelativeCoordinateSystem_World"));
		}

		return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Transform"));
	}

	TSharedRef<SWidget> MakeDrawingToolBar()
	{
		FSlimHorizontalToolBarBuilder ToolbarBuilder(EditorViewport.Pin()->GetCommandList(), FMultiBoxCustomization::None);

		const FName ToolBarStyle = TEXT("EditorViewportToolBar");
		ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
		ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

		ToolbarBuilder.SetIsFocusable(false);

		ToolbarBuilder.BeginSection("Drawing");
		{
			ToolbarBuilder.BeginBlockGroup();

			static FName DrawLightCardName = FName(TEXT("DrawLightCard"));
			ToolbarBuilder.AddToolBarButton(
				FDisplayClusterLightCardEditorCommands::Get().DrawLightCard,
				NAME_None,
				LOCTEXT("DrawLC", "Draw Light Card    "),
				LOCTEXT("DrawLCTooltip", 
					"Draw a new custom light card by adding polygon points clicking the left mouse button on the viewport. "
					"End the shape with right mouse button, or abort by pressing this button again."),
				FSlateIcon(FSlateIcon(FDisplayClusterLightCardEditorStyle::Get().GetStyleSetName(), "DrawPoly")),
				DrawLightCardName
			);

			ToolbarBuilder.EndBlockGroup();
		}

		ToolbarBuilder.EndSection();

		return ToolbarBuilder.MakeWidget();
	}

private:
	/** Reference to the parent viewport */
	TWeakPtr<SDisplayClusterLightCardEditorViewport> EditorViewport;
};

const FVector SDisplayClusterLightCardEditorViewport::ViewDirectionTop = FVector(0.0f, 0.0f, 1.0f);
const FVector SDisplayClusterLightCardEditorViewport::ViewDirectionBottom = FVector(0.0f, 0.0f, -1.0f);
const FVector SDisplayClusterLightCardEditorViewport::ViewDirectionLeft = FVector(0.0f, -1.0f, 0.0f);
const FVector SDisplayClusterLightCardEditorViewport::ViewDirectionRight = FVector(0.0f, 1.0f, 0.0f);
const FVector SDisplayClusterLightCardEditorViewport::ViewDirectionFront = FVector(1.0f, 0.0f, 0.0f);
const FVector SDisplayClusterLightCardEditorViewport::ViewDirectionBack = FVector(-1.0f, 0.0f, 0.0f);

void SDisplayClusterLightCardEditorViewport::Construct(const FArguments& InArgs, TSharedPtr<FDisplayClusterLightCardEditor> InLightCardEditor, TSharedPtr<class FUICommandList> InCommandList)
{
	LightCardEditorPtr = InLightCardEditor;
	
	PreviewScene = MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues());
		
	SEditorViewport::Construct(SEditorViewport::FArguments());

	if (InCommandList.IsValid())
	{
		CommandList->Append(InCommandList.ToSharedRef());
	}

	SetRootActor(LightCardEditorPtr.Pin()->GetActiveRootActor().Get());
}

SDisplayClusterLightCardEditorViewport::~SDisplayClusterLightCardEditorViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
		ViewportClient.Reset();
	}

	if (PreviewScene.IsValid())
	{
		if (UWorld* PreviewWorld = PreviewScene->GetWorld())
		{
			// Call DestroyWorld first so OnWorldCleanup is broadcasted while actors still have a valid world.
			// Then mark as garbage to prevent AddReferencedObjects from continuing to be called.
			PreviewWorld->DestroyWorld(true);
			
			PreviewWorld->MarkObjectsPendingKill();
			PreviewWorld->MarkAsGarbage();
		}
		PreviewScene.Reset();
	}
}

TSharedRef<SEditorViewport> SDisplayClusterLightCardEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SDisplayClusterLightCardEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SDisplayClusterLightCardEditorViewport::OnFloatingButtonClicked()
{
}

FReply SDisplayClusterLightCardEditorViewport::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Cache the current mouse position in case the key event invokes the paste here command
	const FVector2D MousePos = FSlateApplication::Get().GetCursorPos();
	const float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(MousePos.X, MousePos.Y);
	PasteHerePos = MyGeometry.AbsoluteToLocal(MousePos) * DPIScale;

	return SEditorViewport::OnKeyDown(MyGeometry, InKeyEvent);
}

void SDisplayClusterLightCardEditorViewport::SetRootActor(ADisplayClusterRootActor* NewRootActor)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->UpdatePreviewActor(NewRootActor);
	}
}

void SDisplayClusterLightCardEditorViewport::SummonContextMenu()
{
	const FVector2D MousePos = FSlateApplication::Get().GetCursorPos();
	const float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(MousePos.X, MousePos.Y);
	PasteHerePos = GetTickSpaceGeometry().AbsoluteToLocal(FSlateApplication::Get().GetCursorPos()) * DPIScale;

	TSharedRef<SWidget> MenuContents = MakeContextMenu();
	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		MenuContents,
		MousePos,
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

TSharedRef<FEditorViewportClient> SDisplayClusterLightCardEditorViewport::MakeEditorViewportClient()
{
	check(PreviewScene.IsValid());
	
	ViewportClient = MakeShareable(new FDisplayClusterLightCardEditorViewportClient(*PreviewScene.Get(),
		SharedThis(this)));

	if (LightCardEditorPtr.IsValid())
	{
		// Call after construction as UpdatePreviewActor will create a weak reference to itself
		ViewportClient->UpdatePreviewActor(LightCardEditorPtr.Pin()->GetActiveRootActor().Get());
	}
	
	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SDisplayClusterLightCardEditorViewport::MakeViewportToolbar()
{
	return
		SNew(SDisplayClusterLightCardEditorViewportToolBar)
		.EditorViewport(SharedThis(this))
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}

void SDisplayClusterLightCardEditorViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);
}

void SDisplayClusterLightCardEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	// Override some of the editor viewport commands to work with the light card editor viewport
	{
		const FEditorViewportCommands& Commands = FEditorViewportCommands::Get();

		CommandList->MapAction(
			Commands.TranslateMode,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetEditorWidgetMode, FDisplayClusterLightCardEditorWidget::EWidgetMode::WM_Translate),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::IsEditorWidgetModeSelected, FDisplayClusterLightCardEditorWidget::EWidgetMode::WM_Translate)
		);
		
		CommandList->MapAction(
			Commands.RotateMode,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetEditorWidgetMode, FDisplayClusterLightCardEditorWidget::EWidgetMode::WM_RotateZ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::IsEditorWidgetModeSelected, FDisplayClusterLightCardEditorWidget::EWidgetMode::WM_RotateZ)
		);

		CommandList->MapAction(
			Commands.ScaleMode,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetEditorWidgetMode, FDisplayClusterLightCardEditorWidget::EWidgetMode::WM_Scale),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::IsEditorWidgetModeSelected, FDisplayClusterLightCardEditorWidget::EWidgetMode::WM_Scale)
		);

		CommandList->MapAction(
			Commands.CycleTransformGizmos,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::CycleEditorWidgetMode),
			FCanExecuteAction()
		);

		// Remove commands that are being replaced by new specific commands for the nDisplay operator
		CommandList->UnmapAction(Commands.Top);
		CommandList->UnmapAction(Commands.Bottom);
		CommandList->UnmapAction(Commands.Left);
		CommandList->UnmapAction(Commands.Right);
		CommandList->UnmapAction(Commands.Front);
		CommandList->UnmapAction(Commands.Back);
		CommandList->UnmapAction(Commands.FocusViewportToSelection);
		CommandList->UnmapAction(Commands.FocusAllViewportsToSelection);
	}

	{
		const FDisplayClusterLightCardEditorCommands& Commands = FDisplayClusterLightCardEditorCommands::Get();

		CommandList->MapAction(
			FDisplayClusterLightCardEditorCommands::Get().PerspectiveProjection,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetProjectionMode, EDisplayClusterMeshProjectionType::Linear, ELevelViewportType::LVT_Perspective),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::IsProjectionModeSelected, EDisplayClusterMeshProjectionType::Linear, ELevelViewportType::LVT_Perspective));

		CommandList->MapAction(
			FDisplayClusterLightCardEditorCommands::Get().OrthographicProjection,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetProjectionMode, EDisplayClusterMeshProjectionType::Linear, ELevelViewportType::LVT_OrthoFreelook),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::IsProjectionModeSelected, EDisplayClusterMeshProjectionType::Linear, ELevelViewportType::LVT_OrthoFreelook));

		CommandList->MapAction(
			FDisplayClusterLightCardEditorCommands::Get().AzimuthalProjection,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetProjectionMode, EDisplayClusterMeshProjectionType::Azimuthal, ELevelViewportType::LVT_Perspective),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::IsProjectionModeSelected, EDisplayClusterMeshProjectionType::Azimuthal, ELevelViewportType::LVT_Perspective));

		CommandList->MapAction(
			FDisplayClusterLightCardEditorCommands::Get().UVProjection,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetProjectionMode, EDisplayClusterMeshProjectionType::UV, ELevelViewportType::LVT_OrthoFreelook),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::IsProjectionModeSelected, EDisplayClusterMeshProjectionType::UV, ELevelViewportType::LVT_OrthoFreelook));

		CommandList->MapAction(
			Commands.ViewOrientationTop,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetViewDirection, ViewDirectionTop));

		CommandList->MapAction(
			Commands.ViewOrientationBottom,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetViewDirection, ViewDirectionBottom));

		CommandList->MapAction(
			Commands.ViewOrientationLeft,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetViewDirection, ViewDirectionLeft));

		CommandList->MapAction(
			Commands.ViewOrientationRight,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetViewDirection, ViewDirectionRight));

		CommandList->MapAction(
			Commands.ViewOrientationFront,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetViewDirection, ViewDirectionFront));

		CommandList->MapAction(
			Commands.ViewOrientationBack,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::SetViewDirection, ViewDirectionBack));

		CommandList->MapAction(
			Commands.ResetCamera,
			FExecuteAction::CreateSP(ViewportClient.Get(), &FDisplayClusterLightCardEditorViewportClient::ResetCamera, false));

		CommandList->MapAction(
			Commands.FrameSelection,
			FExecuteAction::CreateSP(ViewportClient.Get(), &FDisplayClusterLightCardEditorViewportClient::FrameSelection),
			FCanExecuteAction::CreateSP(ViewportClient.Get(), &FDisplayClusterLightCardEditorViewportClient::HasSelection)
		);

		CommandList->MapAction(
			Commands.CycleEditorWidgetCoordinateSystem,
			FExecuteAction::CreateSP(ViewportClient.Get(), &FDisplayClusterLightCardEditorViewportClient::CycleCoordinateSystem));

		CommandList->MapAction(
			Commands.SphericalCoordinateSystem,
			FExecuteAction::CreateSP(ViewportClient.Get(), &FDisplayClusterLightCardEditorViewportClient::SetCoordinateSystem, FDisplayClusterLightCardEditorHelper::ECoordinateSystem::Spherical),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]() { return ViewportClient->GetCoordinateSystem() == FDisplayClusterLightCardEditorHelper::ECoordinateSystem::Spherical; }));

		CommandList->MapAction(
			Commands.CartesianCoordinateSystem,
			FExecuteAction::CreateSP(ViewportClient.Get(), &FDisplayClusterLightCardEditorViewportClient::SetCoordinateSystem, FDisplayClusterLightCardEditorHelper::ECoordinateSystem::Cartesian),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]() { return ViewportClient->GetCoordinateSystem() == FDisplayClusterLightCardEditorHelper::ECoordinateSystem::Cartesian; }));


		CommandList->MapAction(
			FDisplayClusterLightCardEditorCommands::Get().DrawLightCard,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::DrawLightCard),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::IsDrawingLightCard));

		CommandList->MapAction(
			Commands.PasteHere,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::PasteLightCardsHere),
			FCanExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::CanPasteLightCardsHere));

		CommandList->MapAction(
			Commands.ToggleAllLabels,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::ToggleLabels),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::AreLabelsToggled));

		CommandList->MapAction(
			Commands.ToggleIconVisibility,
			FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::ToggleIcons),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDisplayClusterLightCardEditorViewport::AreIconsToggled));
	}
}

FReply SDisplayClusterLightCardEditorViewport::OnDragOver(const FGeometry& MyGeometry,
	const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FDisplayClusterLightCardTemplateDragDropOp> TemplateDragDropOp = DragDropEvent.GetOperationAs<FDisplayClusterLightCardTemplateDragDropOp>();
	if (TemplateDragDropOp.IsValid() && LightCardEditorPtr.IsValid() &&
		LightCardEditorPtr.Pin()->GetActiveRootActor().IsValid() && TemplateDragDropOp->GetTemplate().IsValid())
	{
		TemplateDragDropOp->SetDropAsValid(FText::Format(LOCTEXT("TemplateDragDropOp_LightCardTemplate", "Spawn light card from template {0}"),
			FText::FromString(TemplateDragDropOp->GetTemplate()->GetName())));

		FIntPoint ViewportOrigin, ViewportSize;
		ViewportClient->GetViewportDimensions(ViewportOrigin, ViewportSize);
		const FVector2D MousePos = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition()) * MyGeometry.Scale - ViewportOrigin;

		const TArray<UObject*> DroppedObjects;
		bool bDroppedObjectsVisible = true; 
		ViewportClient->UpdateDropPreviewActors(MousePos.X, MousePos.Y, DroppedObjects, bDroppedObjectsVisible, nullptr);
			
		return FReply::Handled();
	}

	return SEditorViewport::OnDragOver(MyGeometry, DragDropEvent);
}

void SDisplayClusterLightCardEditorViewport::OnDragEnter(const FGeometry& MyGeometry,
	const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FDisplayClusterLightCardTemplateDragDropOp> TemplateDragDropOp = DragDropEvent.GetOperationAs<FDisplayClusterLightCardTemplateDragDropOp>();
	if (TemplateDragDropOp.IsValid() && LightCardEditorPtr.IsValid())
	{
		FIntPoint ViewportOrigin, ViewportSize;
		ViewportClient->GetViewportDimensions(ViewportOrigin, ViewportSize);

		const FVector2D MousePos = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition()) * MyGeometry.Scale - ViewportOrigin;

		const TWeakObjectPtr<UDisplayClusterLightCardTemplate> LightCardTemplate = TemplateDragDropOp->GetTemplate();

		if (LightCardTemplate.IsValid())
		{
			const TArray<UObject*> DroppedObjects{ LightCardTemplate.Get() };
			
			TArray<AActor*> TemporaryActors;
			
			const bool bIsPreview = true;
			ViewportClient->DropObjectsAtCoordinates(MousePos.X, MousePos.Y, DroppedObjects, TemporaryActors, false, bIsPreview, false, nullptr);

			return;
		}
	}
	
	SEditorViewport::OnDragEnter(MyGeometry, DragDropEvent);
}

void SDisplayClusterLightCardEditorViewport::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	ViewportClient->DestroyDropPreviewActors();
	
	const TSharedPtr<FDisplayClusterLightCardTemplateDragDropOp> TemplateDragDropOp = DragDropEvent.GetOperationAs<FDisplayClusterLightCardTemplateDragDropOp>();
	if (TemplateDragDropOp.IsValid())
	{
		TemplateDragDropOp->SetDropAsInvalid();
		return;
	}
	
	SEditorViewport::OnDragLeave(DragDropEvent);
}

FReply SDisplayClusterLightCardEditorViewport::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FDisplayClusterLightCardTemplateDragDropOp> TemplateDragDropOp = DragDropEvent.GetOperationAs<FDisplayClusterLightCardTemplateDragDropOp>();
	if (TemplateDragDropOp.IsValid() && TemplateDragDropOp->CanBeDropped() && LightCardEditorPtr.IsValid())
	{
		FIntPoint ViewportOrigin, ViewportSize;
		ViewportClient->GetViewportDimensions(ViewportOrigin, ViewportSize);

		const FVector2D MousePos = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition()) * MyGeometry.Scale - ViewportOrigin;

		const TWeakObjectPtr<UDisplayClusterLightCardTemplate> LightCardTemplate = TemplateDragDropOp->GetTemplate();

		if (LightCardTemplate.IsValid())
		{
			const TArray<UObject*> DroppedObjects{ LightCardTemplate.Get() };
			
			TArray<AActor*> TemporaryActors;
			// Only select actor on drop
			const bool bSelectActor = true;

			const FScopedTransaction Transaction(LOCTEXT("CreateLightCardFromTemplate", "Create Light Card from Template"));
			ViewportClient->DropObjectsAtCoordinates(MousePos.X, MousePos.Y, DroppedObjects, TemporaryActors, false, false, bSelectActor, nullptr);

			return FReply::Handled();
		}
	}
	
	return SEditorViewport::OnDrop(MyGeometry, DragDropEvent);
}

TSharedRef<SWidget> SDisplayClusterLightCardEditorViewport::MakeContextMenu()
{
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

	MenuBuilder.BeginSection("Actors", LOCTEXT("ActorsSection", "Actors"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("PlaceActorsSubMenuLabel", "Place Actor"),
			LOCTEXT("PlaceActorsSubMenuToolTip", "Add new actors to the stage"),
			FNewMenuDelegate::CreateSP(this, &SDisplayClusterLightCardEditorViewport::MakePlaceActorsSubMenu));

		MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().RemoveLightCard);
		MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().SaveLightCardTemplate);
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection("View", LOCTEXT("ViewSection", "View"));
	{
		MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().FrameSelection);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Edit", LOCTEXT("EditSection", "Edit"));
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Cut);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
		MenuBuilder.AddMenuEntry(FDisplayClusterLightCardEditorCommands::Get().PasteHere);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SDisplayClusterLightCardEditorViewport::MakePlaceActorsSubMenu(FMenuBuilder& MenuBuilder)
{
	FSlateIcon LightCardIcon = FSlateIconFinder::FindIconForClass(ADisplayClusterLightCardActor::StaticClass());
	FSlateIcon FlagIcon = FSlateIconFinder::FindIcon(TEXT("ClassIcon.DisplayClusterLightCardActor.Flag"));
	FSlateIcon UVLightCardIcon = FSlateIconFinder::FindIcon(TEXT("ClassIcon.DisplayClusterLightCardActor.UVLightCard"));

	bool bIsUVMode = false;
	if (ViewportClient.IsValid())
	{
		bIsUVMode = ViewportClient->GetProjectionMode() == EDisplayClusterMeshProjectionType::UV;
	}

	// Add custom UI actions here for the AddNew commands so that the newly added actors can be moved to the user's mouse position after the
	// add operation is performed
	MenuBuilder.AddMenuEntry(
		FDisplayClusterLightCardEditorCommands::Get().AddNewFlag->GetLabel(),
		FDisplayClusterLightCardEditorCommands::Get().AddNewFlag->GetDescription(),
		FlagIcon,
		FUIAction(FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::AddFlagHere),
			FCanExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::CanPlaceActorHere)));

	MenuBuilder.AddMenuEntry(
		FDisplayClusterLightCardEditorCommands::Get().AddNewLightCard->GetLabel(),
		FDisplayClusterLightCardEditorCommands::Get().AddNewLightCard->GetDescription(),
		bIsUVMode ? UVLightCardIcon : LightCardIcon,
		FUIAction(FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::AddLightCardHere),
			FCanExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::CanPlaceActorHere)));

	TSet<UClass*> StageActorClasses = UE::DisplayClusterLightCardEditorUtils::GetAllStageActorClasses();
	for (UClass* Class : StageActorClasses)
	{
		if (Class == ADisplayClusterLightCardActor::StaticClass())
		{
			// Added manually already
			continue;
		}

		FText Label = Class->GetDisplayNameText();
		FSlateIcon StageActorIcon = FSlateIconFinder::FindIconForClass(Class);
		MenuBuilder.AddMenuEntry(
			Label,
			LOCTEXT("AddStageActorHeader", "Add a stage actor to the scene"), 
			StageActorIcon,
			FUIAction(FExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::AddStageActorHere, Class),
				FCanExecuteAction::CreateSP(this, &SDisplayClusterLightCardEditorViewport::CanPlaceActorHere)));
	}
}

void SDisplayClusterLightCardEditorViewport::SetEditorWidgetMode(FDisplayClusterLightCardEditorWidget::EWidgetMode InWidgetMode)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetEditorWidgetMode(InWidgetMode);
	}
}

bool SDisplayClusterLightCardEditorViewport::IsEditorWidgetModeSelected(FDisplayClusterLightCardEditorWidget::EWidgetMode InWidgetMode) const
{
	if (ViewportClient.IsValid())
	{
		return ViewportClient->GetEditorWidgetMode() == InWidgetMode;
	}

	return false;
}

void SDisplayClusterLightCardEditorViewport::DrawLightCard()
{
	if (!ViewportClient.IsValid())
	{
		return;
	}

	if (IsDrawingLightCard())
	{
		ViewportClient->ExitDrawingLightCardMode();
	}
	else
	{
		ViewportClient->EnterDrawingLightCardMode();
	}
}


void SDisplayClusterLightCardEditorViewport::CycleEditorWidgetMode()
{
	int32 WidgetModeAsInt = ViewportClient->GetEditorWidgetMode();

	WidgetModeAsInt = (WidgetModeAsInt + 1) % FDisplayClusterLightCardEditorWidget::EWidgetMode::WM_Max;

	SetEditorWidgetMode((FDisplayClusterLightCardEditorWidget::EWidgetMode)WidgetModeAsInt);
}

void SDisplayClusterLightCardEditorViewport::SetProjectionMode(EDisplayClusterMeshProjectionType InProjectionMode, ELevelViewportType InViewportType)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetProjectionMode(InProjectionMode, InViewportType);
	}
}

bool SDisplayClusterLightCardEditorViewport::IsProjectionModeSelected(EDisplayClusterMeshProjectionType InProjectionMode, ELevelViewportType ViewportType) const
{
	if (ViewportClient.IsValid())
	{
		return ViewportClient->GetProjectionMode() == InProjectionMode && ViewportClient->GetRenderViewportType() == ViewportType;
	}

	return false;
}

void SDisplayClusterLightCardEditorViewport::SetViewDirection(FVector InViewDirection)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetViewRotation(InViewDirection.Rotation());
	}
}

bool SDisplayClusterLightCardEditorViewport::IsDrawingLightCard() const
{
	if (ViewportClient.IsValid())
	{
		return ViewportClient->GetInputMode() == FDisplayClusterLightCardEditorViewportClient::EInputMode::DrawingLightCard;
	}

	return false;
}

void SDisplayClusterLightCardEditorViewport::PasteLightCardsHere()
{
	if (LightCardEditorPtr.IsValid())
	{
		LightCardEditorPtr.Pin()->PasteActors();

		if (ViewportClient.IsValid())
		{
			// Perform the positioning of the pasted light cards on the next scene refresh, since the light card proxies will have not been 
			// regenerated until then
			ViewportClient->GetOnNextSceneRefresh().AddLambda([this]()
			{
				ViewportClient->MoveSelectedActorsToPixel(FIntPoint(PasteHerePos.X, PasteHerePos.Y));
			});
		}
	}
}

bool SDisplayClusterLightCardEditorViewport::CanPasteLightCardsHere() const
{
	if (LightCardEditorPtr.IsValid())
	{
		return LightCardEditorPtr.Pin()->CanPasteActors();
	}

	return false;
}

void SDisplayClusterLightCardEditorViewport::AddLightCardHere()
{
	if (LightCardEditorPtr.IsValid())
	{
		LightCardEditorPtr.Pin()->AddNewLightCard();

		if (ViewportClient.IsValid())
		{
			// Perform the positioning of the pasted light cards on the next scene refresh, since the light card proxies will have not been 
			// regenerated until then
			ViewportClient->GetOnNextSceneRefresh().AddLambda([this]()
			{
				ViewportClient->MoveSelectedActorsToPixel(FIntPoint(PasteHerePos.X, PasteHerePos.Y));
			});
		}
	}
}

void SDisplayClusterLightCardEditorViewport::AddFlagHere()
{
	if (LightCardEditorPtr.IsValid())
	{
		LightCardEditorPtr.Pin()->AddNewFlag();

		if (ViewportClient.IsValid())
		{
			// Perform the positioning of the pasted light cards on the next scene refresh, since the light card proxies will have not been 
			// regenerated until then
			ViewportClient->GetOnNextSceneRefresh().AddLambda([this]()
			{
				ViewportClient->MoveSelectedActorsToPixel(FIntPoint(PasteHerePos.X, PasteHerePos.Y));
			});
		}
	}
}

void SDisplayClusterLightCardEditorViewport::AddStageActorHere(UClass* InClass)
{
	if (LightCardEditorPtr.IsValid())
	{
		LightCardEditorPtr.Pin()->AddNewDynamic(InClass);

		if (ViewportClient.IsValid())
		{
			// Perform the positioning of the pasted light cards on the next scene refresh, since the light card proxies will have not been 
			// regenerated until then
			ViewportClient->GetOnNextSceneRefresh().AddLambda([this]()
			{
				ViewportClient->MoveSelectedActorsToPixel(FIntPoint(PasteHerePos.X, PasteHerePos.Y));
			});
		}
	}
}

bool SDisplayClusterLightCardEditorViewport::CanPlaceActorHere() const
{
	return LightCardEditorPtr.IsValid() && LightCardEditorPtr.Pin()->CanAddNewActor();
}

void SDisplayClusterLightCardEditorViewport::ToggleLabels()
{
	// TODO: Handle CCR
	if (LightCardEditorPtr.IsValid())
	{
		return LightCardEditorPtr.Pin()->ToggleLightCardLabels();
	}
}

bool SDisplayClusterLightCardEditorViewport::AreLabelsToggled() const
{
	// TODO: Handle CCR
	return LightCardEditorPtr.IsValid() && LightCardEditorPtr.Pin()->ShouldShowLightCardLabels();
}

void SDisplayClusterLightCardEditorViewport::ToggleIcons()
{
	if (LightCardEditorPtr.IsValid())
	{
		LightCardEditorPtr.Pin()->ShowIcons(!AreIconsToggled());
	}
}

bool SDisplayClusterLightCardEditorViewport::AreIconsToggled() const
{
	return LightCardEditorPtr.IsValid() && LightCardEditorPtr.Pin()->ShouldShowIcons();
}

#undef LOCTEXT_NAMESPACE
