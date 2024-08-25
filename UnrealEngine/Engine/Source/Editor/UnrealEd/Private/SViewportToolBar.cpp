// Copyright Epic Games, Inc. All Rights Reserved.


#include "SViewportToolBar.h"

#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"
#include "Widgets/Input/SMenuAnchor.h"

struct FSlateBrush;

#define LOCTEXT_NAMESPACE "ViewportToolBar"

void SViewportToolBar::Construct(const FArguments& InArgs)
{
}

TWeakPtr<SMenuAnchor> SViewportToolBar::GetOpenMenu() const
{
	return OpenedMenu;
}

void SViewportToolBar::SetOpenMenu( TSharedPtr< SMenuAnchor >& NewMenu )
{
	if( OpenedMenu.IsValid() && OpenedMenu.Pin() != NewMenu )
	{
		// Close any other open menus
		OpenedMenu.Pin()->SetIsOpen( false );
	}
	OpenedMenu = NewMenu;
}




FText SViewportToolBar::GetCameraMenuLabelFromViewportType(const ELevelViewportType ViewportType) const
{
	FText Label = LOCTEXT("CameraMenuTitle_Default", "Camera");
	switch (ViewportType)
	{
	case LVT_Perspective:
		Label = LOCTEXT("CameraMenuTitle_Perspective", "Perspective");
		break;

	case LVT_OrthoXY:
		Label = LOCTEXT("CameraMenuTitle_Top", "Top");
		break;

	case LVT_OrthoNegativeXZ:
		Label = LOCTEXT("CameraMenuTitle_Left", "Left");
		break;

	case LVT_OrthoNegativeYZ:
		Label = LOCTEXT("CameraMenuTitle_Front", "Front");
		break;

	case LVT_OrthoNegativeXY:
		Label = LOCTEXT("CameraMenuTitle_Bottom", "Bottom");
		break;

	case LVT_OrthoXZ:
		Label = LOCTEXT("CameraMenuTitle_Right", "Right");
		break;

	case LVT_OrthoYZ:
		Label = LOCTEXT("CameraMenuTitle_Back", "Back");
		break;
	case LVT_OrthoFreelook:
		break;
	}

	return Label;
}

const FSlateBrush* SViewportToolBar::GetCameraMenuLabelIconFromViewportType(const ELevelViewportType ViewportType) const
{
	static FName PerspectiveIcon("EditorViewport.Perspective");
	static FName TopIcon("EditorViewport.Top");
	static FName LeftIcon("EditorViewport.Left");
	static FName FrontIcon("EditorViewport.Front");
	static FName BottomIcon("EditorViewport.Bottom");
	static FName RightIcon("EditorViewport.Right");
	static FName BackIcon("EditorViewport.Back");

	FName Icon = NAME_None;

	switch (ViewportType)
	{
	case LVT_Perspective:
		Icon = PerspectiveIcon;
		break;

	case LVT_OrthoXY:
		Icon = TopIcon;
		break;

	case LVT_OrthoNegativeXZ:
		Icon = LeftIcon;
		break;

	case LVT_OrthoNegativeYZ:
		Icon = FrontIcon;
		break;

	case LVT_OrthoNegativeXY:
		Icon = BottomIcon;
		break;

	case LVT_OrthoXZ:
		Icon = RightIcon;
		break;

	case LVT_OrthoYZ:
		Icon = BackIcon;
		break;
	case LVT_OrthoFreelook:
		break;
	}

	return FAppStyle::GetBrush(Icon);
}

bool SViewportToolBar::IsViewModeSupported(EViewModeIndex ViewModeIndex) const 
{
	switch (ViewModeIndex)
	{
	case VMI_PrimitiveDistanceAccuracy:
	case VMI_MaterialTextureScaleAccuracy:
	case VMI_RequiredTextureResolution:
		return false;
	default:
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
