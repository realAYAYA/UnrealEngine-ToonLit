// Copyright Epic Games, Inc. All Rights Reserved.

#include "TranslationPickerFloatingWindow.h"

#include "Containers/SortedMap.h"
#include "Containers/UnrealString.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Text/TextLayout.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/ArrangedWidget.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Layout/ChildrenBase.h"
#include "Layout/Margin.h"
#include "Layout/SlateRect.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "SDocumentationToolTip.h"
#include "SlotBase.h"
#include "TranslationPickerEditWindow.h"
#include "TranslationPickerWidget.h"
#include "Types/SlateEnums.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

class ICursor;
struct FGeometry;

#define LOCTEXT_NAMESPACE "TranslationPicker"

class FTranslationPickerInputProcessor : public IInputProcessor
{
public:
	FTranslationPickerInputProcessor(STranslationPickerFloatingWindow* InOwner)
		: Owner(InOwner)
	{
	}

	void SetOwner(STranslationPickerFloatingWindow* InOwner)
	{
		Owner = InOwner;
	}

	virtual ~FTranslationPickerInputProcessor() = default;

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (Owner && InKeyEvent.GetKey() == EKeys::Escape)
		{
			Owner->OnEscapePressed();
			return true;
		}

		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("TranslationPicker"); }

private:
	STranslationPickerFloatingWindow* Owner;
};

void STranslationPickerFloatingWindow::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	WindowContents = SNew(SToolTip);

	ChildSlot
	[
		WindowContents.ToSharedRef()
	];

	InputProcessor = MakeShared<FTranslationPickerInputProcessor>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
}

STranslationPickerFloatingWindow::~STranslationPickerFloatingWindow()
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->SetOwner(nullptr);
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}
		InputProcessor.Reset();
	}
}

void STranslationPickerFloatingWindow::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(FSlateApplication::Get().GetCursorPos(), FSlateApplication::Get().GetInteractiveTopLevelWindows(), true);

	if (Path.IsValid())
	{
		// If the path of widgets we're hovering over changed since last time (or if this is the first tick and LastTickHoveringWidgetPath hasn't been set yet)
		if (!LastTickHoveringWidgetPath.IsValid() || LastTickHoveringWidgetPath.ToWidgetPath().ToString() != Path.ToString())
		{
			// Clear all previous text and widgets
			PickedTexts.Reset();

			// Process the leaf-widget under the cursor
			if (Path.Widgets.Num() > 0)
			{
				// General Widget case
				TSharedRef<SWidget> PathWidget = Path.Widgets.Last().Widget;
				PickTextFromWidget(PathWidget);

				// Tooltip case
				TSharedPtr<IToolTip> Tooltip = PathWidget->GetToolTip();
				if (Tooltip.IsValid() && !Tooltip->IsEmpty())
				{
					PickTextFromWidget(Tooltip->AsWidget());
				}

				// Also include tooltips from parent widgets in this path (since they may be visible)
				for (int32 ParentPathIndex = Path.Widgets.Num() - 2; ParentPathIndex >= 0; --ParentPathIndex)
				{
					TSharedRef<SWidget> ParentPathWidget = Path.Widgets[ParentPathIndex].Widget;

					// Tooltip case
					TSharedPtr<IToolTip> ParentTooltip = ParentPathWidget->GetToolTip();
					if (ParentTooltip.IsValid() && !ParentTooltip->IsEmpty())
					{
						PickTextFromWidget(ParentTooltip->AsWidget());
					}
				}
			}

			TSharedRef<SVerticalBox> TextsBox = SNew(SVerticalBox);

			// Add a new Translation Picker Edit Widget for each picked text
			for (FText PickedText : PickedTexts)
			{
				TextsBox->AddSlot()
					.AutoHeight()
					.Padding(FMargin(5))
					[
						SNew(SBorder)
						[
							SNew(STranslationPickerEditWidget)
							.PickedText(PickedText)
							.bAllowEditing(false)
						]
					];
			}

			WindowContents->SetContentWidget(
				SNew(SVerticalBox)
				
				+SVerticalBox::Slot()
				.Padding(0)
				.FillHeight(1)
				.Padding(FMargin(5))
				[
					SNew(SScrollBox)
					.Orientation(EOrientation::Orient_Vertical)
					.ScrollBarAlwaysVisible(true)
					
					+SScrollBox::Slot()
					.Padding(FMargin(0))
					[
						TextsBox
					]
				]

				+SVerticalBox::Slot()
				.Padding(0)
				.AutoHeight()
				.Padding(FMargin(5))
				[
					SNew(STextBlock)
					.Text(PickedTexts.Num() > 0 ? LOCTEXT("TranslationPickerEscToEdit", "Press Esc to edit translations") : LOCTEXT("TranslationPickerHoverToViewEditEscToQuit", "Hover over text to view/edit translations, or press Esc to quit"))
					.Justification(ETextJustify::Center)
				]
			);
		}
	}

	if (ParentWindow.IsValid())
	{
		FVector2D WindowSize = ParentWindow.Pin()->GetSizeInScreen();
		FVector2D DesiredPosition = FSlateApplication::Get().GetCursorPos();
		DesiredPosition.X -= FSlateApplication::Get().GetCursorSize().X;
		DesiredPosition.Y += FSlateApplication::Get().GetCursorSize().Y;

		// Move to opposite side of the cursor than the tool tip, so they don't overlaps
		DesiredPosition.X -= WindowSize.X;

		// Clamp to work area
		DesiredPosition = FSlateApplication::Get().CalculateTooltipWindowPosition(FSlateRect(DesiredPosition, DesiredPosition), WindowSize, false);

		// also kind of a hack, but this is the only way at the moment to get a 'cursor decorator' without using the drag-drop code path
		ParentWindow.Pin()->MoveWindowTo(DesiredPosition);
	}

	LastTickHoveringWidgetPath = FWeakWidgetPath(Path);
}

