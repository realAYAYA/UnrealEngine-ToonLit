// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Algo/Count.h"
#include "Algo/Transform.h"
#include "AudioParameterControllerInterface.h"
#include "Components/Widget.h"
#include "Delegates/Delegate.h"
#include "EdGraph/EdGraphNode.h"
#include "GraphEditorSettings.h"
#include "MetasoundDataReference.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "Misc/Guid.h"
#include "SAudioRadialSlider.h"
#include "Sound/SoundWave.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"

#include "MetasoundEditorGraphMemberDefaults.generated.h"


// Broken out to be able to customize and swap enum behavior for basic integer literal behavior
USTRUCT()
struct FMetasoundEditorGraphMemberDefaultBoolRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	bool Value = false;
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultBool : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

public:
	virtual ~UMetasoundEditorGraphMemberDefaultBool() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	FMetasoundEditorGraphMemberDefaultBoolRef Default;

	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultBoolArray : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

public:
	virtual ~UMetasoundEditorGraphMemberDefaultBoolArray() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	TArray<FMetasoundEditorGraphMemberDefaultBoolRef> Default;

	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
};

// Broken out to be able to customize and swap enum behavior for basic integer literal behavior
USTRUCT()
struct FMetasoundEditorGraphMemberDefaultIntRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	int32 Value = 0;
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultInt : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

public:
	virtual ~UMetasoundEditorGraphMemberDefaultInt() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	FMetasoundEditorGraphMemberDefaultIntRef Default;

	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultIntArray : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

public:
	virtual ~UMetasoundEditorGraphMemberDefaultIntArray() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	TArray<FMetasoundEditorGraphMemberDefaultIntRef> Default;

	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
};

// For input widget
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetasoundInputValueChangedEvent, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetasoundRangeChangedEvent, FVector2D);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetasoundInputClampDefaultChangedEvent, bool);


UENUM()
enum class EMetasoundMemberDefaultWidgetValueType : uint8
{
	Linear,
	Frequency UMETA(DisplayName = "Frequency (Log)"),
	Volume
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultFloat : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

private:
	UPROPERTY(EditAnywhere, Category = DefaultValue)
	float Default = 0.f;

public:
	virtual ~UMetasoundEditorGraphMemberDefaultFloat() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue, meta=(EditCondition = "WidgetType == EMetasoundMemberDefaultWidget::None", EditConditionHides))
	bool ClampDefault = false;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	FVector2D Range = FVector2D(0.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category = Widget, meta=(DisplayName = "Widget"))
	EMetasoundMemberDefaultWidget WidgetType = EMetasoundMemberDefaultWidget::None;
	
	UPROPERTY(EditAnywhere, Category = Widget, meta=(DisplayName = "Orientation", EditCondition = "WidgetType == EMetasoundMemberDefaultWidget::Slider", EditConditionHides))
	TEnumAsByte<EOrientation> WidgetOrientation = EOrientation::Orient_Horizontal;

	UPROPERTY(EditAnywhere, Category = Widget, meta=(DisplayName = "Value Type", EditCondition = "WidgetType != EMetasoundMemberDefaultWidget::None", EditConditionHides))
	EMetasoundMemberDefaultWidgetValueType WidgetValueType = EMetasoundMemberDefaultWidgetValueType::Linear;

	/** If true, output linear value. Otherwise, output dB value. The volume widget itself will always display the value in dB. The Default Value and Range are linear. */
	UPROPERTY(EditAnywhere, Category = Widget, meta = (DisplayName = "Output Linear"))
	bool VolumeWidgetUseLinearOutput = true;

	/** Range in decibels. This will be converted to the linear range in the Default Value category. */
	UPROPERTY(EditAnywhere, Category = Widget, meta = (DisplayName = "Range in dB", EditCondition = "VolumeWidgetUseLinearOutput", EditConditionHides))
	FVector2D VolumeWidgetDecibelRange = FVector2D(SAudioVolumeRadialSlider::MinDbValue, 0.0f);

	static FName GetDefaultPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(UMetasoundEditorGraphMemberDefaultFloat, Default);
	}

	FOnMetasoundInputValueChangedEvent OnDefaultValueChanged;
	FOnMetasoundRangeChangedEvent OnRangeChanged;
	FOnMetasoundInputClampDefaultChangedEvent OnClampChanged;

	virtual void ForceRefresh() override;
	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent) override;

	void SetDefault(const float InDefault);
	// Set Range to reasonable limit given current Default value
	void SetInitialRange();
	float GetDefault();
	FVector2D GetRange();
	void SetRange(const FVector2D InRange);
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultFloatArray : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

public:
	virtual ~UMetasoundEditorGraphMemberDefaultFloatArray() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	TArray<float> Default;

	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultString : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

public:
	virtual ~UMetasoundEditorGraphMemberDefaultString() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	FString Default;

	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultStringArray : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

public:
	virtual ~UMetasoundEditorGraphMemberDefaultStringArray() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	TArray<FString> Default;

	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
};

// Broken out to be able to customize and swap AllowedClass based on provided object proxy
USTRUCT()
struct FMetasoundEditorGraphMemberDefaultObjectRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	TObjectPtr<UObject> Object = nullptr;
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultObject : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

public:
	virtual ~UMetasoundEditorGraphMemberDefaultObject() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	FMetasoundEditorGraphMemberDefaultObjectRef Default;

	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphMemberDefaultObjectArray : public UMetasoundEditorGraphMemberDefaultLiteral
{
	GENERATED_BODY()

public:
	virtual ~UMetasoundEditorGraphMemberDefaultObjectArray() = default;

	UPROPERTY(EditAnywhere, Category = DefaultValue)
	TArray<FMetasoundEditorGraphMemberDefaultObjectRef> Default;

	virtual FMetasoundFrontendLiteral GetDefault() const override;
	virtual EMetasoundFrontendLiteralType GetLiteralType() const override;
	virtual void SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral) override;
	virtual void UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const override;
};