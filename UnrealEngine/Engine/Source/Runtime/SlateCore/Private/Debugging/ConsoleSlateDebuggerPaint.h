// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Debugging/SlateDebugging.h"

#if WITH_SLATE_DEBUGGING

#include "CoreMinimal.h"
#include "Debugging/ConsoleSlateDebuggerUtility.h"
#include "Delegates/Delegate.h"
#include "Input/Reply.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/DrawElements.h"

struct FGeometry;
class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class SWidget;
class SWindow;

/**
 * Allows debugging the behavior of SWidget::Paint from the console.
 * Basics:
 *   Start - SlateDebugger.Paint.Start
 *   Stop  - SlateDebugger.Paint.Stop
 */
class FConsoleSlateDebuggerPaint
{
public:
	FConsoleSlateDebuggerPaint();
	virtual ~FConsoleSlateDebuggerPaint();

	void StartDebugging();
	void StopDebugging();
	bool IsEnabled() const { return bEnabled; }

	void SaveConfig();

private:
	void HandleEnabled(IConsoleVariable* Variable);
	void HandleLogOnce();
	void HandleToggleWidgetNameList();
	void HandleEndFrame();
	void HandleEndWidgetPaint(const SWidget* Widget, const FSlateWindowElementList& OutDrawElements, int32 LayerId);
	void HandlePaintDebugInfo(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry, FSlateWindowElementList& InOutDrawElements, int32& InOutLayerId);

private:
	bool bEnabled;
	bool bEnabledCVarValue;

	//~ Settings
	bool bDisplayWidgetsNameList;
	bool bUseWidgetPathAsName;
	bool bDrawBox;
	bool bDrawQuad;
	bool bLogWidgetName;
	bool bLogWidgetNameOnce;
	bool bLogWarningIfWidgetIsPaintedMoreThanOnce;
	bool bDebugGameWindowOnly;
	FLinearColor DrawBoxColor;
	FLinearColor DrawQuadColor;
	FLinearColor DrawWidgetNameColor;
	int32 MaxNumberOfWidgetInList;
	float CacheDuration;
	FName PIEWindowTag;

	//~ Console objects
	FAutoConsoleCommand ShowPaintWidgetCommand;
	FAutoConsoleCommand HidePaintWidgetCommand;
	FAutoConsoleVariableRef EnabledRefCVar;
	FAutoConsoleCommand LogPaintedWidgetOnceCommand;
	FAutoConsoleCommand DisplayWidgetsNameListCommand;
	FAutoConsoleVariableRef MaxNumberOfWidgetInListtRefCVar;
	FAutoConsoleVariableRef LogWarningIfWidgetIsPaintedMoreThanOnceRefCVar;
	FAutoConsoleVariableRef OnlyGameWindow;

	struct FPaintInfo 
	{
		FConsoleSlateDebuggerUtility::TSWindowId Window;
		FVector2f PaintLocation;
		FVector2f PaintSize;
		FString WidgetName;
		double LastPaint;
		int32 PaintCount;
	};

	using TPaintedWidgetMap = TMap<FConsoleSlateDebuggerUtility::TSWidgetId, FPaintInfo>;
	TPaintedWidgetMap PaintedWidgets;
};

#endif //WITH_SLATE_DEBUGGING