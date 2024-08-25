// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

namespace UE::Chaos::ClothAsset
{
	/**
	 * Editor style setting up the cloth asset icons in editor.
	 */
	class CHAOSCLOTHASSETTOOLS_API FClothAssetEditorStyle final : public FSlateStyleSet
	{
	public:
		FClothAssetEditorStyle();
		virtual ~FClothAssetEditorStyle() override;

	public:
		static FClothAssetEditorStyle& Get()
		{
			if (!Singleton.IsSet())
			{
				Singleton.Emplace();
			}
			return Singleton.GetValue();
		}

		static void Destroy()
		{
			Singleton.Reset();
		}

	private:
		static TOptional<FClothAssetEditorStyle> Singleton;
	};
}  // End namespace UE::Chaos::ClothAsset
