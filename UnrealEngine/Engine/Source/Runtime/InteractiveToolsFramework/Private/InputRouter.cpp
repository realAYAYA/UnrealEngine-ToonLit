// Copyright Epic Games, Inc. All Rights Reserved.


#include "InputRouter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputRouter)


#define LOCTEXT_NAMESPACE "UInputRouter"

UInputRouter::UInputRouter()
{
	ActiveLeftCapture = nullptr;
	ActiveLeftCaptureOwner = nullptr;
	ActiveRightCapture = nullptr;
	ActiveRightCaptureOwner = nullptr;
	ActiveKeyboardCapture = nullptr;
	ActiveKeyboardCaptureOwner = nullptr;

	bAutoInvalidateOnHover = false;
	bAutoInvalidateOnCapture = false;

	ActiveInputBehaviors = NewObject<UInputBehaviorSet>(this, "InputBehaviors");
}



void UInputRouter::Initialize(IToolsContextTransactionsAPI* TransactionsAPIIn)
{
	this->TransactionsAPI = TransactionsAPIIn;
}

void UInputRouter::Shutdown()
{
	this->TransactionsAPI = nullptr;
}




void UInputRouter::RegisterSource(IInputBehaviorSource* Source)
{
	ActiveInputBehaviors->Add(Source->GetInputBehaviors(), Source);
}


void UInputRouter::DeregisterSource(IInputBehaviorSource* Source)
{
	ActiveInputBehaviors->RemoveBySource(Source);
}


bool UInputRouter::PostInputEvent(const FInputDeviceState& Input)
{
	if (ActiveInputBehaviors->IsEmpty())
	{
		return false;
	}

	if (Input.IsFromDevice(EInputDevices::Mouse))
	{
		PostInputEvent_Mouse(Input);
		return HasActiveMouseCapture();
	}
	else if (Input.IsFromDevice(EInputDevices::Keyboard))
	{
		PostInputEvent_Keyboard(Input);
		return (ActiveKeyboardCapture != nullptr);
	}
	else
	{
		unimplemented();
		TransactionsAPI->DisplayMessage(LOCTEXT("PostInputEventMessage", "UInteractiveToolManager::PostInputEvent - input device is not currently supported."), EToolMessageLevel::Internal);
		return false;
	}
}



//
// Keyboard event handling
// 


void UInputRouter::PostInputEvent_Keyboard(const FInputDeviceState& Input)
{
	if (ActiveKeyboardCapture != nullptr)
	{
		HandleCapturedKeyboardInput(Input);
	}
	else
	{
		ActiveKeyboardCaptureData = FInputCaptureData();
		CheckForKeyboardCaptures(Input);
	}
}


void UInputRouter::CheckForKeyboardCaptures(const FInputDeviceState& Input)
{
	TArray<FInputCaptureRequest> CaptureRequests;
	ActiveInputBehaviors->CollectWantsCapture(Input, CaptureRequests);
	if (CaptureRequests.Num() == 0)
	{
		return;
	}

	CaptureRequests.StableSort();

	bool bAccepted = false;
	for (int i = 0; i < CaptureRequests.Num() && bAccepted == false; ++i)
	{
		FInputCaptureUpdate Result =
			CaptureRequests[i].Source->BeginCapture(Input, EInputCaptureSide::Left);
		if (Result.State == EInputCaptureState::Begin)
		{
			ActiveKeyboardCapture = Result.Source;
			ActiveKeyboardCaptureOwner = CaptureRequests[i].Owner;
			ActiveKeyboardCaptureData = Result.Data;
			bAccepted = true;
		}
	}
}


void UInputRouter::HandleCapturedKeyboardInput(const FInputDeviceState& Input)
{
	if (ActiveKeyboardCapture == nullptr)
	{
		return;
	}

	FInputCaptureUpdate Result =
		ActiveKeyboardCapture->UpdateCapture(Input, ActiveKeyboardCaptureData);

	if (Result.State == EInputCaptureState::End)
	{
		ActiveKeyboardCapture = nullptr;
		ActiveKeyboardCaptureOwner = nullptr;
		ActiveKeyboardCaptureData = FInputCaptureData();
	}
	else if (Result.State != EInputCaptureState::Continue)
	{
		TransactionsAPI->DisplayMessage(LOCTEXT("HandleCapturedKeyboardInputMessage", "UInteractiveToolManager::HandleCapturedKeyboardInput - unexpected capture state!"), EToolMessageLevel::Internal);
	}

	if (bAutoInvalidateOnCapture)
	{
		TransactionsAPI->PostInvalidation();
	}
}



