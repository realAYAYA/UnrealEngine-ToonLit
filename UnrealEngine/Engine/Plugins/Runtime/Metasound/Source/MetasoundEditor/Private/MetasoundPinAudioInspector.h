// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DSP/MultithreadedPatching.h"
#include "EdGraph/EdGraphPin.h"
#include "MetasoundEditorGraphConnectionManager.h"
#include "SMetasoundPinAudioInspector.h"
#include "Templates/SharedPointer.h"

namespace AudioWidgets
{
	class FAudioOscilloscope;
}

class UMetasoundEditorGraphNode;

namespace Metasound
{
	namespace Editor
	{
		class METASOUNDEDITOR_API FMetasoundPinAudioInspector
		{
		public:
			explicit FMetasoundPinAudioInspector(FEdGraphPinReference InPinRef);
			virtual ~FMetasoundPinAudioInspector();

			TSharedPtr<SMetasoundPinAudioInspector> GetWidget();
			
		private:
			const UMetasoundEditorGraphNode& GetReroutedNode() const;
			FGraphConnectionManager* GetConnectionManager();

			UEdGraphPin* GraphPinObj = nullptr;

			TSharedPtr<AudioWidgets::FAudioOscilloscope> Oscilloscope = nullptr;
			Audio::FPatchInput PatchInput;
			
			TSharedPtr<SMetasoundPinAudioInspector> PinAudioInspectorWidget = nullptr;
		};
	} // namespace Editor
} // namespace Metasound
