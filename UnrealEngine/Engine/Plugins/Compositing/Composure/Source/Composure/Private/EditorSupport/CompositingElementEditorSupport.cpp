// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompositingElement.h"
#include "HAL/IConsoleManager.h"
#include "CompositingElements/CompElementRenderTargetPool.h"
#include "ComposurePlayerCompositingTarget.h"
#include "CompositingElements/CompositingElementInputs.h"
#include "CompositingElements/CompositingElementTransforms.h"
#include "CompositingElements/CompositingElementOutputs.h"
#include "CompositingElements/CompositingElementPassUtils.h"
#include "UObject/UObjectGlobals.h"
#include "EditorSupport/ICompositingEditor.h"
#include "Engine/Blueprint.h"


namespace CompositingElementEditorSupport_Impl
{
	template<typename T>
	T FindReplacedPass(const TArray<T>& PublicList, const TArray<T>& InternalList, const int32 ReplacedIndex);
}

template<typename T>
T CompositingElementEditorSupport_Impl::FindReplacedPass(const TArray<T>& PublicList, const TArray<T>& InternalList, const int32 ReplacedIndex)
{
	T FoundPass = nullptr;

	if (PublicList.IsValidIndex(ReplacedIndex))
	{
		if (T AlteredPass = PublicList[ReplacedIndex])
		{
			if (!InternalList.Contains(AlteredPass))
			{
				for (int32 PublicPassIndex = 0, InternalPassIndex = 0; PublicPassIndex <= ReplacedIndex && InternalPassIndex < InternalList.Num(); ++InternalPassIndex, ++PublicPassIndex)
				{
					while (PublicList[PublicPassIndex] == nullptr)
					{
						++PublicPassIndex;
					}

					if (PublicList[PublicPassIndex] != InternalList[InternalPassIndex])
					{
						FoundPass = InternalList[InternalPassIndex];
					}
				}
			}
		}
	}

	return FoundPass;
}

void ACompositingElement::SetEditorColorPickingTarget(UTextureRenderTarget2D* PickingTarget)
{
#if WITH_EDITOR
	ColorPickerTarget = PickingTarget;
#endif
}

void ACompositingElement::SetEditorColorPickerDisplayImage(UTexture* PickerDisplayImage)
{
#if WITH_EDITOR
	ColorPickerDisplayImage = PickerDisplayImage;
#endif
}

#if WITH_EDITOR
void ACompositingElement::OnBeginPreview()
{
	++PreviewCount;
}

UTexture* ACompositingElement::GetEditorPreviewImage()
{
	UTexture* PreviewImage = EditorPreviewImage;
	if (EditorPreviewImage == nullptr || bUsingDebugDisplayImage)
	{
		PreviewImage = CompositingTarget->GetDisplayTexture();
	}

	UClass* MyClass = GetClass();
	if (MyClass && MyClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(MyClass->ClassGeneratedBy))
		{
			if ((Blueprint->Status == EBlueprintStatus::BS_Error || Blueprint->Status == EBlueprintStatus::BS_Unknown))
			{
				PreviewImage = CompilerErrImage;
			}
		}
	}

	return PreviewImage;
}

void ACompositingElement::OnEndPreview()
{
	--PreviewCount;
}

bool ACompositingElement::UseImplicitGammaForPreview() const 
{
	UCompositingElementTransform* PreviewPass = GetPreviewPass();
	return (PreviewPass == nullptr) || !PreviewPass->IsPassEnabled();
}

UTexture* ACompositingElement::GetColorPickerDisplayImage()
{
	return (ColorPickerDisplayImage) ? ToRawPtr(ColorPickerDisplayImage) : (ColorPickerTarget ? ToRawPtr(ColorPickerTarget) : GetEditorPreviewImage());
}
UTextureRenderTarget2D* ACompositingElement::GetColorPickerTarget()
{
	return (ColorPickerTarget) ? ToRawPtr(ColorPickerTarget) : Cast<UTextureRenderTarget2D>(GetColorPickerDisplayImage());
}

FCompFreezeFrameController* ACompositingElement::GetFreezeFrameController()
{
	return &FreezeFrameController;
}

