// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_ControlRig.h"
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "GameFramework/Actor.h"
#include "Animation/NodeMappingContainer.h"
#include "AnimationRuntime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ControlRig)

#if WITH_EDITOR
#include "Editor.h"
#endif

FAnimNode_ControlRig::FAnimNode_ControlRig()
	: FAnimNode_ControlRigBase()
	, ControlRig(nullptr)
	, Alpha(1.f)
	, AlphaInputType(EAnimAlphaInputType::Float)
	, bAlphaBoolEnabled(true)
	, bSetRefPoseFromSkeleton(false)
	, AlphaCurveName(NAME_None)
	, LODThreshold(INDEX_NONE)
	, RefPoseSetterHash()
{
}

FAnimNode_ControlRig::~FAnimNode_ControlRig()
{
	if(ControlRig)
	{
		ControlRig->OnInitialized_AnyThread().RemoveAll(this);
	}
}

void FAnimNode_ControlRig::HandleOnInitialized_AnyThread(UControlRig*, const EControlRigState, const FName&)
{
	RefPoseSetterHash.Reset();
}

void FAnimNode_ControlRig::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (ControlRigClass)
	{
		ControlRig = NewObject<UControlRig>(InAnimInstance->GetOwningComponent(), ControlRigClass);
		ControlRig->Initialize(true);
		ControlRig->RequestInit();
		RefPoseSetterHash.Reset();
		ControlRig->OnInitialized_AnyThread().AddRaw(this, &FAnimNode_ControlRig::HandleOnInitialized_AnyThread);

		UpdateControlRigRefPoseIfNeeded(InProxy);
	}

	FAnimNode_ControlRigBase::OnInitializeAnimInstance(InProxy, InAnimInstance);

	InitializeProperties(InAnimInstance, GetTargetClass());
}

void FAnimNode_ControlRig::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(%s)"), *GetNameSafe(ControlRigClass.Get()));
	DebugData.AddDebugItem(DebugLine);
	Source.GatherDebugData(DebugData.BranchFlow(1.f));
}

void FAnimNode_ControlRig::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (IsLODEnabled(Context.AnimInstanceProxy))
	{
		GetEvaluateGraphExposedInputs().Execute(Context);

		// alpha handlers
		InternalBlendAlpha = 0.f;
		switch (AlphaInputType)
		{
		case EAnimAlphaInputType::Float:
			InternalBlendAlpha = AlphaScaleBias.ApplyTo(AlphaScaleBiasClamp.ApplyTo(Alpha, Context.GetDeltaTime()));
			break;
		case EAnimAlphaInputType::Bool:
			InternalBlendAlpha = AlphaBoolBlend.ApplyTo(bAlphaBoolEnabled, Context.GetDeltaTime());
			break;
		case EAnimAlphaInputType::Curve:
			if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject()))
			{
				InternalBlendAlpha = AlphaScaleBiasClamp.ApplyTo(AnimInstance->GetCurveValue(AlphaCurveName), Context.GetDeltaTime());
			}
			break;
		};

		// Make sure Alpha is clamped between 0 and 1.
		InternalBlendAlpha = FMath::Clamp<float>(InternalBlendAlpha, 0.f, 1.f);

		PropagateInputProperties(Context.AnimInstanceProxy->GetAnimInstanceObject());
	}
	else
	{
		InternalBlendAlpha = 0.f;
	}

	UpdateControlRigRefPoseIfNeeded(Context.AnimInstanceProxy);
	FAnimNode_ControlRigBase::Update_AnyThread(Context);

	TRACE_ANIM_NODE_VALUE(Context, TEXT("Class"), *GetNameSafe(ControlRigClass.Get()));
}

void FAnimNode_ControlRig::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_ControlRigBase::Initialize_AnyThread(Context);

	AlphaBoolBlend.Reinitialize();
	AlphaScaleBiasClamp.Reinitialize();
}

