// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuV/CustomizableObjectValidationCommandlet.h"

#include "ValidationUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Array.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuR/Model.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"


int32 UCustomizableObjectValidationCommandlet::Main(const FString& Params)
{
	// Execution arguments for commandlet from IDE
	// -run=CustomizableObjectValidation -CustomizableObject=(PathToCO)

	// Ensure we do not show any OK dialog since we are not an user that can interact with them
	GIsRunningUnattendedScript = true;
	
	// Get the package name of the CO to test
	FString CustomizableObjectAssetPath = "";
	if (!FParse::Value(*Params, TEXT("CustomizableObject="), CustomizableObjectAssetPath))
	{
		UE_LOG(LogMutable,Error,TEXT("Failed to parse Customizable Object package name from provided argument : %s"),*Params)
		return 1;
	}
	
	// Get the amount of instances to generate if parameter was provided (it will get multiplied by the amount of states later so this is a minimun value)
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

	// What platform we want to compile the CO against
	ITargetPlatform* TargetCompilationPlatform = ParseCompilationPlatform(Params);
	if (!TargetCompilationPlatform)
	{
		UE_LOG(LogMutable,Error,TEXT("No Target Compilation Platform was provided for the compilation of the CO."));
		return 1;
	}
	
	// Perform a blocking search to ensure all assets used by mutable are reachable using the AssetRegistry
	PrepareAssetRegistry();

	// Make sure there is nothing else that the engine needs to do before starting our test
	Wait(60);

	LogMutableSettings();
	
	// Compile the Customizable Object ------------------------------------------------------------------------------ //
	bool bWasCoCompilationSuccessful = false;
	{
		// Override some configurations that may have been changed by the user
		FCompilationOptions CompilationOptions = ToTestCustomizableObject->CompileOptions;
		CompilationOptions.bSilentCompilation = false;
		CompilationOptions.OptimizationLevel = 2;			// Set the optimization level to the max
		CompilationOptions.TextureCompression = ECustomizableObjectTextureCompression::Fast;

		// Set the target compilation platform based on what the caller wants
		CompilationOptions.TargetPlatform = TargetCompilationPlatform;

		// Disk cache usage for compilation operation : Override if the user provided an argument with a different value than the default one of the CO
		CompilationOptions.bUseDiskCompilation = false;
		FParse::Bool(*Params,TEXT("UseDiskCompilation="),CompilationOptions.bUseDiskCompilation);

		bWasCoCompilationSuccessful = CompileCustomizableObject(ToTestCustomizableObject, true, &CompilationOptions);
	}
	// -------------------------------------------------------------------------------------------------------------- //

	if (!bWasCoCompilationSuccessful)
	{
		UE_LOG(LogMutable,Error,TEXT("The compilation of the Customizable object was not succesfull : No instances will be generated."));
		return 1;		// Validation failed
	}

	UE_LOG(LogMutable,Display,TEXT("Customizable Object was compiled succesfully."));
	
	// GHet the total size of the streaming data of the model ---------------------------------------------- //
	{
		const TSharedPtr<const mu::Model> MutableModel = ToTestCustomizableObject->GetPrivate()->GetModel();
		check (MutableModel);

		// Roms ---------------------- //
		{
			const int32 RomCount =  MutableModel->GetRomCount();
			int64 TotalRomSizeBytes = 0;
			for (int32 RomIndex = 0; RomIndex < RomCount; RomIndex++)
			{
				const uint32 RomByteSize = MutableModel->GetRomSize(RomIndex);
				TotalRomSizeBytes += RomByteSize;
			}

			// Print MTU parseable logs
			UE_LOG(LogMutable, Log,TEXT("(int) model_rom_count : %d "), RomCount);
			UE_LOG(LogMutable, Log,TEXT("(int) model_roms_size : %lld "), TotalRomSizeBytes);
		}

		// CO embedded data size ------ //
		{
			TArray<uint8> EmbeddedDataBytes{};
			FMemoryWriter SerializationTarget{EmbeddedDataBytes, false};
		
			ToTestCustomizableObject->GetPrivate()->SaveEmbeddedData(SerializationTarget);
			const int64 COEmbeddedDataSizeBytes = EmbeddedDataBytes.Num();
		
			UE_LOG(LogMutable, Log,TEXT("(int) co_embedded_data_bytes : %lld "), COEmbeddedDataSizeBytes);
		}
	}
	
	// Skip instances updating if no instances should be updated 
	if (InstancesToGenerate <= 0)
	{
		UE_LOG(LogMutable,Display,TEXT("Instances to generate are 0 : No instances will be generated."));
		return 0;	// No instances are targeted for generation, this will be taken as compilation only test.
	}

	// Do not generate instances if the selected platform is not the running platform
	if (TargetCompilationPlatform != GetTargetPlatformManagerRef().GetRunningTargetPlatform())
	{
		UE_LOG(LogMutable,Display,TEXT("RunningPlatform != UserProvidedCompilationPlatform : No instances will be generated."));
		return 0;
	}

	// Generate target random instances to be tested ------------------------------------------------------------ //
	bool bWasInstancesCreationSuccessful = true;
	{
		// Test this parameter configuration in all the states of the CO
		const uint32 StateCount = ToTestCustomizableObject->GetStateCount();
		check(StateCount >= 1);

		UE_LOG(LogMutable, Display, TEXT("Requeted Instances Count : %i"), InstancesToGenerate);
		UE_LOG(LogMutable, Display, TEXT("State Count = %i"), StateCount);
	
		// Compute actual total amount of instances to generate
		const uint32 TotalInstancesToTestCount = InstancesToGenerate * StateCount;
		UE_LOG(LogMutable,Display,TEXT("Generating %i instances (states * requested instances)..."), TotalInstancesToTestCount);
		
		// Create randomization stream for the parameters of the instance
		FRandomStream RandomizationStream = FRandomStream(0);
		
		// Generate a series of instances to later update
		for (uint32 CustomizableObjectInstanceIndex = 0; CustomizableObjectInstanceIndex < InstancesToGenerate; CustomizableObjectInstanceIndex++)
		{
			UCustomizableObjectInstance* GeneratedInstance = ToTestCustomizableObject->CreateInstance();
			if (GeneratedInstance)
			{
				// Force generation of all LODS
				TArray<uint16> RequestedLodLevels{};
				RequestedLodLevels.Init(MAX_uint8, GeneratedInstance->GetNumComponents());
				GeneratedInstance->GetPrivate()->GetDescriptor().SetRequestedLODLevels(RequestedLodLevels);
				
				// Randomize instance values
				GeneratedInstance->SetRandomValuesFromStream(RandomizationStream);
				
				for (uint32 State = 0; State < StateCount; State++)
				{
					// Set the state for the instance and store it for later update.
					GeneratedInstance->GetPrivate()->SetState(State);
                	InstancesToProcess.Push(GeneratedInstance->Clone());
				}
			}
			else
			{
				UE_LOG(LogMutable,Error,TEXT("Failed to generate COI for the %s CO."),*ToTestCustomizableObject->GetName());
				bWasInstancesCreationSuccessful = false;
			}
		}
	}
	// ---------------------------------------------------------------------------------------------------------- //

	UE_LOG(LogMutable, Log,TEXT("(int) generated_instances_count : %u "), InstancesToProcess.Num());
	
	// Update the instances generated --------------------------------------------------------------------------- //
	UE_LOG(LogMutable,Display,TEXT("Updating generated instances..."));
	bool bInstanceFailedUpdate = false;
	const double InstancesUpdateStartSeconds = FPlatformTime::Seconds();
	{
		InstanceUpdater = NewObject<UCOIUpdater>();
		for (UCustomizableObjectInstance* InstanceToUpdate : InstancesToProcess)
		{
			if (InstanceUpdater && !InstanceUpdater->UpdateInstance(InstanceToUpdate))
			{
				bInstanceFailedUpdate = true;
			}
		}
	}
	const double InstancesUpdateEndSeconds = FPlatformTime::Seconds();
	
	// Notify and log time required by the instances to get updated
	const double CombinedInstanceUpdateSeconds = InstancesUpdateEndSeconds - InstancesUpdateStartSeconds;
	UE_LOG(LogMutable, Log,TEXT("(double) combined_update_time_ms : %f "), CombinedInstanceUpdateSeconds * 1000);

	check(InstancesToProcess.Num() > 0);
	const double AverageInstanceUpdateSeconds = CombinedInstanceUpdateSeconds / InstancesToProcess.Num();
	UE_LOG(LogMutable, Log,TEXT("(double) avg_update_time_ms : %f "), AverageInstanceUpdateSeconds * 1000);

	UE_LOG(LogMutable,Display,TEXT("Generation of Customizable object instances took %f seconds (%f seconds avg)."), CombinedInstanceUpdateSeconds, AverageInstanceUpdateSeconds);
	// ---------------------------------------------------------------------------------------------------------- //

	// Compute instance update result
	const bool bInstancesTestedSuccessfully = !bInstanceFailedUpdate && bWasInstancesCreationSuccessful;
	if (bInstancesTestedSuccessfully)
    {
        UE_LOG(LogMutable,Display,TEXT("Generation of Customizable object instances was succesfull."));
    }
    else
    {
        UE_LOG(LogMutable,Error,TEXT("The generation of Customizable object instances was not succesfull."));
    }
	
	
	// If something failed then fail the commandlet execution
	UE_LOG(LogMutable,Display,TEXT("Mutable commandlet finished."));
	return 0;
}


ITargetPlatform* UCustomizableObjectValidationCommandlet::ParseCompilationPlatform(const FString& Params) const
{
	// Get the package name of the CO to test
	FString TargetPlatformName = "";
	if (!FParse::Value(*Params, TEXT("CompilationPlatformName="), TargetPlatformName))
	{
		UE_LOG(LogMutable,Error,TEXT("Failed to parse the target compilation platformm."))
		return nullptr;
	}

	// Set the target platform in the context. For now it is the current platform.
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	check(TPM);
	
	ITargetPlatform* TargetCompilationPlatform = nullptr;
	const TArray<ITargetPlatform*> TPMPlatforms = TPM->GetTargetPlatforms();
	for (ITargetPlatform* Platform : TPMPlatforms)
	{
		FString PlatformName = Platform->PlatformName();
		if (PlatformName.Compare(TargetPlatformName) == 0)
		{
			// We have found the platform provided
			TargetCompilationPlatform = Platform;
			break;
		}
	}
	
	if (!TargetCompilationPlatform)
	{
		UE_LOG(LogMutable, Error, TEXT("Unable to relate the provided platform name (%s) with the available platforms in this machine."), *TargetPlatformName);
	}

	return TargetCompilationPlatform;
}
