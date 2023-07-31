// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

namespace UE::MLDeformer
{
	/**
	 * The editor style class that describes specific UI style related settings for the ML Deformer editor.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerEditorStyle
		: public FSlateStyleSet
	{
	public:
		FMLDeformerEditorStyle();
		~FMLDeformerEditorStyle();

		static FMLDeformerEditorStyle& Get();
	};
}	// namespace UE::MLDeformer