void FAnimNode_ControlRig::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_ControlRigBase::CacheBones_AnyThread(Context);

	FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();
	InputToCurveMappingUIDs.Reset();
	InputToControlIndex.Reset();

	if(RequiredBones.IsValid())
	{
		RefPoseSetterHash.Reset();

		const FSmartNameMapping* CurveMapping = RequiredBones.GetSkeletonAsset()->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

		auto CacheMapping = [&](const TMap<FName, FName>& Mapping, const FSmartNameMapping* CurveNameMapping, 
			const FAnimationCacheBonesContext& InContext, URigHierarchy* InHierarchy)
		{
			for (auto Iter = Mapping.CreateConstIterator(); Iter; ++Iter)
			{
				// we need to have list of variables using pin
				const FName SourcePath = Iter.Key();
				const FName TargetPath = Iter.Value();

				if (SourcePath != NAME_None && TargetPath != NAME_None)
				{
					const SmartName::UID_Type Found = CurveNameMapping->FindUID(TargetPath);
					if (Found != SmartName::MaxUID)
					{
						// set value - sound should be UID
						InputToCurveMappingUIDs.Add(Iter.Value()) = Found;
						continue;
					}
					else if(InHierarchy)
					{
						const FRigElementKey Key(TargetPath, ERigElementType::Control);
						if(const FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(Key))
						{
							InputToControlIndex.Add(TargetPath, ControlElement->GetIndex());
							continue;
						}
					}

					UE_LOG(LogAnimation, Warning, TEXT("Curve %s Not Found from the Skeleton %s"), 
						*TargetPath.ToString(), *GetNameSafe(InContext.AnimInstanceProxy->GetSkeleton()));
				}

				// @todo: should we clear the item if not found?
			}
		};

		URigHierarchy* Hierarchy = nullptr;
		if(UControlRig* CurrentControlRig = GetControlRig())
		{
			Hierarchy = CurrentControlRig->GetHierarchy();
		}

		CacheMapping(InputMapping, CurveMapping, Context, Hierarchy);
		CacheMapping(OutputMapping, CurveMapping, Context, Hierarchy);
	}
}

void FAnimNode_ControlRig::Evaluate_AnyThread(FPoseContext & Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// evaluate 
	FAnimNode_ControlRigBase::Evaluate_AnyThread(Output);
}

void FAnimNode_ControlRig::PostSerialize(const FArchive& Ar)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// after compile, we have to reinitialize
	// because it needs new execution code
	// since memory has changed
	if (Ar.IsObjectReferenceCollector())
	{
		if (ControlRig)
		{
			ControlRig->Initialize();
		}
	}
}

void FAnimNode_ControlRig::UpdateInput(UControlRig* InControlRig, const FPoseContext& InOutput)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_ControlRigBase::UpdateInput(InControlRig, InOutput);

	// now go through variable mapping table and see if anything is mapping through input
	if (InputMapping.Num() > 0 && InControlRig)
	{
		for (auto Iter = InputMapping.CreateConstIterator(); Iter; ++Iter)
		{
			// we need to have list of variables using pin
			const FName SourcePath = Iter.Key();
			if (SourcePath != NAME_None)
			{
				const FName CurveName = Iter.Value();

				SmartName::UID_Type UID = *InputToCurveMappingUIDs.Find(CurveName);
				if (UID != SmartName::MaxUID)
				{
					const float Value = InOutput.Curve.Get(UID);

					FRigVMExternalVariable Variable = InControlRig->GetPublicVariableByName(SourcePath);
					if (!Variable.bIsReadOnly && Variable.TypeName == TEXT("float"))
					{
						Variable.SetValue<float>(Value);
					}
					else
					{
						UE_LOG(LogAnimation, Warning, TEXT("[%s] Missing Input Variable [%s]"), *GetNameSafe(InControlRig->GetClass()), *SourcePath.ToString());
					}
				}
			}
		} 
	}
}

void FAnimNode_ControlRig::UpdateOutput(UControlRig* InControlRig, FPoseContext& InOutput)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_ControlRigBase::UpdateOutput(InControlRig, InOutput);

	if (OutputMapping.Num() > 0 && InControlRig)
	{
		for (auto Iter = OutputMapping.CreateConstIterator(); Iter; ++Iter)
		{
			// we need to have list of variables using pin
			const FName SourcePath = Iter.Key();
			const FName CurveName = Iter.Value();

			if (SourcePath != NAME_None)
			{
				FRigVMExternalVariable Variable = InControlRig->GetPublicVariableByName(SourcePath);
				if (Variable.TypeName == TEXT("float"))
				{
					float Value = Variable.GetValue<float>();
					SmartName::UID_Type* UID = InputToCurveMappingUIDs.Find(Iter.Value());
					if (UID)
					{
						InOutput.Curve.Set(*UID, Value);
					}
				}
				else
				{
					UE_LOG(LogAnimation, Warning, TEXT("[%s] Missing Output Variable [%s]"), *GetNameSafe(ControlRig->GetClass()), *SourcePath.ToString());
				}
			}
		}
	}
}

