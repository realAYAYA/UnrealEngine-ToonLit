// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorGraphMemberDefaults.h"

#include "Algo/Count.h"
#include "Algo/Transform.h"
#include "AudioDefines.h"
#include "AudioParameterControllerInterface.h"
#include "EdGraph/EdGraphNode.h"
#include "GraphEditorSettings.h"
#include "MetasoundDataReference.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundPrimitives.h"
#include "Misc/Guid.h"
#include "Sound/SoundWave.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundEditorGraphMemberDefaults)

namespace Metasound
{
	namespace Editor
	{
		namespace MemberDefaultsPrivate
		{
			template <typename TPODType>
			void ConvertLiteral(const FMetasoundFrontendLiteral& InLiteral, TPODType& OutValue)
			{
				InLiteral.TryGet(OutValue);
			}

			template <typename TPODType, typename TLiteralType = TPODType>
			void ConvertLiteralToArray(const FMetasoundFrontendLiteral& InLiteral, TArray<TLiteralType>& OutArray)
			{
				OutArray.Reset();
				TArray<TPODType> Values;
				InLiteral.TryGet(Values);
				Algo::Transform(Values, OutArray, [](const TPODType& InValue) { return TLiteralType{ InValue }; });
			}
		}
	}
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultBool::GetDefault() const 
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Default.Value);
	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultBool::GetLiteralType() const 
{
	return EMetasoundFrontendLiteralType::Boolean;
}

void UMetasoundEditorGraphMemberDefaultBool::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	Metasound::Editor::MemberDefaultsPrivate::ConvertLiteral<bool>(InLiteral, Default.Value);
}

void UMetasoundEditorGraphMemberDefaultBool::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	InParameterInterface->SetBoolParameter(InParameterName, Default.Value);
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultBoolArray::GetDefault() const
{
	TArray<bool> BoolArray;
	Algo::Transform(Default, BoolArray, [](const FMetasoundEditorGraphMemberDefaultBoolRef& InValue) { return InValue.Value; });

	FMetasoundFrontendLiteral Literal;
	Literal.Set(BoolArray);
	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultBoolArray::GetLiteralType() const
{
	return EMetasoundFrontendLiteralType::BooleanArray;
}

void UMetasoundEditorGraphMemberDefaultBoolArray::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	Metasound::Editor::MemberDefaultsPrivate::ConvertLiteralToArray<bool, FMetasoundEditorGraphMemberDefaultBoolRef>(InLiteral, Default);
}

void UMetasoundEditorGraphMemberDefaultBoolArray::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	TArray<bool> BoolArray;
	Algo::Transform(Default, BoolArray, [](const FMetasoundEditorGraphMemberDefaultBoolRef& InValue) { return InValue.Value; });
	InParameterInterface->SetBoolArrayParameter(InParameterName, BoolArray);
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultInt::GetDefault() const
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Default.Value);
	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultInt::GetLiteralType() const
{
	return EMetasoundFrontendLiteralType::Integer;
}

void UMetasoundEditorGraphMemberDefaultInt::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	Metasound::Editor::MemberDefaultsPrivate::ConvertLiteral<int32>(InLiteral, Default.Value);
}

void UMetasoundEditorGraphMemberDefaultInt::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	InParameterInterface->SetIntParameter(InParameterName, Default.Value);
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultIntArray::GetDefault() const
{
	TArray<int32> IntArray;
	Algo::Transform(Default, IntArray, [](const FMetasoundEditorGraphMemberDefaultIntRef& InValue) { return InValue.Value; });

	FMetasoundFrontendLiteral Literal;
	Literal.Set(IntArray);

	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultIntArray::GetLiteralType() const
{
	return EMetasoundFrontendLiteralType::IntegerArray;
}

void UMetasoundEditorGraphMemberDefaultIntArray::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	Metasound::Editor::MemberDefaultsPrivate::ConvertLiteralToArray<int32, FMetasoundEditorGraphMemberDefaultIntRef>(InLiteral, Default);
}

void UMetasoundEditorGraphMemberDefaultIntArray::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	TArray<int32> IntArray;
	Algo::Transform(Default, IntArray, [](const FMetasoundEditorGraphMemberDefaultIntRef& InValue) { return InValue.Value; });
	InParameterInterface->SetIntArrayParameter(InParameterName, IntArray);
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultFloat::GetDefault() const
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Default);
	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultFloat::GetLiteralType() const
{
	return EMetasoundFrontendLiteralType::Float;
}

void UMetasoundEditorGraphMemberDefaultFloat::ForceRefresh()
{
	OnRangeChanged.Broadcast(Range);
	SetDefault(FMath::Clamp(Default, Range.X, Range.Y));
}

void UMetasoundEditorGraphMemberDefaultFloat::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	Metasound::Editor::MemberDefaultsPrivate::ConvertLiteral<float>(InLiteral, Default);

	// If set from literal, we force the default value to be the literal's value which may require the range to be fixed up 
	SetInitialRange();
}

void UMetasoundEditorGraphMemberDefaultFloat::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	InParameterInterface->SetFloatParameter(InParameterName, Default);
}

