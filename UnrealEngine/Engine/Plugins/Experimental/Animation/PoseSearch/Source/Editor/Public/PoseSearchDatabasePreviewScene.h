// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"

namespace UE::PoseSearch
{
	class FDatabaseEditor;

	class FDatabasePreviewScene : public FAdvancedPreviewScene
	{
	public:

		FDatabasePreviewScene(
			ConstructionValues CVs,
			const TSharedRef<FDatabaseEditor>& Editor);
		~FDatabasePreviewScene() {}

		virtual void Tick(float InDeltaTime) override;

		TSharedRef<FDatabaseEditor> GetEditor() const
		{
			return EditorPtr.Pin().ToSharedRef();
		}

	private:

		/** The asset editor we are embedded in */
		TWeakPtr<FDatabaseEditor> EditorPtr;
	};
}

