// Copyright Epic Games, Inc. All Rights Reserved.

#if NV_GEFORCENOW

#include "GeForceNOWActionZoneProcessor.h"
#include "Widgets/Accessibility/SlateWidgetTracker.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Ticker.h"
#include "GeForceNOWWrapper.h"
#include "Widgets/SWindow.h"
#include "Widgets/SWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGFNActionZoneProcessor, Display, All);

static bool bForceProcessGFNWidgetActionZones = false;
FAutoConsoleVariableRef CVarForceProcessGFNWidgetActionZones(
	TEXT("GFN.ForceProcessGFNWidgetActionZones"),
	bForceProcessGFNWidgetActionZones,
	TEXT("Force the processing of GFN Actions Zones even if we aren't running in GFN"));

static float GFNWidgetActionZonesProcessDelay = 0.1f;
FAutoConsoleVariableRef CVarGFNWidgetActionZonesProcessDelay(
	TEXT("GFN.WidgetActionZonesProcessDelay"),
	GFNWidgetActionZonesProcessDelay,
	TEXT("Intervals in seconds between each processing of the GFN Action Zones"));

namespace GeForceNowTrackedWidgetTags
{
	FName EditableTextTag = TEXT("EditableText");
}

//---------------------------GFNWidgetActionZone---------------------------

FWidgetGFNActionZone::FWidgetGFNActionZone(const SWidget* InWidget) : Widget(InWidget) {}

void FWidgetGFNActionZone::UpdateActionZone(TArray<TSharedRef<SWindow>>& SlateWindows)
{
	const FVector2D AbsolutePosition = Widget->GetTickSpaceGeometry().GetAbsolutePosition();
	const FVector2D AbsoluteSize = Widget->GetTickSpaceGeometry().GetAbsoluteSize();
	const FVector2D AbsoluteMiddle = FVector2D(AbsolutePosition.X + (AbsoluteSize.X * 0.5f), AbsolutePosition.Y + (AbsoluteSize.Y * 0.5f));

	const FSlateRect AbsolutePosRect = (FSlateRect(AbsolutePosition.X, AbsolutePosition.Y, AbsolutePosition.X + AbsoluteSize.X, AbsolutePosition.Y + AbsoluteSize.Y));
	const bool bRectChanged = ActionZoneRect != AbsolutePosRect;
	ActionZoneRect = AbsolutePosRect;

	FWidgetPath WidgetPath = FSlateApplication::Get().LocateWindowUnderMouse(AbsoluteMiddle, SlateWindows);

	const bool bIsInteractable = WidgetPath.IsValid() && &WidgetPath.GetLastWidget().Get() == Widget;

	if (bIsInteractable)
	{
		if (bRectChanged || !bWasInteractable)
		{
			if (ActionZoneRect.IsValid() && !ActionZoneRect.IsEmpty())
			{
				//Our Widget is interactable; let GFN know.
				UE_LOG(LogGFNActionZoneProcessor, Display, TEXT("[GFNWidgetActionZone::UpdateActionZone] Updating Widget %p GFN Action Zone | ActionZoneRect : L: %f , T: %f , R: %f , B: %f"), Widget, ActionZoneRect.Left, ActionZoneRect.Top, ActionZoneRect.Right, ActionZoneRect.Bottom);

				bWasInteractable = true;
				GfnRect ActionZoneGFNRect;
				ActionZoneGFNRect.value1 = ActionZoneRect.Left;
				ActionZoneGFNRect.value2 = ActionZoneRect.Top;
				ActionZoneGFNRect.value3 = ActionZoneRect.Right;
				ActionZoneGFNRect.value4 = ActionZoneRect.Bottom;
				ActionZoneGFNRect.format = gfnRectLTRB;
				ActionZoneGFNRect.normalized = false;
			
				bHasActionZone = true;
				GfnRuntimeError GfnResult = GeForceNOWWrapper::Get().SetActionZone(gfnEditBox, GetID(), &ActionZoneGFNRect);
				if (GfnResult != GfnError::gfnSuccess)
				{
					bHasActionZone = false;
					UE_LOG(LogGFNActionZoneProcessor, Warning, TEXT("[GFNWidgetActionZone::UpdateActionZone] Failed to set Action Zone.  | Error Code : %i"), GfnResult);
				}
			}
			else
			{
				UE_LOG(LogGFNActionZoneProcessor, Display, TEXT("[GFNWidgetActionZone::UpdateActionZone] Updating Widget %p GFN Action Zone | No longer interactable"), Widget);
				//Our Widget has an invalid Rect and is no longer interactable; let GFN know.
				bWasInteractable = false;
				ClearActionZone();
			}
		}
	}
	else if (bWasInteractable)
	{
		UE_LOG(LogGFNActionZoneProcessor, Display, TEXT("[GFNWidgetActionZone::UpdateActionZone] Updating Widget %p GFN Action Zone | No longer interactable"), Widget);
		//Our Widget was interactable but no longer is; let GFN know.
		bWasInteractable = false;
		ClearActionZone();
	}
}