void UMetasoundEditorGraphMemberDefaultFloat::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName().IsEqual(GET_MEMBER_NAME_CHECKED(UMetasoundEditorGraphMemberDefaultFloat, Default)))
	{
		SetDefault(Default);
	}
	else if (PropertyChangedEvent.GetPropertyName().IsEqual(GET_MEMBER_NAME_CHECKED(UMetasoundEditorGraphMemberDefaultFloat, WidgetType)) ||
		PropertyChangedEvent.GetPropertyName().IsEqual(GET_MEMBER_NAME_CHECKED(UMetasoundEditorGraphMemberDefaultFloat, WidgetValueType)))
	{
		// Update VolumeWidgetDecibelRange based on current range (it might be stale)
		if (WidgetValueType == EMetasoundMemberDefaultWidgetValueType::Volume && VolumeWidgetUseLinearOutput)
		{
			VolumeWidgetDecibelRange = FVector2D(Audio::ConvertToDecibels(Range.X), Audio::ConvertToDecibels(Range.Y));
		}
		else
		{
			SetInitialRange();
		}

		// If the widget type is changed to none, we need to refresh clamping the value or not, since if the widget was a slider before, the value was clamped
		OnClampChanged.Broadcast(ClampDefault);
	}
	else if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName().IsEqual(GET_MEMBER_NAME_CHECKED(UMetasoundEditorGraphMemberDefaultFloat, Range)))
	{
		if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
		{
			if (WidgetType != EMetasoundMemberDefaultWidget::None &&
				WidgetValueType == EMetasoundMemberDefaultWidgetValueType::Volume)
			{
				if (VolumeWidgetUseLinearOutput)
				{
					Range.X = FMath::Max(Range.X, Audio::ConvertToLinear(SAudioVolumeRadialSlider::MinDbValue));
					Range.Y = FMath::Min(Range.Y, Audio::ConvertToLinear(SAudioVolumeRadialSlider::MaxDbValue));
					if (Range.X > Range.Y)
					{
						Range.Y = Range.X;
					}
					VolumeWidgetDecibelRange = FVector2D(Audio::ConvertToDecibels(Range.X), Audio::ConvertToDecibels(Range.Y));
					OnRangeChanged.Broadcast(VolumeWidgetDecibelRange);
					SetDefault(FMath::Clamp(Default, Range.X, Range.Y));
				}
			}
			else
			{
				// if Range.X > Range.Y, set Range.Y to Range.X
				if (Range.X > Range.Y)
				{
					SetRange(FVector2D(Range.X, FMath::Max(Range.X, Range.Y)));
				}
				else
				{
					ForceRefresh();
				}
			}
		}
	}
	else if (PropertyChangedEvent.GetPropertyName().IsEqual(GET_MEMBER_NAME_CHECKED(UMetasoundEditorGraphMemberDefaultFloat, ClampDefault)))
	{
		SetInitialRange();
		OnClampChanged.Broadcast(ClampDefault);
	}
	else if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName().IsEqual(GET_MEMBER_NAME_CHECKED(UMetasoundEditorGraphMemberDefaultFloat, VolumeWidgetDecibelRange)))
	{
		if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
		{
			VolumeWidgetDecibelRange.X = FMath::Max(VolumeWidgetDecibelRange.X, SAudioVolumeRadialSlider::MinDbValue);
			VolumeWidgetDecibelRange.Y = FMath::Min(VolumeWidgetDecibelRange.Y, SAudioVolumeRadialSlider::MaxDbValue);
			SetRange(FVector2D(Audio::ConvertToLinear(VolumeWidgetDecibelRange.X), Audio::ConvertToLinear(VolumeWidgetDecibelRange.Y)));
		}
	}
	else if (PropertyChangedEvent.GetPropertyName().IsEqual(GET_MEMBER_NAME_CHECKED(UMetasoundEditorGraphMemberDefaultFloat, VolumeWidgetUseLinearOutput)))
	{
		if (VolumeWidgetUseLinearOutput)
		{
			// Range and default are currently in dB, need to change to linear
			float DbDefault = Default;
			VolumeWidgetDecibelRange = Range;
			Range = FVector2D(Audio::ConvertToLinear(VolumeWidgetDecibelRange.X), Audio::ConvertToLinear(VolumeWidgetDecibelRange.Y));
			Default = Audio::ConvertToLinear(DbDefault);
		}
		else
		{
			// Range and default are currently linear, need to change to dB
			Range = VolumeWidgetDecibelRange;
			Default = Audio::ConvertToDecibels(Default);
		}
	}

	UMetasoundEditorGraphMember* Member = GetParentMember();
	if (ensure(Member))
	{
		// Only update member on non-interactive changes to avoid refreshing the details panel mid-update
		if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
		{
			FMetasoundFrontendDocumentModifyContext& ModifyContext = Member->GetOwningGraph()->GetModifyContext();
			ModifyContext.AddMemberIDsModified({ Member->GetMemberID() });

			// Mark all nodes as modified to refresh them on synchronization.  This ensures all corresponding widgets get updated.
			const TArray<UMetasoundEditorGraphMemberNode*> MemberNodes = Member->GetNodes();
			TSet<FGuid> NodesToRefresh;
			Algo::Transform(MemberNodes, NodesToRefresh, [](const UMetasoundEditorGraphMemberNode* MemberNode) { return MemberNode->GetNodeID(); });
			ModifyContext.AddNodeIDsModified({ NodesToRefresh });
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UMetasoundEditorGraphMemberDefaultFloat::SetDefault(const float InDefault)
{
	if (!FMath::IsNearlyEqual(Default, InDefault))
	{
		Default = InDefault;
		OnDefaultValueChanged.Broadcast(InDefault);
	}
}

void UMetasoundEditorGraphMemberDefaultFloat::SetInitialRange()
{
	// If value is within current range, keep it, otherwise set range to something reasonable 
	if (!(Default >= Range.X && Default <= Range.Y))
	{
		if (FMath::IsNearlyEqual(Default, 0.0f))
		{
			SetRange(FVector2D(0.0f, 1.0f));
		}
		else
		{
			SetRange(FVector2D(FMath::Min(0.0f, Default), FMath::Max(0.0f, Default)));
		}
	}
}

float UMetasoundEditorGraphMemberDefaultFloat::GetDefault()
{
	return Default;
}

FVector2D UMetasoundEditorGraphMemberDefaultFloat::GetRange()
{
	return Range;
}

void UMetasoundEditorGraphMemberDefaultFloat::SetRange(const FVector2D InRange)
{
	if (!(Range - InRange).IsNearlyZero())
	{
		Range = InRange;
		OnRangeChanged.Broadcast(InRange);
		SetDefault(FMath::Clamp(Default, Range.X, Range.Y));
	}
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultFloatArray::GetDefault() const
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Default);
	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultFloatArray::GetLiteralType() const
{
	return EMetasoundFrontendLiteralType::FloatArray;
}

void UMetasoundEditorGraphMemberDefaultFloatArray::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	Metasound::Editor::MemberDefaultsPrivate::ConvertLiteralToArray<float>(InLiteral, Default);
}

void UMetasoundEditorGraphMemberDefaultFloatArray::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	InParameterInterface->SetFloatArrayParameter(InParameterName, Default);
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultString::GetDefault() const
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Default);
	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultString::GetLiteralType() const
{
	return EMetasoundFrontendLiteralType::String;
}

