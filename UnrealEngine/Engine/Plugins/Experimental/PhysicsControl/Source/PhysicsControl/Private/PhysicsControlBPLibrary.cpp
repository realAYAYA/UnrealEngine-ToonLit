// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlBPLibrary.h"

#include "RigidBodyControlData.h"
#include "AnimNode_RigidBodyWithControl.h"

//======================================================================================================================
template<typename TParameterType> const TArray<FName>* FindNamesInSet(
	const FPhysicsControlNameRecords& NameRecords, const FName SetName)
{
	return nullptr;
}

//======================================================================================================================
template<> const TArray<FName>* FindNamesInSet<FPhysicsControl>(
	const FPhysicsControlNameRecords& NameRecords, const FName SetName)
{
	return NameRecords.ControlSets.Find(SetName);
}

//======================================================================================================================
template<> const TArray<FName>* FindNamesInSet<FPhysicsBodyModifier>(
	const FPhysicsControlNameRecords& NameRecords, const FName SetName)
{
	return NameRecords.BodyModifierSets.Find(SetName);
}

//======================================================================================================================
template<typename TParameterType> TArray<FName> GetNamesInSet(
	const FRigidBodyWithControlReference& RigidBodyWithControl, const FName SetName)
{
	TArray<FName> OutputNames;

	RigidBodyWithControl.CallAnimNodeFunction<FAnimNode_RigidBodyWithControl>(
		TEXT("GetControlNamesInSet"),
		[&OutputNames, SetName](FAnimNode_RigidBodyWithControl& InRigidBodyWithControl)
		{
			if (const TArray<FName>* FoundNames = FindNamesInSet<TParameterType>(
				InRigidBodyWithControl.GetNameRecords(), SetName))
			{
				OutputNames = *FoundNames;
			}
		});

	return OutputNames;
}

//======================================================================================================================
template<typename TNamedParameters> void InterpolateParametersContainers(
	TArray<TNamedParameters>& TargetContainer, const TArray<TNamedParameters>& SourceContainer, const float Weight)
{
	for (const TNamedParameters& Source : SourceContainer)
	{
		const FName SourceName = Source.Name;
		if (TNamedParameters* const ExistingUpdate = TargetContainer.FindByPredicate(
			[SourceName](const TNamedParameters& Element) { return Element.Name == SourceName; }))
		{
			// Interpolate values if the target container includes an element with the source values name.
			ExistingUpdate->Data = Interpolate(ExistingUpdate->Data, Source.Data, Weight);
		}
		else
		{
			// Add this element to the target container.
			TargetContainer.Add(Source);
		}
	}
}

//======================================================================================================================
template<typename TNamedParameters> void BlendParametersThroughSet(
	const FPhysicsControlControlAndModifierParameters& InParametersContainer, 
	const TNamedParameters&                            InStartParameters,
	const TNamedParameters&                            InEndParameters,
	const TArray<FName>&                               Names,
	FPhysicsControlControlAndModifierParameters&       OutParametersContainer)
{
	// TODO - Limbs can include branches. Would need a more sophisticated approach to deal with
	// those properly, perhaps include a depth index in the control name ?

	OutParametersContainer = InParametersContainer;

	const float WeightDelta = 1.0f / static_cast<float>(Names.Num() - 1);
	float Weight = 0.0f;

	for (const FName& Name : Names)
	{
		OutParametersContainer.Add(
			TNamedParameters(Name, Interpolate(InStartParameters.Data, InEndParameters.Data, Weight)));
		Weight += WeightDelta;
	}
}

//======================================================================================================================
void UPhysicsControlBPLibrary::AddControlParameters(
	UPARAM(ref) FPhysicsControlControlAndModifierParameters& InParametersContainer,
	FPhysicsControlControlAndModifierParameters&             OutParametersContainer,
	const FName                                              Name, 
	const FPhysicsControlSparseData&                         ControlData)
{
	OutParametersContainer = InParametersContainer;
	OutParametersContainer.Add(FPhysicsControlNamedControlParameters(Name, ControlData));
}

//======================================================================================================================
void UPhysicsControlBPLibrary::AddMultipleControlParameters(
	UPARAM(ref) FPhysicsControlControlAndModifierParameters& InParametersContainer,
	FPhysicsControlControlAndModifierParameters&             OutParametersContainer,
	const TArray<FName>&                                     Names,
	const FPhysicsControlSparseData&                         ControlData)
{
	OutParametersContainer = InParametersContainer;
	OutParametersContainer.ControlParameters.Reserve(OutParametersContainer.ControlParameters.Num() + Names.Num());

	for (const FName Name : Names)
	{
		OutParametersContainer.Add(FPhysicsControlNamedControlParameters(Name, ControlData));
	}	
}