//
// Mouse event handling
//



void UInputRouter::PostInputEvent_Mouse(const FInputDeviceState& Input)
{
	if (ActiveLeftCapture != nullptr)
	{
		HandleCapturedMouseInput(Input);
	}
	else
	{
		ActiveLeftCaptureData = FInputCaptureData();
		CheckForMouseCaptures(Input);
	}

	// update hover if nobody is capturing
	if (ActiveLeftCapture == nullptr)
	{
		bool bHoverStateUpdated = ProcessMouseHover(Input);
		if (bHoverStateUpdated && bAutoInvalidateOnHover)
		{
			TransactionsAPI->PostInvalidation();
		}
	}

}


void UInputRouter::PostHoverInputEvent(const FInputDeviceState& Input)
{
	bool bHoverStateUpdated = ProcessMouseHover(Input);
	if (bHoverStateUpdated && bAutoInvalidateOnHover)
	{
		TransactionsAPI->PostInvalidation();
	}
}




bool UInputRouter::HasActiveMouseCapture() const
{
	return (ActiveLeftCapture != nullptr);
}


void UInputRouter::CheckForMouseCaptures(const FInputDeviceState& Input)
{
	TArray<FInputCaptureRequest> CaptureRequests;
	ActiveInputBehaviors->CollectWantsCapture(Input, CaptureRequests);
	if (CaptureRequests.Num() == 0)
	{
		return;
	}

	CaptureRequests.StableSort();

	bool bAccepted = false;
	for (int i = 0; i < CaptureRequests.Num() && bAccepted == false; ++i)
	{
		FInputCaptureUpdate Result =
			CaptureRequests[i].Source->BeginCapture(Input, EInputCaptureSide::Left);
		if (Result.State == EInputCaptureState::Begin)
		{
			// end outstanding hover
			TerminateHover(EInputCaptureSide::Left);

			ActiveLeftCapture = Result.Source;
			ActiveLeftCaptureOwner = CaptureRequests[i].Owner;
			ActiveLeftCaptureData = Result.Data;
			bAccepted = true;
		}

	}
}


void UInputRouter::HandleCapturedMouseInput(const FInputDeviceState& Input)
{
	if (ActiveLeftCapture == nullptr)
	{
		return;
	}

	// have active capture - give it this event

	FInputCaptureUpdate Result =
		ActiveLeftCapture->UpdateCapture(Input, ActiveLeftCaptureData);

	if (Result.State == EInputCaptureState::End)
	{
		ActiveLeftCapture = nullptr;
		ActiveLeftCaptureOwner = nullptr;
		ActiveLeftCaptureData = FInputCaptureData();
	}
	else if (Result.State != EInputCaptureState::Continue)
	{
		TransactionsAPI->DisplayMessage(LOCTEXT("HandleCapturedMouseInputMessage", "UInteractiveToolManager::HandleCapturedMouseInput - unexpected capture state!"), EToolMessageLevel::Internal);
	}

	if (bAutoInvalidateOnCapture)
	{
		TransactionsAPI->PostInvalidation();
	}
}




void UInputRouter::TerminateHover(EInputCaptureSide Side)
{
	if (Side == EInputCaptureSide::Left && ActiveLeftHoverCapture != nullptr)
	{
		ActiveLeftHoverCapture->EndHoverCapture();
		ActiveLeftHoverCapture = nullptr;
		ActiveLeftCaptureOwner = nullptr;
	}
}


