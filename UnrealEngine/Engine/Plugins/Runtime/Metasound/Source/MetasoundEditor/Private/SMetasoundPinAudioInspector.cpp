// Copyright Epic Games, Inc. All Rights Reserved.
#include "SMetasoundPinAudioInspector.h"

#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "MetasoundEditor"

namespace Metasound
{
	namespace Editor
	{
		void SMetasoundPinAudioInspector::Construct(const FArguments& InArgs)
		{
			SPinValueInspector::FArguments InspectorArgs;
			SPinValueInspector::Construct(InspectorArgs);

			ChildSlot
			[
				SNew(SBox)
				.MaxDesiredWidth(250)
				.MaxDesiredHeight(200)
				[
					InArgs._VisualizationWidget
				]
			];
		}
	} // namespace Editor
} // namespace Metasound

#undef LOCTEXT_NAMESPACE