void STranslationPickerFloatingWindow::PickTextFromWidget(TSharedRef<SWidget> Widget)
{
	auto AppendPickedTextImpl = [this](const FText& InPickedText)
	{
		const bool bAlreadyPicked = PickedTexts.ContainsByPredicate([&InPickedText](const FText& InOtherPickedText)
		{
			return InOtherPickedText.IdenticalTo(InPickedText);
		});

		if (!bAlreadyPicked)
		{
			PickedTexts.Add(InPickedText);
		}
	};

	auto AppendPickedText = [this, AppendPickedTextImpl](const FText& InPickedText)
	{
		if (InPickedText.IsEmpty())
		{
			return;
		}

		// Search the text from this widget's FText::Format history to find any source text
		TArray<FHistoricTextFormatData> HistoricFormatData;
		FTextInspector::GetHistoricFormatData(InPickedText, HistoricFormatData);

		if (HistoricFormatData.Num() > 0)
		{
			for (const FHistoricTextFormatData& HistoricFormatDataItem : HistoricFormatData)
			{
				AppendPickedTextImpl(HistoricFormatDataItem.SourceFmt.GetSourceText());

				for (auto It = HistoricFormatDataItem.Arguments.CreateConstIterator(); It; ++It)
				{
					const FFormatArgumentValue& ArgumentValue = It.Value();
					if (ArgumentValue.GetType() == EFormatArgumentType::Text)
					{
						AppendPickedTextImpl(ArgumentValue.GetTextValue());
					}
				}
			}
		}
		else
		{
			AppendPickedTextImpl(InPickedText);
		}
	};

	// Have to parse the various widget types to find the FText
	if (Widget->GetTypeAsString() == "STextBlock")
	{
		STextBlock& TextBlock = (STextBlock&)Widget.Get();
		AppendPickedText(TextBlock.GetText());
	}
	else if (Widget->GetTypeAsString() == "SRichTextBlock")
	{
		SRichTextBlock& RichTextBlock = (SRichTextBlock&)Widget.Get();
		AppendPickedText(RichTextBlock.GetText());
	}
	else if (Widget->GetTypeAsString() == "SToolTip")
	{
		SToolTip& ToolTipWidget = (SToolTip&)Widget.Get();
		AppendPickedText(ToolTipWidget.GetTextTooltip());
	}
	else if (Widget->GetTypeAsString() == "SDocumentationToolTip")
	{
		SDocumentationToolTip& DocumentationToolTip = (SDocumentationToolTip&)Widget.Get();
		AppendPickedText(DocumentationToolTip.GetTextTooltip());
	}
	else if (Widget->GetTypeAsString() == "SEditableText")
	{
		SEditableText& EditableText = (SEditableText&)Widget.Get();
		AppendPickedText(EditableText.GetText());
		AppendPickedText(EditableText.GetHintText());
	}
	else if (Widget->GetTypeAsString() == "SMultiLineEditableText")
	{
		SMultiLineEditableText& MultiLineEditableText = (SMultiLineEditableText&)Widget.Get();
		AppendPickedText(MultiLineEditableText.GetText());
		AppendPickedText(MultiLineEditableText.GetHintText());
	}

	// Recurse into child widgets
	PickTextFromChildWidgets(Widget);
}

void STranslationPickerFloatingWindow::PickTextFromChildWidgets(TSharedRef<SWidget> Widget)
{
	FChildren* Children = Widget->GetChildren();

	for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
	{
		TSharedRef<SWidget> ChildWidget = Children->GetChildAt(ChildIndex);

		// Pull out any FText from this child widget
		PickTextFromWidget(ChildWidget);
	}
}

void STranslationPickerFloatingWindow::OnEscapePressed()
{
	if (PickedTexts.Num() > 0)
	{
		// Open a different window to allow editing of the translation
		TSharedRef<SWindow> NewWindow = SNew(SWindow)
			.Title(LOCTEXT("TranslationPickerEditWindowTitle", "Edit Translations"))
			.CreateTitleBar(true)
			.SizingRule(ESizingRule::UserSized);

		TSharedRef<STranslationPickerEditWindow> EditWindow = SNew(STranslationPickerEditWindow)
			.ParentWindow(NewWindow)
			.PickedTexts(PickedTexts);

		NewWindow->SetContent(EditWindow);

		// Make this roughly the same size as the Edit Window, so when you press Esc to edit, the window is in basically the same size
		NewWindow->Resize(FVector2D(STranslationPickerEditWindow::DefaultEditWindowWidth, STranslationPickerEditWindow::DefaultEditWindowHeight));

		TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
		if (RootWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(NewWindow, RootWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(NewWindow);
		}

		NewWindow->MoveWindowTo(ParentWindow.Pin()->GetPositionInScreen());
	}

	TranslationPickerManager::ClosePickerWindow();
}

#undef LOCTEXT_NAMESPACE
