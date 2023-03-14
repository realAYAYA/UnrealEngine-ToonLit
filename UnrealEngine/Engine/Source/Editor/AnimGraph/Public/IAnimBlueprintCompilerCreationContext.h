// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Templates/SubclassOf.h"

class UObject;
class UClass;
class IAnimBlueprintCompilationContext;
class IAnimBlueprintCompilationBracketContext;
class IAnimBlueprintPostExpansionStepContext;
class IAnimBlueprintCopyTermDefaultsContext;
class IAnimBlueprintGeneratedClassCompiledData;
class UAnimGraphNode_Base;
class UEdGraph;
class UEdGraphSchema;

UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnStartCompilingClass, const UClass* /*InClass*/, IAnimBlueprintCompilationBracketContext& /*InCompilerContext*/, IAnimBlueprintGeneratedClassCompiledData& /*OutCompiledData*/);

UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPreProcessAnimationNodes, TArrayView<UAnimGraphNode_Base*> /*InAnimNodes*/, IAnimBlueprintCompilationContext& /*InCompilerContext*/, IAnimBlueprintGeneratedClassCompiledData& /*OutCompiledData*/);

UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPostProcessAnimationNodes, TArrayView<UAnimGraphNode_Base*> /*InAnimNodes*/, IAnimBlueprintCompilationContext& /*InCompilerContext*/, IAnimBlueprintGeneratedClassCompiledData& /*OutCompiledData*/);

UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPostExpansionStep, const UEdGraph* /*InGraph*/, IAnimBlueprintPostExpansionStepContext& /*InCompilerContext*/, IAnimBlueprintGeneratedClassCompiledData& /*OutCompiledData*/);

UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFinishCompilingClass, const UClass* /*InClass*/, IAnimBlueprintCompilationBracketContext& /*InCompilerContext*/, IAnimBlueprintGeneratedClassCompiledData& /*OutCompiledData*/);

UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCopyTermDefaultsToDefaultObject, UObject* /*InDefaultObject*/, IAnimBlueprintCopyTermDefaultsContext& /*InCompilerContext*/, IAnimBlueprintGeneratedClassCompiledData& /*OutCompiledData*/);

/** Interface to the creation of an anim BP compiler context */
class ANIMGRAPH_API IAnimBlueprintCompilerCreationContext
{
public:
	virtual ~IAnimBlueprintCompilerCreationContext() {}

	/** Registers a graphs schema class to the anim BP compiler so that default function processing is not performed on it */
	virtual void RegisterKnownGraphSchema(TSubclassOf<UEdGraphSchema> InGraphSchemaClass) = 0;
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
	virtual FOnStartCompilingClass& OnStartCompilingClass() { static FOnStartCompilingClass Dummy; return Dummy; }

	UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
	virtual FOnPreProcessAnimationNodes& OnPreProcessAnimationNodes() { static FOnPreProcessAnimationNodes Dummy; return Dummy; }

	UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
	virtual FOnPostProcessAnimationNodes& OnPostProcessAnimationNodes() { static FOnPostProcessAnimationNodes Dummy; return Dummy; }

	UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
	virtual FOnPostExpansionStep& OnPostExpansionStep() { static FOnPostExpansionStep Dummy; return Dummy; }

	UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
	virtual FOnFinishCompilingClass& OnFinishCompilingClass() { static FOnFinishCompilingClass Dummy; return Dummy; }

	UE_DEPRECATED(5.0, "Anim BP compiler delegate system is deprecated. Use UAnimBlueprintExtension instead")
	virtual FOnCopyTermDefaultsToDefaultObject& OnCopyTermDefaultsToDefaultObject() { static FOnCopyTermDefaultsToDefaultObject Dummy; return Dummy; }
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