void UMetasoundEditorGraphMemberDefaultString::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	Metasound::Editor::MemberDefaultsPrivate::ConvertLiteral<FString>(InLiteral, Default);
}

void UMetasoundEditorGraphMemberDefaultString::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	InParameterInterface->SetStringParameter(InParameterName, Default);
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultStringArray::GetDefault() const
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Default);
	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultStringArray::GetLiteralType() const
{
	return EMetasoundFrontendLiteralType::StringArray;
}

void UMetasoundEditorGraphMemberDefaultStringArray::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	Metasound::Editor::MemberDefaultsPrivate::ConvertLiteralToArray<FString>(InLiteral, Default);
}

void UMetasoundEditorGraphMemberDefaultStringArray::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	InParameterInterface->SetStringArrayParameter(InParameterName, Default);
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultObject::GetDefault() const
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Default.Object);
	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultObject::GetLiteralType() const
{
	return EMetasoundFrontendLiteralType::UObject;
}

void UMetasoundEditorGraphMemberDefaultObject::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	UObject* Object = nullptr;
	ensure(InLiteral.TryGet(Object));
	Default.Object = Object;
}

void UMetasoundEditorGraphMemberDefaultObject::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	// TODO. We need proxy object here safely.
}

FMetasoundFrontendLiteral UMetasoundEditorGraphMemberDefaultObjectArray::GetDefault() const
{
	TArray<UObject*> ObjectArray;
	Algo::Transform(Default, ObjectArray, [](const FMetasoundEditorGraphMemberDefaultObjectRef& InValue) { return InValue.Object; });

	FMetasoundFrontendLiteral Literal;
	Literal.Set(ObjectArray);
	return Literal;
}

EMetasoundFrontendLiteralType UMetasoundEditorGraphMemberDefaultObjectArray::GetLiteralType() const
{
	return EMetasoundFrontendLiteralType::UObjectArray;
}

void UMetasoundEditorGraphMemberDefaultObjectArray::SetFromLiteral(const FMetasoundFrontendLiteral& InLiteral)
{
	TArray<UObject*> ObjectArray;
	ensure(InLiteral.TryGet(ObjectArray));

	Default.Reset();
	Algo::Transform(ObjectArray, Default, [](UObject* InValue) { return FMetasoundEditorGraphMemberDefaultObjectRef { InValue }; });
}

void UMetasoundEditorGraphMemberDefaultObjectArray::UpdatePreviewInstance(const Metasound::FVertexName& InParameterName, TScriptInterface<IAudioParameterControllerInterface>& InParameterInterface) const
{
	TArray<UObject*> ObjectArray;
	Algo::Transform(Default, ObjectArray, [](const FMetasoundEditorGraphMemberDefaultObjectRef& InValue) { return InValue.Object; });
	// TODO. We need proxy object here safely.
}