void FWidgetGFNActionZone::ClearActionZone()
{
	if (bHasActionZone)
	{
		bHasActionZone = false;
		GfnRuntimeError GfnResult = GeForceNOWWrapper::Get().SetActionZone(gfnEditBox, GetID(), nullptr);
		if (GfnResult != GfnError::gfnSuccess)
		{
			bHasActionZone = true;
			UE_LOG(LogGFNActionZoneProcessor, Warning, TEXT("[GFNWidgetActionZone::ClearActionZone] Failed to Remove Action Zone. | Error Code : %i"), GfnResult);
		}
	}
}

unsigned int FWidgetGFNActionZone::GetID() const
{
	//Transform the address of our widget into an Id.
	return static_cast<unsigned int>(reinterpret_cast<uintptr_t>(Widget));
}

//--------------------------GeForceNOWActionZoneProcessor--------------------------

bool GeForceNOWActionZoneProcessor::Initialize()
{
	if (FSlateWidgetTracker::Get().IsEnabled())
	{
		if (GeForceNOWWrapper::Get().IsRunningInGFN() || bForceProcessGFNWidgetActionZones)
		{
			FSlateWidgetTracker::Get().OnTrackedWidgetsChanged(GeForceNowTrackedWidgetTags::EditableTextTag).AddSP(this, &GeForceNOWActionZoneProcessor::HandleTrackedWidgetChanges);

			FSlateWidgetTracker::Get().ForEachTrackedWidget(GeForceNowTrackedWidgetTags::EditableTextTag, [this](const SWidget* Widget)
															{
																HandleEditableTextWidgetRegistered(Widget);
															});
			return true;
		}
	}
	else
	{
		UE_LOG(LogGFNActionZoneProcessor, Warning, TEXT("SlateWidgetTracker is not initialized. GeForceNOWActionZoneProcessor will not function."));
	}
	return false;
}

void GeForceNOWActionZoneProcessor::Terminate()
{
	if (FSlateWidgetTracker::Get().IsEnabled())
	{
		if (GeForceNOWWrapper::Get().IsRunningInGFN() || bForceProcessGFNWidgetActionZones)
		{
			FSlateWidgetTracker::Get().OnTrackedWidgetsChanged(GeForceNowTrackedWidgetTags::EditableTextTag).RemoveAll(this);
			StopProcess();

			for (int32 i = GFNWidgetActionZones.Num() - 1; i >= 0; i--)
			{
				GFNWidgetActionZones[i].ClearActionZone();
				GFNWidgetActionZones.RemoveAt(i);
			}
		}
	}
}

void GeForceNOWActionZoneProcessor::HandleTrackedWidgetChanges(const SWidget* Widget, const FName& Tag, ETrackedSlateWidgetOperations Operation)
{
	switch (Operation)
	{
		case ETrackedSlateWidgetOperations::AddedTrackedWidget :
			HandleEditableTextWidgetRegistered(Widget);
			break;
		case ETrackedSlateWidgetOperations::RemovedTrackedWidget :
			HandleEditableTextWidgetUnregistered(Widget);
			break;
	}
}

void GeForceNOWActionZoneProcessor::HandleEditableTextWidgetRegistered(const SWidget* Widget)
{
	if (GeForceNOWWrapper::Get().IsRunningInGFN() || bForceProcessGFNWidgetActionZones)
	{
		if (GFNWidgetActionZones.Num() == 0)
		{
			StartProcess();
		}
		GFNWidgetActionZones.Add(Widget);
	}
}

void GeForceNOWActionZoneProcessor::HandleEditableTextWidgetUnregistered(const SWidget* Widget)
{
	if (GeForceNOWWrapper::Get().IsRunningInGFN() || bForceProcessGFNWidgetActionZones)
	{
		if (FWidgetGFNActionZone* GFNWidgetActionZone = GFNWidgetActionZones.FindByKey(Widget))
		{
			GFNWidgetActionZone->ClearActionZone();
			int32 Index = GFNWidgetActionZones.Find(*GFNWidgetActionZone);
			if (Index != INDEX_NONE)
			{
				GFNWidgetActionZones.RemoveAt(Index);
			}
		}

		if (GFNWidgetActionZones.Num() == 0)
		{
			StopProcess();
		}
	}
}

bool GeForceNOWActionZoneProcessor::ProcessGFNWidgetActionZones(float DeltaTime)
{
	TArray<TSharedRef<SWindow>> SlateWindows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(SlateWindows);

	for (FWidgetGFNActionZone& GFNWidgetActionZone : GFNWidgetActionZones)
	{
		GFNWidgetActionZone.UpdateActionZone(SlateWindows);
	}
	return true;
}

void GeForceNOWActionZoneProcessor::StartProcess()
{
	if (!ProcessDelegateHandle.IsValid())
	{
		ProcessDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &GeForceNOWActionZoneProcessor::ProcessGFNWidgetActionZones), GFNWidgetActionZonesProcessDelay);
	}
}

void GeForceNOWActionZoneProcessor::StopProcess()
{
	FTSTicker::GetCoreTicker().RemoveTicker(ProcessDelegateHandle);
	ProcessDelegateHandle.Reset();
}

#endif // NV_GEFORCENOW