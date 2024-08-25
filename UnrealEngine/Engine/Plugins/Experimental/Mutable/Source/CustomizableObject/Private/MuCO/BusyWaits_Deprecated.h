// Copyright Epic Games, Inc. All Rights Reserved.

// ---------------------------------------------------------------
// THIS FILE SHOULD ONLY CONTAIN CODE BEING DEPRECATED BY UE-207754
// ---------------------------------------------------------------

#pragma once

#include "Templates/SharedPointer.h"

class FUpdateContextPrivate;
struct FMutableImageOperationData;
struct FMutableMeshOperationData;

namespace mu
{
	class Model;
}


namespace CustomizableObjectSystem::ImplDeprecated
{
	// This runs in the mutable thread.
	void Subtask_Mutable_GetImages(const TSharedRef<FUpdateContextPrivate>& OperationData);

	// This runs in a worker thread.
	void Task_Mutable_GetImages(const TSharedRef<FUpdateContextPrivate>& OperationData);

	// This runs in the mutable thread.
	void Subtask_Mutable_BeginUpdate_GetMesh(const TSharedRef<FUpdateContextPrivate>& OperationData, TSharedPtr<mu::Model> Model);

	// This runs in a worker thread.
	void Task_Mutable_GetMeshes(const TSharedRef<FUpdateContextPrivate>& OperationData);
}


namespace CustomizableObjectMipDataProvider::ImplDeprecated
{
	// This runs in the mutable thread.
	void Task_Mutable_UpdateImage(TSharedPtr<FMutableImageOperationData> OperationData);
}


namespace CustomizableObjectMeshUpdate::ImplDeprecated
{
	// This runs in the mutable thread.
	void Task_Mutable_UpdateMesh(const TSharedPtr<FMutableMeshOperationData>& OperationData);
}