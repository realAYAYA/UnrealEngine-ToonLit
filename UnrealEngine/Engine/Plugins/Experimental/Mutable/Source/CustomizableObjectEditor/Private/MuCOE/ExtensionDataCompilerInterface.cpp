// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/ExtensionDataCompilerInterface.h"

#include "InstancedStruct.h"
#include "MuCO/CustomizableObjectStreamedResourceData.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuR/ExtensionData.h"

FExtensionDataCompilerInterface::FExtensionDataCompilerInterface(FMutableGraphGenerationContext& InGenerationContext)
	: GenerationContext(InGenerationContext)
{
}

mu::ExtensionDataPtrConst FExtensionDataCompilerInterface::MakeStreamedExtensionData(UCustomizableObjectResourceDataContainer*& OutContainer)
{
	mu::ExtensionDataPtr Result = new mu::ExtensionData;
	Result->Origin = mu::ExtensionData::EOrigin::ConstantStreamed;
	Result->Index = GenerationContext.StreamedExtensionData.Num();

	// Generate a deterministic name to help with deterministic cooking
	const FString ContainerName = FString::Printf(TEXT("Streamed_%d"), Result->Index);

	UObject* ExistingObject = FindObject<UObject>(GenerationContext.Object, *ContainerName);
	if (ExistingObject)
	{
		// This must have been left behind from a previous compilation and hasn't been deleted by 
		// GC yet.
		//
		// Move it into the transient package to get it out of the way.
		ExistingObject->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);

		check(!FindObject<UObject>(GenerationContext.Object, *ContainerName));
	}

	check(GenerationContext.Object);
	OutContainer = NewObject<UCustomizableObjectResourceDataContainer>(
		GenerationContext.Object,
		FName(*ContainerName),
		RF_Public);

	GenerationContext.StreamedExtensionData.Add(OutContainer);

	return Result;
}

mu::ExtensionDataPtrConst FExtensionDataCompilerInterface::MakeAlwaysLoadedExtensionData(FInstancedStruct&& Data)
{
	mu::ExtensionDataPtr Result = new mu::ExtensionData;
	Result->Origin = mu::ExtensionData::EOrigin::ConstantAlwaysLoaded;
	Result->Index = GenerationContext.AlwaysLoadedExtensionData.Num();

	FCustomizableObjectResourceData* CompileTimeExtensionData = &GenerationContext.AlwaysLoadedExtensionData.AddDefaulted_GetRef();
	CompileTimeExtensionData->Data = MoveTemp(Data);

	return Result;
}

UObject* FExtensionDataCompilerInterface::GetOuterForAlwaysLoadedObjects()
{
	check(GenerationContext.Object);
	return GenerationContext.Object;
}

void FExtensionDataCompilerInterface::AddGeneratedNode(const UCustomizableObjectNode* InNode)
{
	check(InNode);

	// A const_cast here is required because the new node needs to be added in the GeneratedNodes list so mutable can
	// discover new parameters that can potentially be attached to the extension node, however, this
	// function is called as ICustomizableObjectExtensionNode::GenerateMutableNode(this), so we need to cast the const away here.
	// Decided to do the case here so the use of AddGeneratedNode is as clean as possible
	GenerationContext.GeneratedNodes.Add(const_cast<UCustomizableObjectNode*>(InNode));
}

void FExtensionDataCompilerInterface::CompilerLog(const FText& InLogText, const UCustomizableObjectNode* InNode)
{
	GenerationContext.Compiler->CompilerLog(InLogText, InNode);
}
