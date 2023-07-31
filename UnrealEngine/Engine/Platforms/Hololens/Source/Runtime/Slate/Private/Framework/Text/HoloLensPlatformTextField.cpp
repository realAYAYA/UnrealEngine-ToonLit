// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/HoloLensPlatformTextField.h"

using namespace Windows::UI::Text::Core;
using namespace Windows::UI::Core;
using namespace Windows::System;
using namespace Windows::UI::ViewManagement;

ref class VirtualKeyboardInputContext
{
internal:

	VirtualKeyboardInputContext()
	{
	}

	void CreateContext()
	{
		using namespace Windows::Foundation;

		if (coreTextContext != nullptr)
		{
			return;
		}

		coreTextContext = CoreTextServicesManager::GetForCurrentView()->CreateEditContext();
		coreTextContext->InputPaneDisplayPolicy = CoreTextInputPaneDisplayPolicy::Manual;

		SelectionRequestedToken = coreTextContext->SelectionRequested += ref new TypedEventHandler<CoreTextEditContext^, CoreTextSelectionRequestedEventArgs^>(this, &VirtualKeyboardInputContext::OnSelectionRequested);
		SelectionUpdatingToken = coreTextContext->SelectionUpdating += ref new TypedEventHandler<CoreTextEditContext^, CoreTextSelectionUpdatingEventArgs^>(this, &VirtualKeyboardInputContext::OnSelectionUpdating);
		TextRequestedToken = coreTextContext->TextRequested += ref new TypedEventHandler<CoreTextEditContext^, CoreTextTextRequestedEventArgs^>(this, &VirtualKeyboardInputContext::OnTextRequested);
		TextUpdatingToken = coreTextContext->TextUpdating += ref new TypedEventHandler<CoreTextEditContext^, CoreTextTextUpdatingEventArgs^>(this, &VirtualKeyboardInputContext::OnTextUpdating);
		FormatUpdatingToken = coreTextContext->FormatUpdating += ref new TypedEventHandler<CoreTextEditContext^, CoreTextFormatUpdatingEventArgs^>(this, &VirtualKeyboardInputContext::OnFormatUpdating);

		KeyDownCoreWindowToken = CoreWindow::GetForCurrentThread()->KeyDown += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &VirtualKeyboardInputContext::OnKeyDownCoreWindow);

		TextEntryWidget->OnSelectionChanged.BindLambda([this]() { OnSelectionChanged(); });
	}

	void DeleteContext()
	{
		if (TextEntryWidget)
		{
			TextEntryWidget->OnSelectionChanged.Unbind();
		}

		CoreWindow::GetForCurrentThread()->KeyDown -= KeyDownCoreWindowToken;

		if (coreTextContext)
		{
			coreTextContext->SelectionRequested -= SelectionRequestedToken;
			coreTextContext->SelectionUpdating -= SelectionUpdatingToken;
			coreTextContext->TextRequested -= TextRequestedToken;
			coreTextContext->TextUpdating -= TextUpdatingToken;
			coreTextContext->FormatUpdating -= FormatUpdatingToken;
		}

		coreTextContext = nullptr;
	}

	void OnSelectionRequested(CoreTextEditContext^, CoreTextSelectionRequestedEventArgs^ args)
	{
		int start, end;
		if (TextEntryWidget->GetSelection(start, end))
		{
			CoreTextRange range;
			range.StartCaretPosition = start;
			range.EndCaretPosition = end;
			args->Request->Selection = range;
		}
	}

	void OnSelectionUpdating(CoreTextEditContext^, CoreTextSelectionUpdatingEventArgs^ args)
	{
		TextEntryWidget->SetSelectionFromVirtualKeyboard(args->Selection.StartCaretPosition, args->Selection.EndCaretPosition);
	}

	void OnTextRequested(CoreTextEditContext^, CoreTextTextRequestedEventArgs^ args)
	{
		args->Request->Text = ref new Platform::String(*TextEntryWidget->GetText().ToString());
	}

	void OnTextUpdating(CoreTextEditContext^, CoreTextTextUpdatingEventArgs^ args)
	{
		FString text = TextEntryWidget->GetText().ToString();
		uint32 start = args->Range.StartCaretPosition;
		uint32 end = args->Range.EndCaretPosition;
		FString newText = args->Text->Data();

		if (start != end)
		{
			text.RemoveAt(start, end - start);
		}
		text.InsertAt(start, newText);

		TextEntryWidget->SetTextFromVirtualKeyboard(FText::FromString(text), ETextEntryType::TextEntryUpdated);
		TextEntryWidget->SetSelectionFromVirtualKeyboard(args->NewSelection.StartCaretPosition, args->NewSelection.EndCaretPosition);
	}

	// Needed for autocomplete
	void OnFormatUpdating(CoreTextEditContext^, CoreTextFormatUpdatingEventArgs^ args)
	{
	}

	void OnSelectionChanged()
	{
		int start, end;
		if (TextEntryWidget->GetSelection(start, end))
		{
			CoreTextRange selRange;
			selRange.StartCaretPosition = start;
			selRange.EndCaretPosition = end;
			coreTextContext->NotifySelectionChanged(selRange);
		}
	}

	void Start(TSharedPtr<IVirtualKeyboardEntry> textEntryWidget)
	{
		TextEntryWidget = textEntryWidget;
		CreateContext();
		coreTextContext->NotifyFocusEnter();
		InputPane::GetForCurrentView()->TryShow();
	}

	void Stop()
	{
		if (InputPane::GetForCurrentView() != nullptr)
		{
			InputPane::GetForCurrentView()->TryHide();
		}
		
		if (coreTextContext)
		{
			coreTextContext->NotifyFocusLeave();
		}

		DeleteContext();
		TextEntryWidget = nullptr;
	}

	void OnBack()
	{
		FString text = TextEntryWidget->GetText().ToString();
		int oldLen = text.Len();
		if (oldLen == 0)
		{
			return;
		}

		int start, end;
		if (!TextEntryWidget->GetSelection(start, end))
		{
			start = end = text.Len();
		}

		if (start == end)
		{
			//no selection
			if (start == 0)
			{
				return;
			}

			start--;
		}

		text.RemoveAt(start, end - start);

		CoreTextRange txtRange, selRange;
		txtRange.StartCaretPosition = start;
		txtRange.EndCaretPosition = end;
		selRange.StartCaretPosition = start;
		selRange.EndCaretPosition = start;

		TextEntryWidget->SetTextFromVirtualKeyboard(FText::FromString(text), ETextEntryType::TextEntryUpdated);
		TextEntryWidget->SetSelectionFromVirtualKeyboard(start, start);
		coreTextContext->NotifyTextChanged(txtRange, oldLen - (end - start), selRange);
	}

	void OnLeft()
	{
		FString text = TextEntryWidget->GetText().ToString();
		int oldLen = text.Len();
		if (oldLen == 0)
		{
			return;
		}

		int start, end;
		if (!TextEntryWidget->GetSelection(start, end))
		{
			start = end = text.Len();
		}

		if (start == end)
		{
			if (start == 0)
			{
				return;
			}

			--start;
		}
		end = start;

		CoreTextRange selRange;
		selRange.StartCaretPosition = start;
		selRange.EndCaretPosition = end;
		coreTextContext->NotifySelectionChanged(selRange);
		TextEntryWidget->SetSelectionFromVirtualKeyboard(start, end);
	}

	void OnRight()
	{
		FString text = TextEntryWidget->GetText().ToString();
		int oldLen = text.Len();
		if (oldLen == 0)
		{
			return;
		}

		int start, end;
		if (!TextEntryWidget->GetSelection(start, end))
		{
			start = end = text.Len();
		}

		if (start == end)
		{
			if (end == oldLen)
			{
				return;
			}

			++end;
		}
		start = end;

		CoreTextRange selRange;
		selRange.StartCaretPosition = start;
		selRange.EndCaretPosition = end;
		coreTextContext->NotifySelectionChanged(selRange);
		TextEntryWidget->SetSelectionFromVirtualKeyboard(start, end);

	}

	void OnKeyDownCoreWindow(CoreWindow^ sender, KeyEventArgs^ args)
	{
		switch (args->VirtualKey)
		{
		case VirtualKey::Back:
			OnBack();
			break;
		case VirtualKey::Left:
			OnLeft();
			break;
		case VirtualKey::Right:
			OnRight();
			break;
		}
	}


private:
	Windows::Foundation::EventRegistrationToken SelectionRequestedToken;
	Windows::Foundation::EventRegistrationToken SelectionUpdatingToken;
	Windows::Foundation::EventRegistrationToken TextRequestedToken;
	Windows::Foundation::EventRegistrationToken TextUpdatingToken;
	Windows::Foundation::EventRegistrationToken FormatUpdatingToken;
	Windows::Foundation::EventRegistrationToken KeyDownCoreWindowToken;

	Windows::UI::Text::Core::CoreTextEditContext^ coreTextContext = nullptr;
	TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget;
};

FHoloLensPlatformTextField::FHoloLensPlatformTextField()
{
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddRaw(this, &FHoloLensPlatformTextField::LevelChanging);

	inputContext = ref new VirtualKeyboardInputContext();
}

FHoloLensPlatformTextField::~FHoloLensPlatformTextField()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
}

void FHoloLensPlatformTextField::LevelChanging(const FString& MapName)
{
	if (inputContext)
	{
		inputContext->Stop();
	}
}

void FHoloLensPlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) 
{
	if (bShow)
	{
		inputContext->Start(TextEntryWidget);
	}
	else
	{
		inputContext->Stop();
	}
}
