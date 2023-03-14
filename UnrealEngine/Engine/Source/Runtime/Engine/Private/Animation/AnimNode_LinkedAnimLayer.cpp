// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_LinkedAnimLayer.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_LinkedAnimLayer)

FName FAnimNode_LinkedAnimLayer::GetDynamicLinkFunctionName() const
{
	return Layer;
}

UAnimInstance* FAnimNode_LinkedAnimLayer::GetDynamicLinkTarget(UAnimInstance* InOwningAnimInstance) const
{
	if(Interface.Get())
	{
		return GetTargetInstance<UAnimInstance>();
	}
	else
	{
		return InOwningAnimInstance;
	}
}

void FAnimNode_LinkedAnimLayer::InitializeSourceProperties(const UAnimInstance* InAnimInstance)
{
	const int32 NumSourceProperties = SourcePropertyNames.Num();
	SourceProperties.SetNumZeroed(NumSourceProperties);

	const UClass* ThisClass = InAnimInstance->GetClass();
	for(int32 SourcePropertyIndex = 0; SourcePropertyIndex < NumSourceProperties; ++SourcePropertyIndex)
	{
		SourceProperties[SourcePropertyIndex] = ThisClass->FindPropertyByName(SourcePropertyNames[SourcePropertyIndex]);
	}
}

void FAnimNode_LinkedAnimLayer::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
#if WITH_EDITORONLY_DATA
	SourceInstance = const_cast<UAnimInstance*>(InAnimInstance);
#endif
	
	check(SourcePropertyNames.Num() == DestPropertyNames.Num());
	
	// Initialize source properties here as they do not change
	InitializeSourceProperties(InAnimInstance);

	// We only initialize here if we are running a 'self' layer. Layers that use external instances need to be 
	// initialized by the owning anim instance as they may share linked instances via grouping.
	if(Interface.Get() == nullptr || InstanceClass.Get() == nullptr)
	{
		InitializeSelfLayer(InAnimInstance);
	}
}

void FAnimNode_LinkedAnimLayer::InitializeSelfLayer(const UAnimInstance* SelfAnimInstance)
{
	UAnimInstance* CurrentTarget = GetTargetInstance<UAnimInstance>();

	IAnimClassInterface* PriorAnimBPClass = CurrentTarget ? IAnimClassInterface::GetFromClass(CurrentTarget->GetClass()) : nullptr;

	USkeletalMeshComponent* MeshComp = SelfAnimInstance->GetSkelMeshComponent();
	check(MeshComp);

	if (LinkedRoot)
	{
		DynamicUnlink(const_cast<UAnimInstance*>(SelfAnimInstance));
	}

	// Switch from dynamic external to internal, kill old instance
	if (CurrentTarget && CurrentTarget != SelfAnimInstance)
	{
		CurrentTarget->UninitializeAnimation();
		MeshComp->GetLinkedAnimInstances().Remove(CurrentTarget);
		CurrentTarget->MarkAsGarbage();
		CurrentTarget = nullptr;
	}

	SetTargetInstance(const_cast<UAnimInstance*>(SelfAnimInstance));

	// Link before we call InitializeAnimation() so we propgate the call to linked input poses
	DynamicLink(const_cast<UAnimInstance*>(SelfAnimInstance));

	UClass* SelfClass = SelfAnimInstance->GetClass();
	InitializeProperties(SelfAnimInstance, SelfClass);

	IAnimClassInterface* NewAnimBPClass = IAnimClassInterface::GetFromClass(SelfClass);

	RequestBlend(PriorAnimBPClass, NewAnimBPClass);
}

void FAnimNode_LinkedAnimLayer::SetLinkedLayerInstance(const UAnimInstance* InOwningAnimInstance, UAnimInstance* InNewLinkedInstance)
{
	// Reseting to running as a self-layer, in case it is applicable
	if ((Interface.Get() == nullptr || InstanceClass.Get() == nullptr) && (InNewLinkedInstance == nullptr))
	{
		InitializeSelfLayer(InOwningAnimInstance);
	}
	else
	{
		ReinitializeLinkedAnimInstance(InOwningAnimInstance, InNewLinkedInstance);
	}

#if WITH_EDITOR
	OnInstanceChangedEvent.Broadcast();
#endif
}

void FAnimNode_LinkedAnimLayer::InitializeProperties(const UObject* InSourceInstance, UClass* InTargetClass)
{
	check(SourcePropertyNames.Num() == DestPropertyNames.Num());

#if WITH_EDITOR
	// When reinstancing we need to init source properties here too as the class may have changed
	if(GIsReinstancing)
	{
		InitializeSourceProperties(CastChecked<UAnimInstance>(InSourceInstance));
	}
#endif
	
	// Build dest property list - source is set up when we initialize
	DestProperties.SetNumZeroed(SourcePropertyNames.Num());
	
	IAnimClassInterface* TargetAnimClassInterface = IAnimClassInterface::GetFromClass(InTargetClass);
	check(TargetAnimClassInterface);

	const FName FunctionName = GetDynamicLinkFunctionName();
	if(const FAnimBlueprintFunction* Function = IAnimClassInterface::FindAnimBlueprintFunction(TargetAnimClassInterface, FunctionName))
	{
		// Target properties are linked via the anim BP function params
		for(int32 DestPropertyIndex = 0; DestPropertyIndex < DestPropertyNames.Num(); ++DestPropertyIndex)
		{
			const FName& DestName = DestPropertyNames[DestPropertyIndex];

			// Look for an input property (parameter) with the specified name
			const int32 NumParams = Function->InputPropertyData.Num();
			for(int32 InputPropertyIndex = 0; InputPropertyIndex < NumParams; ++InputPropertyIndex)
			{
				if(Function->InputPropertyData[InputPropertyIndex].Name == DestName)
				{
					DestProperties[DestPropertyIndex] = Function->InputPropertyData[InputPropertyIndex].ClassProperty;
					break;
				}
			}
		}
	}
}

#if WITH_EDITOR

void FAnimNode_LinkedAnimLayer::HandleObjectsReinstanced_Impl(UObject* InSourceObject, UObject* InTargetObject, const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	static IConsoleVariable* UseLegacyAnimInstanceReinstancingBehavior = IConsoleManager::Get().FindConsoleVariable(TEXT("bp.UseLegacyAnimInstanceReinstancingBehavior"));
	if(UseLegacyAnimInstanceReinstancingBehavior == nullptr || !UseLegacyAnimInstanceReinstancingBehavior->GetBool())
	{
		UAnimInstance* SourceAnimInstance = CastChecked<UAnimInstance>(InSourceObject);
		FAnimInstanceProxy& SourceProxy = SourceAnimInstance->GetProxyOnAnyThread<FAnimInstanceProxy>();

		// Call Initialize here to ensure any custom proxies are initialized (as they may have been re-created during
		// re-instancing, and they dont call the constructor that takes a UAnimInstance*)
		SourceProxy.Initialize(SourceAnimInstance);

		InitializeProperties(SourceAnimInstance, Interface.Get() != nullptr ? Interface.Get() : InSourceObject->GetClass());
		DynamicUnlink(SourceAnimInstance);
		DynamicLink(SourceAnimInstance);

		SourceProxy.InitializeCachedClassData();
		
		FAnimationInitializeContext Context(&SourceProxy);
		InitializeSubGraph_AnyThread(Context);
	}
}

#endif
