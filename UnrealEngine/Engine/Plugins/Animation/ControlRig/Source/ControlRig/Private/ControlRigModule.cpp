// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigModule.h"

#include "ControlRigGizmoActor.h"
#include "TransformableRegistry.h"
#include "Modules/ModuleManager.h"
#include "Sequencer/ControlRigObjectSpawner.h"
#include "ILevelSequenceModule.h"
#include "ControlRigObjectVersion.h"
#include "Constraints/ControlRigTransformableHandle.h"
#include "UObject/DevObjectVersion.h"
#include "ControlRig.h"

// Unique Control Rig Object version id
const FGuid FControlRigObjectVersion::GUID(0xA7820CFB, 0x20A74359, 0x8C542C14, 0x9623CF50);
// Register AnimPhys custom version with Core
FDevVersionRegistration GRegisterControlRigObjectVersion(FControlRigObjectVersion::GUID, FControlRigObjectVersion::LatestVersion, TEXT("Dev-ControlRig"));

void FControlRigModule::StartupModule()
{
	ILevelSequenceModule& LevelSequenceModule = FModuleManager::LoadModuleChecked<ILevelSequenceModule>("LevelSequence");
	OnCreateMovieSceneObjectSpawnerHandle = LevelSequenceModule.RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner::CreateStatic(&FControlRigObjectSpawner::CreateObjectSpawner));

	ManipulatorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/ControlRig/M_Manip.M_Manip"));

	RegisterTransformableCustomization();
}

void FControlRigModule::ShutdownModule()
{
	ILevelSequenceModule* LevelSequenceModule = FModuleManager::GetModulePtr<ILevelSequenceModule>("LevelSequence");
	if (LevelSequenceModule)
	{
		LevelSequenceModule->UnregisterObjectSpawner(OnCreateMovieSceneObjectSpawnerHandle);
	}
}

void FControlRigModule::RegisterTransformableCustomization() const
{
	// Register UTransformableControlHandle and has function to handle control thru the transform constraints system
	// as AControlRigShapeActor is only available if the ControlRig plugin is loaded.
	auto CreateControlHandleFunc = [](const UObject* InObject, UObject* Outer)->UTransformableHandle*
	{
		if (const AControlRigShapeActor* ControlActor = Cast<AControlRigShapeActor>(InObject))
		{
			if (UControlRig* ControlRig = ControlActor->ControlRig.Get())
			{
				UTransformableControlHandle* CtrlHandle = ControlRig->CreateTransformableControlHandle(Outer, ControlActor->ControlName);
				return CtrlHandle;
			}
		}
		return nullptr;
	};

	auto GetControlHashFunc = [](const UObject* InObject)->uint32
	{
		if (const AControlRigShapeActor* ControlActor = Cast<AControlRigShapeActor>(InObject))
		{
			const uint32 ControlHash = UTransformableControlHandle::ComputeHash(
				ControlActor->ControlRig.Get(), ControlActor->ControlName);
			return ControlHash;
		}
		return 0;
	};

	FTransformableRegistry& Registry = FTransformableRegistry::Get();
	Registry.Register( AControlRigShapeActor::StaticClass(), CreateControlHandleFunc, GetControlHashFunc);
}

IMPLEMENT_MODULE(FControlRigModule, ControlRig)