bool UInputRouter::ProcessMouseHover(const FInputDeviceState& Input)
{
	TArray<FInputCaptureRequest> CaptureRequests;
	ActiveInputBehaviors->CollectWantsHoverCapture(Input, CaptureRequests);

	if (CaptureRequests.Num() == 0 )
	{
		if (ActiveLeftHoverCapture != nullptr)
		{
			TerminateHover(EInputCaptureSide::Left);
			return true;
		}
		return false;
	}

	CaptureRequests.StableSort();

	// if we have an active hover, either update it, or terminate if we got a new best hit
	bool bHoverStateModified = false;
	if (ActiveLeftHoverCapture != nullptr)
	{
		bool bTerminateActiveHover = false;
		if (CaptureRequests[0].Source == ActiveLeftHoverCapture)
		{
			FInputCaptureUpdate Result =
				ActiveLeftHoverCapture->UpdateHoverCapture(Input);
			bTerminateActiveHover = (Result.State == EInputCaptureState::End);
		}
		else
		{
			bTerminateActiveHover = true;
		}

		if (bTerminateActiveHover)
		{
			TerminateHover(EInputCaptureSide::Left);
			bHoverStateModified = true;
		}
		else
		{
			return true;		// hover has been consumed
		}
	}

	// if we get here, we have a new hover
	bool bAccepted = false;
	for (int i = 0; i < CaptureRequests.Num() && bAccepted == false; ++i)
	{
		FInputCaptureUpdate Result =
			CaptureRequests[i].Source->BeginHoverCapture(Input, EInputCaptureSide::Left);
		if (Result.State == EInputCaptureState::Begin)
		{
			ActiveLeftHoverCapture = Result.Source;
			ActiveLeftHoverCaptureOwner = CaptureRequests[i].Owner;
			bAccepted = true;
			return true;
		}
	}
	
	// no hover! but we might have terminated an active hover
	return bHoverStateModified;
}





void UInputRouter::ForceTerminateAll()
{
	if (ActiveKeyboardCapture != nullptr)
	{
		ActiveKeyboardCapture->ForceEndCapture(ActiveKeyboardCaptureData);
		ActiveKeyboardCapture = nullptr;
		ActiveKeyboardCaptureOwner = nullptr;
		ActiveKeyboardCaptureData = FInputCaptureData();
	}

	if (ActiveLeftCapture != nullptr)
	{
		ActiveLeftCapture->ForceEndCapture(ActiveLeftCaptureData);
		ActiveLeftCapture = nullptr;
		ActiveLeftCaptureOwner = nullptr;
		ActiveLeftCaptureData = FInputCaptureData();
	}

	if (ActiveRightCapture != nullptr)
	{
		ActiveRightCapture->ForceEndCapture(ActiveRightCaptureData);
		ActiveRightCapture = nullptr;
		ActiveRightCaptureOwner = nullptr;
		ActiveRightCaptureData = FInputCaptureData();
	}

	if (ActiveLeftHoverCapture != nullptr)
	{
		TerminateHover(EInputCaptureSide::Left);
	}
}


void UInputRouter::ForceTerminateSource(IInputBehaviorSource* Source)
{
	if (ActiveKeyboardCapture != nullptr && ActiveKeyboardCaptureOwner == Source)
	{
		ActiveKeyboardCapture->ForceEndCapture(ActiveKeyboardCaptureData);
		ActiveKeyboardCapture = nullptr;
		ActiveKeyboardCaptureOwner = nullptr;
		ActiveKeyboardCaptureData = FInputCaptureData();
	}

	if (ActiveLeftCapture != nullptr && ActiveLeftCaptureOwner == Source)
	{
		ActiveLeftCapture->ForceEndCapture(ActiveLeftCaptureData);
		ActiveLeftCapture = nullptr;
		ActiveLeftCaptureOwner = nullptr;
		ActiveLeftCaptureData = FInputCaptureData();
	}

	if (ActiveRightCapture != nullptr && ActiveRightCaptureOwner == Source)
	{
		ActiveRightCapture->ForceEndCapture(ActiveRightCaptureData);
		ActiveRightCapture = nullptr;
		ActiveRightCaptureOwner = nullptr;
		ActiveRightCaptureData = FInputCaptureData();
	}

	if (ActiveLeftHoverCapture != nullptr && ActiveLeftHoverCaptureOwner == Source)
	{
		TerminateHover(EInputCaptureSide::Left);
	}
}



#undef LOCTEXT_NAMESPACE

