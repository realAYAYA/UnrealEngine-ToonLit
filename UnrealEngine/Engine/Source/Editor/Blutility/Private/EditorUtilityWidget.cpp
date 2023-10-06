// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityWidget.h"

#include "Internationalization/Internationalization.h"
#include "Misc/AssertionMacros.h"
#include "ScopedTransaction.h"
#include "UObject/Script.h"

/////////////////////////////////////////////////////
#define LOCTEXT_NAMESPACE "EditorUtility"

void UEditorUtilityWidget::ExecuteDefaultAction()
{
	check(bAutoRunDefaultAction);

	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "BlutilityAction", "Blutility Action"));
	FEditorScriptExecutionGuard ScriptGuard;

	Run();
}



#undef LOCTEXT_NAMESPACE