//======================================================================================================================
void UPhysicsControlBPLibrary::AddModifierParameters(
	UPARAM(ref) FPhysicsControlControlAndModifierParameters& InParametersContainer,
	FPhysicsControlControlAndModifierParameters&             OutParametersContainer,
	const FName                                              Name,
	const FPhysicsControlModifierSparseData&                 ModifierData)
{
	OutParametersContainer = InParametersContainer;
	OutParametersContainer.Add(FPhysicsControlNamedModifierParameters(Name, ModifierData));
}

//======================================================================================================================
void UPhysicsControlBPLibrary::AddMultipleModifierParameters(
	UPARAM(ref) FPhysicsControlControlAndModifierParameters& InParametersContainer,
	FPhysicsControlControlAndModifierParameters&             OutParametersContainer,
	const TArray<FName>&                                     Names,
	const FPhysicsControlModifierSparseData&                 ModifierData)
{
	OutParametersContainer = InParametersContainer;
	OutParametersContainer.ModifierParameters.Reserve(OutParametersContainer.ModifierParameters.Num() + Names.Num());

	for (const FName Name : Names)
	{
		OutParametersContainer.Add(FPhysicsControlNamedModifierParameters(Name, ModifierData));
	}
}

//======================================================================================================================
void UPhysicsControlBPLibrary::BlendParameters(
	UPARAM(ref) FPhysicsControlControlAndModifierParameters& InParametersContainerA, 
	UPARAM(ref) FPhysicsControlControlAndModifierParameters& InParametersContainerB, 
	const float                                              InInterpolationWeight, 
	FPhysicsControlControlAndModifierParameters&             OutParametersContainers)
{
	OutParametersContainers = InParametersContainerA;

	InterpolateParametersContainers(
		OutParametersContainers.ControlParameters, InParametersContainerB.ControlParameters, InInterpolationWeight);
	InterpolateParametersContainers(
		OutParametersContainers.ModifierParameters, InParametersContainerB.ModifierParameters, InInterpolationWeight);
}

//======================================================================================================================
void UPhysicsControlBPLibrary::BlendControlParametersThroughSet(
	UPARAM(ref) FPhysicsControlControlAndModifierParameters& InParametersContainer,
	UPARAM(ref) const FPhysicsControlNamedControlParameters& InStartParameters,
	UPARAM(ref) const FPhysicsControlNamedControlParameters& InEndParameters,
	const TArray<FName>&                                     InNames,
	FPhysicsControlControlAndModifierParameters&             OutParametersContainer)
{
	BlendParametersThroughSet(InParametersContainer, InStartParameters, InEndParameters, InNames, OutParametersContainer);
}

//======================================================================================================================
void UPhysicsControlBPLibrary::BlendModifierParametersThroughSet(
	UPARAM(ref) FPhysicsControlControlAndModifierParameters&  InParametersContainer, 
	UPARAM(ref) const FPhysicsControlNamedModifierParameters& InStartParameters,
	UPARAM(ref) const FPhysicsControlNamedModifierParameters& InEndParameters,
	const TArray<FName>&                                      InNames,
	FPhysicsControlControlAndModifierParameters&              OutParametersContainer)
{
	BlendParametersThroughSet(InParametersContainer, InStartParameters, InEndParameters, InNames, OutParametersContainer);
}

//======================================================================================================================
FRigidBodyWithControlReference UPhysicsControlBPLibrary::ConvertToRigidBodyWithControl(
	const FAnimNodeReference&           Node,
	EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FRigidBodyWithControlReference>(Node, Result);
}

//======================================================================================================================
FRigidBodyWithControlReference UPhysicsControlBPLibrary::SetOverridePhysicsAsset(
	const FRigidBodyWithControlReference& Node, UPhysicsAsset* PhysicsAsset)
{
	Node.CallAnimNodeFunction<FAnimNode_RigidBodyWithControl>(
		TEXT("SetOverridePhysicsAsset"),
		[PhysicsAsset](FAnimNode_RigidBodyWithControl& Node)
		{
			Node.SetOverridePhysicsAsset(PhysicsAsset);
		});

	return Node;
}

//======================================================================================================================
UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
TArray<FName> UPhysicsControlBPLibrary::GetControlNamesInSet(
	const FRigidBodyWithControlReference& RigidBodyWithControl, 
	const FName                           SetName)
{
	return GetNamesInSet<FPhysicsControl>(RigidBodyWithControl, SetName);
}

//======================================================================================================================
UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
TArray<FName> UPhysicsControlBPLibrary::GetBodyModifierNamesInSet(
	const FRigidBodyWithControlReference& RigidBodyWithControl,
	const FName                           SetName)
{
	return GetNamesInSet<FPhysicsBodyModifier>(RigidBodyWithControl, SetName);
}

