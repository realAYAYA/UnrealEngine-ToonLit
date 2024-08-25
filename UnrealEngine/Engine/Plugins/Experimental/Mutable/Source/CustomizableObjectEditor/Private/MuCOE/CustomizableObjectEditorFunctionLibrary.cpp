// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectEditorFunctionLibrary.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObject.h"

ECustomizableObjectCompilationState UCustomizableObjectEditorFunctionLibrary::CompileCustomizableObjectSynchronously(
	UCustomizableObject* CustomizableObject,
	ECustomizableObjectOptimizationLevel InOptimizationLevel,
	ECustomizableObjectTextureCompression InTextureCompression)
{
	if (!CustomizableObject)
	{
		UE_LOG(LogMutable, Warning, TEXT("Attempted to compile nullptr Customizable Object"));
		return ECustomizableObjectCompilationState::Failed;
	}

	FString ObjectPath = CustomizableObject->GetPathName();
	if (CustomizableObject->GetPrivate()->IsLocked())
	{
		// Take this if you need a hack:
		// UCustomizableObjectSystem::GetInstance()->UnlockObject(CustomizableObject);
		UE_LOG( LogMutable, Warning, 
			TEXT("Attempted to compile %s Customizable Object when it is locked"),
			*ObjectPath);
		return ECustomizableObjectCompilationState::Failed;
	}

	// store package dirty state so that we can restore it - compile is not an edit:
	const bool bPackageWasDirty = CustomizableObject->GetOutermost()->IsDirty();

	const double StartTime = FPlatformTime::Seconds();

	FCustomizableObjectCompiler Compiler;
	FCompilationOptions Options = CustomizableObject->CompileOptions;
	Options.OptimizationLevel = static_cast<int32>(InOptimizationLevel);
	Options.TextureCompression = InTextureCompression;
	Options.bSilentCompilation = false;
	const bool bAsync = false;
	Compiler.Compile(*CustomizableObject, Options, bAsync);

	CustomizableObject->GetOutermost()->SetDirtyFlag(bPackageWasDirty);

	const double CurrentTime = FPlatformTime::Seconds();
	UE_LOG( LogMutable, Display,
		TEXT("Synchronously Compiled %s %s in %f seconds"),
		*ObjectPath, 
		Compiler.GetCompilationState() == ECustomizableObjectCompilationState::Completed ? 
		TEXT("successfully") : TEXT("unsuccessfully"),
		CurrentTime - StartTime
	);

	if (!CustomizableObject->IsCompiled())
	{
		UE_LOG(LogMutable, Warning, TEXT("CO not marked as compiled"));
	}

	return Compiler.GetCompilationState();
}
