// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

class SButton;
class UDMXGDTFImportUI;
class IDetailsView;

class SDMXGDTFOptionWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS( SDMXGDTFOptionWindow )
    : _ImportUI(nullptr)
    , _WidgetWindow()
    , _FullPath()
    , _MaxWindowHeight(0.0f)
    , _MaxWindowWidth(0.0f)
    {}

    SLATE_ARGUMENT( UDMXGDTFImportUI*, ImportUI )
    SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
    SLATE_ARGUMENT( FText, FullPath )
    SLATE_ARGUMENT( float, MaxWindowHeight)
    SLATE_ARGUMENT(float, MaxWindowWidth)
    SLATE_END_ARGS()

public:
    void Construct(const FArguments& InArgs);
    virtual bool SupportsKeyboardFocus() const override { return true; }

    FReply OnImport()
    {
        bShouldImport = true;
        if ( WidgetWindow.IsValid() )
        {
            WidgetWindow.Pin()->RequestDestroyWindow();
        }
        return FReply::Handled();
    }

    FReply OnImportAll()
    {
        bShouldImportAll = true;
        return OnImport();
    }

    FReply OnCancel()
    {
        bShouldImport = false;
        bShouldImportAll = false;
        if ( WidgetWindow.IsValid() )
        {
            WidgetWindow.Pin()->RequestDestroyWindow();
        }
        return FReply::Handled();
    }

    virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
    {
        if( InKeyEvent.GetKey() == EKeys::Escape )
        {
            return OnCancel();
        }

        return FReply::Unhandled();
    }

    bool ShouldImport() const
    {
        return bShouldImport;
    }

    bool ShouldImportAll() const
    {
        return bShouldImportAll;
    }

    SDMXGDTFOptionWindow()
        : ImportUI(nullptr)
        , bShouldImport(false)
        , bShouldImportAll(false)
    {}

private:

    FReply OnResetToDefaultClick() const;
    FText GetImportTypeDisplayText() const;


private:
    TWeakObjectPtr<UDMXGDTFImportUI> ImportUI;
    TSharedPtr<IDetailsView> DetailsView;
    TWeakPtr< SWindow > WidgetWindow;
    TSharedPtr< SButton > ImportButton;
    bool			bShouldImport;
    bool			bShouldImportAll;
};
