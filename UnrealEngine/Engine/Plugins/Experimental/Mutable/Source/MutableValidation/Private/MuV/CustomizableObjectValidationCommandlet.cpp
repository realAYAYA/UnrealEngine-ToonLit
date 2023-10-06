// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuV/CustomizableObjectValidationCommandlet.h"

#include "Containers/Array.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectSystem.h"


int32 UCustomizableObjectValidationCommandlet::Main(const FString& Params)
{
	// Execution arguments for commandlet from IDE
	// $(LocalDebuggerCommandArguments) -run=CustomizableObjectValidation -CustomizableObject=(PathToCO)
	
	// Get the package name of the CO to test
	FString CustomizableObjectAssetPath = "";
	if (!FParse::Value(*Params, TEXT("CustomizableObject="), CustomizableObjectAssetPath))
	{
		UE_LOG(LogMutable,Error,TEXT("Failed to parse Customizable Object package name from provided argument : %s"),*Params)
		return 1;
	}
	
	// Get the amount of instances to generate if parameter was provided
	uint32 InstancesToGenerate = 16;
	if (!FParse::Value(*Params, TEXT("InstanceGenerationCount="),InstancesToGenerate))
	{
		UE_LOG(LogMutable,Display,TEXT("Instance generation count not specified. Using default value : %u"),InstancesToGenerate);
	}
	
	// Load the resource
	UObject* FoundObject = FSoftObjectPath(CustomizableObjectAssetPath).TryLoad();
	if (!FoundObject)
	{
		UE_LOG(LogMutable,Error,TEXT("Failed to retrieve UObject from path %s"),*CustomizableObjectAssetPath);
		return 1;
	}
	
	// Get the CustomizableObject.
	ToTestCustomizableObject = Cast<UCustomizableObject>(FoundObject);
	if (!ToTestCustomizableObject)
	{
		UE_LOG(LogMutable,Error,TEXT("Failed to cast found UObject to UCustomizableObject."));
		return 1;
	}
	
	// Compile the Customizable Object ------------------------------------------------------------------------------ //
	bool bWasCoCompilationSuccesfull = false;
	{
		UE_LOG(LogMutable,Display,TEXT("Compiling Customizable Object..."));
    	
    	// Request a compiler to be able to locate the root and to compile it
    	const TUniquePtr<FCustomizableObjectCompilerBase> Compiler =
    		TUniquePtr<FCustomizableObjectCompilerBase>(UCustomizableObjectSystem::GetInstance()->GetNewCompiler());
    		
    	// Compile the CO with the provided compilation options
    	// Run Sync compilation -> Warning : Potentially long operation -------------
    	Compiler->Compile(*ToTestCustomizableObject, ToTestCustomizableObject->CompileOptions, false);
    	// --------------------------------------------------------------------------
    		
    	// Get the compilation result
    	const ECustomizableObjectCompilationState CompilationEndResult = Compiler->GetCompilationState();
    	check(CompilationEndResult != ECustomizableObjectCompilationState::None);
    	check(CompilationEndResult != ECustomizableObjectCompilationState::InProgress);
    	
    	bWasCoCompilationSuccesfull = CompilationEndResult == ECustomizableObjectCompilationState::Completed;
	}
	// -------------------------------------------------------------------------------------------------------------- //
	
	if (bWasCoCompilationSuccesfull)
	{
		UE_LOG(LogMutable,Display,TEXT("Customizable Object was compiled succesfully."));
				
		// Generate target random instances to be tested ------------------------------------------------------------ //
		bool bWasInstancesCreationSuccessful = true;
		{
			UE_LOG(LogMutable,Display,TEXT("Generating %i random instances..."), InstancesToGenerate);	
			
			// Generate a series of instances to later update
			for (uint32 CustomizableObjectInstanceIndex = 0; CustomizableObjectInstanceIndex < InstancesToGenerate; CustomizableObjectInstanceIndex++)
			{
				UCustomizableObjectInstance* GeneratedInstance = ToTestCustomizableObject->CreateInstance();
				if (GeneratedInstance)
				{
					// Randomize instance values
					GeneratedInstance->SetRandomValues(0);
					InstancesToProcess.Push(GeneratedInstance);
				}
				else
				{
					UE_LOG(LogMutable,Error,TEXT("Failed to generate COI for Customizable Object with name : %s ."),*ToTestCustomizableObject->GetName());
					bWasInstancesCreationSuccessful = false;
				}
			}
		}
		// ---------------------------------------------------------------------------------------------------------- //
		
		// Update the instances generated --------------------------------------------------------------------------- //
		{
			UE_LOG(LogMutable,Display,TEXT("Updating generated instances..."));
            	
            // Now update the instances one by one
            while (!InstancesToProcess.IsEmpty() || InstanceBeingUpdated)
            {
            	// Tick the engine
            	CommandletHelpers::TickEngine();
    
            	// Stop if exit was requested
            	if (IsEngineExitRequested())
            	{
            		break;
            	}
            
            	// Wait until current instance turns invalid
            	if (InstanceBeingUpdated)
            	{
            		continue;
            	}
           
            	// We may have already updated the instance so ensure we have more instances to work with
            	if (InstancesToProcess.IsEmpty())
            	{
            		continue;
            	}
            	
            	InstanceBeingUpdated = InstancesToProcess[0];
            	InstancesToProcess.RemoveAt(0);
            	if (InstanceBeingUpdated)
            	{
            		UE_LOG(LogMutable,Display,TEXT("Invoking update for %s instance."),*InstanceBeingUpdated->GetName());
            		InstanceUpdateDelegate.BindDynamic(this, &UCustomizableObjectValidationCommandlet::OnInstanceUpdate);
            		InstanceBeingUpdated->UpdateSkeletalMeshAsyncResult(InstanceUpdateDelegate);
            	}
            }
		}	
		// ---------------------------------------------------------------------------------------------------------- //

		// Compute instance update result
		const bool bInstancesTestedSuccessfully = !bInstanceFailedUpdate && bWasInstancesCreationSuccessful;
        if (bInstancesTestedSuccessfully)
        {
        	UE_LOG(LogMutable,Display,TEXT("Generation of Customizable object instances was succesfull."));
        }
        else
        {
        	UE_LOG(LogMutable,Error,TEXT("The generation of Customizable object instances  was not succesfull."));
        }
	}
	else
	{
		UE_LOG(LogMutable,Error,TEXT("The compilation of the Customizable object was not succesfull."));
	}
	
	
	// If something failed then fail the commandlet execution
	UE_LOG(LogMutable,Display,TEXT("Mutable commandlet finished."));
	return 0;
}


void UCustomizableObjectValidationCommandlet::OnInstanceUpdate(const FUpdateContext& Result)
{
	const EUpdateResult InstanceUpdateResult = Result.UpdateResult;
	if (InstanceUpdateResult == EUpdateResult::Success)
	{
		UE_LOG(LogMutable,Display,TEXT("Instance %s finished update succesfully."),*InstanceBeingUpdated->GetName());
	}
	else
	{
		const FString OutputStatus = UEnum::GetValueAsString(Result.UpdateResult);
		UE_LOG(LogMutable,Error,TEXT("Instance %s finished update with anomalous state : %s."),*InstanceBeingUpdated->GetName(),*OutputStatus);
		bInstanceFailedUpdate = true;
	}
	
	InstanceUpdateDelegate.Unbind();
	InstanceBeingUpdated = nullptr;
}

