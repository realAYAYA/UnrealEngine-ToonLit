// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorGizmos/EditorGizmoStateTarget.h"
#include "EditorGizmos/TransformGizmo.h"
#include "EditorModeManager.h"
#include "GizmoEdModeInterface.h"

void UEditorGizmoStateTarget::BeginUpdate()
{
	const TSharedPtr<FEditorModeTools> ModeTools = WeakModeTools.IsValid() ? WeakModeTools.Pin() : nullptr;
	if (ensure(ModeTools.IsValid()))
	{
		if (TransactionManager)
		{
			TransactionManager->BeginUndoTransaction(TransactionDescription);
		}

		// empty state for now
		constexpr FGizmoState State;
		(void)ModeTools->BeginTransform(State);
	}
}

void UEditorGizmoStateTarget::EndUpdate()
{
	const TSharedPtr<FEditorModeTools> ModeTools = WeakModeTools.IsValid() ? WeakModeTools.Pin() : nullptr;
	if (ensure(ModeTools))
	{
		// empty state for now
		constexpr FGizmoState State;
		(void)ModeTools->EndTransform(State);

		if (TransactionManager)
		{
			TransactionManager->EndUndoTransaction();
		}
	}
}

UEditorGizmoStateTarget* UEditorGizmoStateTarget::Construct(
	FEditorModeTools* InModeManager,
	const FText& InDescription,
	IToolContextTransactionProvider* TransactionManagerIn,
	UObject* Outer)
{
	UEditorGizmoStateTarget* NewTarget = NewObject<UEditorGizmoStateTarget>(Outer);
	NewTarget->WeakModeTools = InModeManager->AsShared();
	NewTarget->TransactionDescription = InDescription;

	// have to explicitly configure this because we only have IToolContextTransactionProvider pointer
	NewTarget->TransactionManager.SetInterface(TransactionManagerIn);
	NewTarget->TransactionManager.SetObject(CastChecked<UObject>(TransactionManagerIn));
		
	return NewTarget;
}

void UEditorGizmoStateTarget::SetTransformGizmo(UTransformGizmo* InGizmo)
{
	TransformGizmo = InGizmo;
}