void ACompositingElement::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == TEXT("ActorLabel"))//GET_MEMBER_NAME_CHECKED(ACompositingElement, ActorLabel))
	{
		CompShotIdName = *GetActorLabel();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACompositingElement, bUseSharedTargetPool))
	{
		if (RenderTargetPool.IsValid())
		{
			RenderTargetPool->ReleaseAssignedTargets(this);
			RenderTargetPool.Reset();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACompositingElement, bAutoRun) || 
	         PropertyName == GET_MEMBER_NAME_CHECKED(ACompositingElement, bRunInEditor))
	{
		if (!IsActivelyRunning())
		{
			OnDisabled();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACompositingElement, Inputs))
	{
		if (!HasAnyFlags(RF_ClassDefaultObject))
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd && DefaultInputType)
			{
				const int32 AddedIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());
				if (Inputs.IsValidIndex(AddedIndex))
				{
					Inputs[AddedIndex] = FCompositingElementPassUtils::NewInstancedSubObj<UCompositingElementInput>(/*Outer =*/this, DefaultInputType);
					Inputs[AddedIndex]->PassName = MakeUniqueObjectName(this, UCompositingElementInput::StaticClass(), TEXT("InputPass"));
				}
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
			{
				const int32 AlteredIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());
				if (Inputs.IsValidIndex(AlteredIndex) && Inputs[AlteredIndex] && Inputs[AlteredIndex]->PassName.IsNone())
				{
					if (UCompositingElementInput* ReplacedInput = CompositingElementEditorSupport_Impl::FindReplacedPass(Inputs, GetInternalInputsList(), AlteredIndex))
					{
						Inputs[AlteredIndex]->PassName = ReplacedInput->PassName;
					}
					else if (!GetInternalInputsList().Contains(Inputs[AlteredIndex]))
					{
						Inputs[AlteredIndex]->PassName = MakeUniqueObjectName(this, UCompositingElementInput::StaticClass(), TEXT("InputPass"));
					}
				}			
			}
			RefreshInternalInputsList();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACompositingElement, TransformPasses))
	{
		if (!HasAnyFlags(RF_ClassDefaultObject))
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd && DefaultTransformType)
			{
				const int32 AddedIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());
				if (TransformPasses.IsValidIndex(AddedIndex))
				{
					TransformPasses[AddedIndex] = FCompositingElementPassUtils::NewInstancedSubObj<UCompositingElementTransform>(/*Outer =*/this, DefaultTransformType);
					TransformPasses[AddedIndex]->PassName = MakeUniqueObjectName(this, UCompositingElementTransform::StaticClass(), TEXT("TransformPass"));
				}
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
			{
				const int32 AlteredIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());
				if (TransformPasses.IsValidIndex(AlteredIndex) && TransformPasses[AlteredIndex] && TransformPasses[AlteredIndex]->PassName.IsNone())
				{
					if (UCompositingElementTransform* ReplacedInput = CompositingElementEditorSupport_Impl::FindReplacedPass(TransformPasses, GetInternalTransformsList(), AlteredIndex))
					{
						TransformPasses[AlteredIndex]->PassName = ReplacedInput->PassName;
					}
					else if (!GetInternalTransformsList().Contains(TransformPasses[AlteredIndex]))
					{
						TransformPasses[AlteredIndex]->PassName = MakeUniqueObjectName(this, UCompositingElementTransform::StaticClass(), TEXT("TransformPass"));
					}
				}
			}
			RefreshInternalTransformsList();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACompositingElement, Outputs))
	{
		if (!HasAnyFlags(RF_ClassDefaultObject))
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd && DefaultOutputType)
			{
				const int32 AddedIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());
				if (Outputs.IsValidIndex(AddedIndex))
				{
					Outputs[AddedIndex] = FCompositingElementPassUtils::NewInstancedSubObj<UCompositingElementOutput>(/*Outer =*/this, DefaultOutputType);
					Outputs[AddedIndex]->PassName = MakeUniqueObjectName(this, UCompositingElementOutput::StaticClass(), TEXT("OutputPass"));
				}
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
			{
				const int32 AlteredIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());
				if (Outputs.IsValidIndex(AlteredIndex) && Outputs[AlteredIndex] && Outputs[AlteredIndex]->PassName.IsNone())
				{
					if (UCompositingElementOutput* ReplacedInput = CompositingElementEditorSupport_Impl::FindReplacedPass(Outputs, GetInternalOutputsList(), AlteredIndex))
					{
						Outputs[AlteredIndex]->PassName = ReplacedInput->PassName;
					}
					else if (!GetInternalOutputsList().Contains(Outputs[AlteredIndex]))
					{
						Outputs[AlteredIndex]->PassName = MakeUniqueObjectName(this, UCompositingElementOutput::StaticClass(), TEXT("OutputPass"));
					}
				}
			}
			RefreshInternalOutputsList();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACompositingElement, bAutoRunChildElementsAndSelf))
	{
		SetAutoRunChildrenAndSelf(bAutoRunChildElementsAndSelf);
	}

	if (ICompositingEditor* CompositingEditor = ICompositingEditor::Get())
	{
		CompositingEditor->RequestRedraw();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ACompositingElement::PostEditUndo()
{
	Super::PostEditUndo();

	if (!IsActivelyRunning())
	{
		SetDebugDisplayImage(DisabledMsgImage);
	}

	RefreshAllInternalPassLists();
}

void ACompositingElement::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (Parent)
	{
		Parent->AttachAsChildLayer(this);
	}

	RefreshAllInternalPassLists();
}

void ACompositingElement::OnConstruction(const FTransform& Transform)
{
	OnConstructed.Broadcast(this);
	Super::OnConstruction(Transform);
}

UCompositingElementTransform* ACompositingElement::GetPreviewPass() const
{
	if (Parent && PreviewTransformSource == EInheritedSourceType::Inherited)
	{
		return Parent->GetPreviewPass();
	}
	return PreviewTransform;
}

bool ACompositingElement::IsPreviewing() const
{
	ensure(PreviewCount >= 0);
	return PreviewCount > 0 || CompositingTarget->IsPreviewing();
}

void ACompositingElement::OnPIEStarted(bool /*bIsSimulating*/)
{
	if (IsAutoRunSuspended())
	{
		if (RenderTargetPool.IsValid())
		{
			RenderTargetPool->ReleaseAssignedTargets(this);
		}
		SetDebugDisplayImage(SuspendedDbgImage);
	}
}

void ACompositingElement::SetDebugDisplayImage(UTexture* DebugDisplayImg)
{
	bUsingDebugDisplayImage = (DebugDisplayImg != nullptr);
	if (bUsingDebugDisplayImage)
	{
		PassResultsTable.SetMostRecentResult(nullptr);

		if (CompositingTarget)
		{
			CompositingTarget->SetDisplayTexture(DebugDisplayImg);
			CompositingTarget->SetUseImplicitGammaForPreview(true);
		}
	}
}
#endif // WITH_EDITOR
