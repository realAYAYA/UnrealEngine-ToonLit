// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/CoreDefines.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "MetasoundEditorSettings.generated.h"

UENUM()
enum class EMetasoundActiveAnalyzerEnvelopeDirection : uint8
{
	FromSourceOutput,
	FromDestinationInput
};

UENUM()
enum class EMetasoundMemberDefaultWidget : uint8
{
	None,
	Slider,
	RadialSlider UMETA(DisplayName = "Knob"),
};

UENUM()
enum class EMetasoundActiveDetailView : uint8
{
	Metasound,
	General
};

USTRUCT()
struct FMetasoundAnalyzerAnimationSettings
{
	GENERATED_BODY()

	/** Whether or not animated connections are enabled. */
	UPROPERTY(EditAnywhere, config, Category = GraphAnimation, meta = (DisplayName = "Animate Connections (Beta)"))
	bool bAnimateConnections = true;

	/** Thickness of default envelope analyzer wire thickness when connection analyzer is active. */
	UPROPERTY(EditAnywhere, config, Category = GraphAnimation, meta = (EditCondition = "bAnimateConnections", UIMin = 1, UIMax = 2, ClampMin = 1))
	float EnvelopeWireThickness = 1.0f;

	/** Speed of default envelope analyzer drawing over wire when connection analyzer is active, where 0 is full visual history (slowest progress) and 1 is no visual history (fastest progress). */
	UPROPERTY(EditAnywhere, config, Category = GraphAnimation, meta = (EditCondition = "bAnimateConnections", ClampMin = 0, ClampMax = 1))
	float EnvelopeSpeed = 0.95f;

	/** Whether analyzer envelopes draw from a source output (default) or from the destination input. From the destination input may not
	  * give the expected illusion of audio processing flowing left-to-right, but results in a waveform with earlier events on the left
	  * and later on the right (like a traditional timeline with a moving play head). */
	UPROPERTY(EditAnywhere, config, Category = GraphAnimation, meta = (EditCondition = "bAnimateConnections", ClampMin = 0, ClampMax = 1))
	EMetasoundActiveAnalyzerEnvelopeDirection EnvelopeDirection = EMetasoundActiveAnalyzerEnvelopeDirection::FromSourceOutput;

	/** Thickness of default numeric analyzer wire thickness when connection analyzer is active. */
	UPROPERTY(EditAnywhere, config, Category = GraphAnimation, meta = (EditCondition = "bAnimateConnections", UIMin = 1, UIMax = 10, ClampMin = 1))
	float NumericWireThickness = 5.0f;

	/** Minimum height scalar of wire signal analyzers (ex. audio, triggers). */
	UPROPERTY(EditAnywhere, config, Category = GraphAnimation, meta = (EditCondition = "bAnimateConnections", UIMin = 1, UIMax = 5, ClampMin = 1))
	float WireScalarMin = 1.0f;

	/** Maximum height scalar of wire signal analyzers (ex. audio, triggers). */
	UPROPERTY(EditAnywhere, config, Category = GraphAnimation, meta = (EditCondition = "bAnimateConnections", UIMin = 1, UIMax = 5, ClampMin = 1))
	float WireScalarMax = 4.5f;
};

UCLASS(config=EditorPerProjectUserSettings)
class METASOUNDEDITOR_API UMetasoundEditorSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Whether to pin the MetaSound Patch asset type when creating new assets. */
	UPROPERTY(EditAnywhere, config, DisplayName = "Pin MetaSound Patch in Asset Menu", Category = AssetMenu)
	bool bPinMetaSoundPatchInAssetMenu = false;

	/** Whether to pin the MetaSound Source asset type when creating new assets. */
	UPROPERTY(EditAnywhere, config, DisplayName = "Pin MetaSound Source in Asset Menu", Category = AssetMenu)
	bool bPinMetaSoundSourceInAssetMenu = true;

	/** Default author title to use when authoring a new
	  * MetaSound.  If empty, uses machine name by default.
	  */
	UPROPERTY(EditAnywhere, config, Category=General)
	FString DefaultAuthor;

	/** Default pin type color */
	UPROPERTY(EditAnywhere, config, Category=PinColors)
	FLinearColor DefaultPinTypeColor;

	/** Audio pin type color */
	UPROPERTY(EditAnywhere, config, Category=PinColors)
	FLinearColor AudioPinTypeColor;

	/** Boolean pin type color */
	UPROPERTY(EditAnywhere, config, Category=PinColors)
	FLinearColor BooleanPinTypeColor;

	/** Floating-point pin type color */
	UPROPERTY(EditAnywhere, config, Category=PinColors)
	FLinearColor FloatPinTypeColor;

	/** Integer pin type color */
	UPROPERTY(EditAnywhere, config, Category=PinColors)
	FLinearColor IntPinTypeColor;

	/** Object pin type color */
	UPROPERTY(EditAnywhere, config, Category=PinColors)
	FLinearColor ObjectPinTypeColor;

	/** String pin type color */
	UPROPERTY(EditAnywhere, config, Category=PinColors)
	FLinearColor StringPinTypeColor;

	/** Time pin type color */
	UPROPERTY(EditAnywhere, config, Category=PinColors)
	FLinearColor TimePinTypeColor;

	/** Trigger pin type color */
	UPROPERTY(EditAnywhere, config, Category=PinColors)
	FLinearColor TriggerPinTypeColor;

	/** WaveTable pin type color */
	UPROPERTY(EditAnywhere, config, Category = PinColors)
	FLinearColor WaveTablePinTypeColor;

	/** Native node class title color */
	UPROPERTY(EditAnywhere, config, Category = NodeTitleColors)
	FLinearColor NativeNodeTitleColor;

	/** Title color for references to MetaSound assets */
	UPROPERTY(EditAnywhere, config, Category = NodeTitleColors)
	FLinearColor AssetReferenceNodeTitleColor;

	/** Input node title color */
	UPROPERTY(EditAnywhere, config, Category = NodeTitleColors)
	FLinearColor InputNodeTitleColor;

	/** Output node title color */
	UPROPERTY(EditAnywhere, config, Category = NodeTitleColors)
	FLinearColor OutputNodeTitleColor;

	/** Variable node title color */
	UPROPERTY(EditAnywhere, config, Category = NodeTitleColors)
	FLinearColor VariableNodeTitleColor;

	/** Widget type to show on input nodes by default */
	UPROPERTY(EditAnywhere, config, Category = General)
	EMetasoundMemberDefaultWidget DefaultInputWidgetType = EMetasoundMemberDefaultWidget::RadialSlider;

	/** Settings for visualizing analyzed MetaSound connections */
	UPROPERTY(EditAnywhere, config, Category = GraphAnimation, meta = (ShowOnlyInnerProperties))
	FMetasoundAnalyzerAnimationSettings AnalyzerAnimationSettings;

	/** Determines which details view to show in Metasounds Editor */
	UPROPERTY(Transient)
	EMetasoundActiveDetailView DetailView = EMetasoundActiveDetailView::Metasound;
};