void FAnimNode_ControlRig::UpdateControlRigRefPoseIfNeeded(const FAnimInstanceProxy* InProxy, bool bIncludePoseInHash)
{
	if(!bSetRefPoseFromSkeleton)
	{
		return;
	}

	int32 ExpectedHash = 0;
	ExpectedHash = HashCombine(ExpectedHash, (int32)reinterpret_cast<uintptr_t>(InProxy->GetAnimInstanceObject()));
	ExpectedHash = HashCombine(ExpectedHash, (int32)reinterpret_cast<uintptr_t>(InProxy->GetSkelMeshComponent()));

	if(InProxy->GetSkelMeshComponent())
	{
		ExpectedHash = HashCombine(ExpectedHash, (int32)reinterpret_cast<uintptr_t>(InProxy->GetSkelMeshComponent()->GetSkeletalMeshAsset()));
	}

	if(bIncludePoseInHash)
	{
		FMemMark Mark(FMemStack::Get());
		FCompactPose RefPose;
		RefPose.ResetToRefPose(InProxy->GetRequiredBones());

		for(const FCompactPoseBoneIndex& BoneIndex : RefPose.ForEachBoneIndex())
		{
			const FTransform& Transform = RefPose.GetRefPose(BoneIndex);
			const FQuat Rotation = Transform.GetRotation();

			ExpectedHash = HashCombine(ExpectedHash, GetTypeHash(Transform.GetTranslation()));
			ExpectedHash = HashCombine(ExpectedHash, GetTypeHash(FVector4(Rotation.X, Rotation.Y, Rotation.Z, Rotation.W)));
			ExpectedHash = HashCombine(ExpectedHash, GetTypeHash(Transform.GetScale3D()));
		}

		if(RefPoseSetterHash.IsSet() && (ExpectedHash == RefPoseSetterHash.GetValue()))
		{
			return;
		}

		ControlRig->SetBoneInitialTransformsFromCompactPose(&RefPose);
	}
	else
	{
		if(RefPoseSetterHash.IsSet() && (ExpectedHash == RefPoseSetterHash.GetValue()))
		{
			return;
		}

		ControlRig->SetBoneInitialTransformsFromAnimInstanceProxy(InProxy);
	}
	
	RefPoseSetterHash = ExpectedHash;
}

void FAnimNode_ControlRig::SetIOMapping(bool bInput, const FName& SourceProperty, const FName& TargetCurve)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UClass* TargetClass = GetTargetClass();
	if (TargetClass)
	{
		UControlRig* CDO = TargetClass->GetDefaultObject<UControlRig>();
		if (CDO)
		{
			TMap<FName, FName>& MappingData = (bInput) ? InputMapping : OutputMapping;

			// if it's valid as of now, we add it
			bool bIsReadOnly = CDO->GetPublicVariableByName(SourceProperty).bIsReadOnly;
			if (!bInput || !bIsReadOnly)
			{
				if (TargetCurve == NAME_None)
				{
					MappingData.Remove(SourceProperty);
				}
				else
				{
					MappingData.FindOrAdd(SourceProperty) = TargetCurve;
				}
			}
		}
	}
}

FName FAnimNode_ControlRig::GetIOMapping(bool bInput, const FName& SourceProperty) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	const TMap<FName, FName>& MappingData = (bInput) ? InputMapping : OutputMapping;
	if (const FName* NameFound = MappingData.Find(SourceProperty))
	{
		return *NameFound;
	}

	return NAME_None;
}

void FAnimNode_ControlRig::InitializeProperties(const UObject* InSourceInstance, UClass* InTargetClass)
{
	// Build property lists
	SourceProperties.Reset(SourcePropertyNames.Num());
	DestProperties.Reset(SourcePropertyNames.Num());

	check(SourcePropertyNames.Num() == DestPropertyNames.Num());

	for (int32 Idx = 0; Idx < SourcePropertyNames.Num(); ++Idx)
	{
		FName& SourceName = SourcePropertyNames[Idx];
		UClass* SourceClass = InSourceInstance->GetClass();

		FProperty* SourceProperty = FindFProperty<FProperty>(SourceClass, SourceName);
		SourceProperties.Add(SourceProperty);
		DestProperties.Add(nullptr);
	}
}

void FAnimNode_ControlRig::PropagateInputProperties(const UObject* InSourceInstance)
{
	if (TargetInstance)
	{
		UControlRig* TargetControlRig = Cast<UControlRig>((UObject*)TargetInstance);

		// First copy properties
		check(SourceProperties.Num() == DestProperties.Num());
		for (int32 PropIdx = 0; PropIdx < SourceProperties.Num(); ++PropIdx)
		{
			FProperty* CallerProperty = SourceProperties[PropIdx];

			if(FRigControlElement* ControlElement = TargetControlRig->FindControl(DestPropertyNames[PropIdx]))
			{
				const uint8* SrcPtr = CallerProperty->ContainerPtrToValuePtr<uint8>(InSourceInstance);

				bool bIsValid = false;
				FRigControlValue Value;
				switch(ControlElement->Settings.ControlType)
				{
					case ERigControlType::Bool:
					{
						if(ensure(CastField<FBoolProperty>(CallerProperty)))
						{
							Value = FRigControlValue::Make<bool>(*(bool*)SrcPtr);
							bIsValid = true;
						}
						break;
					}
					case ERigControlType::Float:
					{
						if(ensure(CastField<FFloatProperty>(CallerProperty)))
						{
							Value = FRigControlValue::Make<float>(*(float*)SrcPtr);
							bIsValid = true;
						}
						break;
					}
					case ERigControlType::Integer:
					{
						if(ensure(CastField<FIntProperty>(CallerProperty)))
						{
							Value = FRigControlValue::Make<int32>(*(int32*)SrcPtr);
							bIsValid = true;
						}
						break;
					}
					case ERigControlType::Vector2D:
					{
						FStructProperty* StructProperty = CastField<FStructProperty>(CallerProperty);
						if(ensure(StructProperty))
						{
							if(ensure(StructProperty->Struct == TBaseStructure<FVector2D>::Get()))
							{
								const FVector2D& SrcVector = *(FVector2D*)SrcPtr;
								Value = FRigControlValue::Make<FVector2D>(SrcVector);
								bIsValid = true;
							}
						}
						break;
					}
					case ERigControlType::Position:
					case ERigControlType::Scale:
					{
						FStructProperty* StructProperty = CastField<FStructProperty>(CallerProperty);
						if(ensure(StructProperty))
						{
							if(ensure(StructProperty->Struct == TBaseStructure<FVector>::Get()))
							{
								const FVector& SrcVector = *(FVector*)SrcPtr;  
								Value = FRigControlValue::Make<FVector>(SrcVector);
								bIsValid = true;
							}
						}
						break;
					}
					case ERigControlType::Rotator:
					{
						FStructProperty* StructProperty = CastField<FStructProperty>(CallerProperty);
						if(ensure(StructProperty))
						{
							if(ensure(StructProperty->Struct == TBaseStructure<FRotator>::Get()))
							{
								const FRotator& SrcRotator = *(FRotator*)SrcPtr;
								Value = FRigControlValue::Make<FRotator>(SrcRotator);
								bIsValid = true;
							}
						}
						break;
					}
					case ERigControlType::Transform:
					{
						FStructProperty* StructProperty = CastField<FStructProperty>(CallerProperty);
						if(ensure(StructProperty))
						{
							if(ensure(StructProperty->Struct == TBaseStructure<FTransform>::Get()))
							{
								const FTransform& SrcTransform = *(FTransform*)SrcPtr;  
								Value = FRigControlValue::Make<FTransform>(SrcTransform);
								bIsValid = true;
							}
						}
						break;
					}
					case ERigControlType::TransformNoScale:
					{
						FStructProperty* StructProperty = CastField<FStructProperty>(CallerProperty);
						if(ensure(StructProperty))
						{
							if(ensure(StructProperty->Struct == TBaseStructure<FTransform>::Get()))
							{
								const FTransform& SrcTransform = *(FTransform*)SrcPtr;  
								Value = FRigControlValue::Make<FTransformNoScale>(SrcTransform);
								bIsValid = true;
							}
						}
						break;
					}
					case ERigControlType::EulerTransform:
					{
						FStructProperty* StructProperty = CastField<FStructProperty>(CallerProperty);
						if(ensure(StructProperty))
						{
							if(ensure(StructProperty->Struct == TBaseStructure<FTransform>::Get()))
							{
								const FTransform& SrcTransform = *(FTransform*)SrcPtr;  
								Value = FRigControlValue::Make<FEulerTransform>(FEulerTransform(SrcTransform));
								bIsValid = true;
							}
						}
						break;
					}
					default:
					{
						checkNoEntry();
					}
				}

				if(bIsValid)
				{
					TargetControlRig->GetHierarchy()->SetControlValue(ControlElement, Value, ERigControlValueType::Current);
				}
				continue;
			}

			FRigVMExternalVariable Variable = TargetControlRig->GetPublicVariableByName(DestPropertyNames[PropIdx]);
			if(Variable.IsValid())
			{
				if (Variable.bIsReadOnly)
				{
					continue;
				}

				const uint8* SrcPtr = CallerProperty->ContainerPtrToValuePtr<uint8>(InSourceInstance);

				if (CastField<FBoolProperty>(CallerProperty) != nullptr && Variable.TypeName == TEXT("bool"))
				{
					const bool Value = *(const bool*)SrcPtr;
					Variable.SetValue<bool>(Value);
				}
				else if (CastField<FFloatProperty>(CallerProperty) != nullptr && Variable.TypeName == TEXT("float"))
				{
					const float Value = *(const float*)SrcPtr;
					Variable.SetValue<float>(Value);
				}
				else if (CastField<FDoubleProperty>(CallerProperty) != nullptr && Variable.TypeName == TEXT("double"))
				{
					const double Value = *(const double*)SrcPtr;
					Variable.SetValue<double>(Value);
				}
				else if (CastField<FIntProperty>(CallerProperty) != nullptr && Variable.TypeName == TEXT("int32"))
				{
					const int32 Value = *(const int32*)SrcPtr;
					Variable.SetValue<int32>(Value);
				}
				else if (CastField<FNameProperty>(CallerProperty) != nullptr && Variable.TypeName == TEXT("FName"))
				{
					const FName Value = *(const FName*)SrcPtr;
					Variable.SetValue<FName>(Value);
				}
				else if (CastField<FNameProperty>(CallerProperty) != nullptr && Variable.TypeName == TEXT("FString"))
				{
					const FString Value = *(const FString*)SrcPtr;
					Variable.SetValue<FString>(Value);
				}
				else if (FStructProperty* StructProperty = CastField<FStructProperty>(CallerProperty))
				{
					if (StructProperty->Struct == Variable.TypeObject)
					{
						StructProperty->Struct->CopyScriptStruct(Variable.Memory, SrcPtr, 1);
					}
				}
				else if(FArrayProperty* ArrayProperty = CastField<FArrayProperty>(CallerProperty))
				{
					if(ensure(ArrayProperty->SameType(Variable.Property)))
					{
						ArrayProperty->CopyCompleteValue(Variable.Memory, SrcPtr);
					}
				}
				else
				{
					ensureMsgf(false, TEXT("Property %s type %s not recognized"), *CallerProperty->GetName(), *CallerProperty->GetCPPType());
				}
			}
		}
	}
}

#if WITH_EDITOR

void FAnimNode_ControlRig::HandleObjectsReinstanced_Impl(UObject* InSourceObject, UObject* InTargetObject, const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	Super::HandleObjectsReinstanced_Impl(InSourceObject, InTargetObject, OldToNewInstanceMap);
	
	if(ControlRig)
	{
		ControlRig->OnInitialized_AnyThread().RemoveAll(this);
		ControlRig->OnInitialized_AnyThread().AddRaw(this, &FAnimNode_ControlRig::HandleOnInitialized_AnyThread);
	}
}

#endif