// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "MLDeformerTestModel.h"
#include "MLDeformerAsset.h"
#include "MLDeformerComponent.h"
#include "MLDeformerInputInfo.h"
#include "UObject/GCObjectScopeGuard.h"
#include "PreviewScene.h"
#include "Components/SkeletalMeshComponent.h"

namespace UE::MLDeformerTests
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModelMainTest, "MLDeformer.Model.MainTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
	bool FModelMainTest::RunTest(const FString& Parameters)
	{
		using namespace UE::MLDeformer;

#if WITH_EDITORONLY_DATA
		UMLDeformerAsset* DeformerAsset = NewObject<UMLDeformerAsset>();
		FGCObjectScopeGuard AssetScopeGuard(DeformerAsset);
		UMLDeformerTestModel* TestModel = NewObject<UMLDeformerTestModel>(DeformerAsset);
		FGCObjectScopeGuard ModelScopeGuard(TestModel);
		UTEST_NOT_NULL(TEXT("DeformerAsset check"), DeformerAsset);
		UTEST_NOT_NULL(TEXT("Creating test model"), TestModel);
		UTEST_EQUAL(TEXT("Display name check"), TestModel->GetDisplayName(), FString("Test Model"));

		USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("SkeletalMesh'/MLDeformerFramework/Tests/SKM_Cylinder.SKM_Cylinder'"));
		UTEST_NOT_NULL(TEXT("Loading skeletal mesh"), SkeletalMesh);

		TestModel->SetSkeletalMesh(SkeletalMesh);
		UTEST_EQUAL(TEXT("GetSkeletalMesh check"), TestModel->GetSkeletalMesh(), SkeletalMesh);

		DeformerAsset->SetModel(TestModel);
		UTEST_TRUE(TEXT("GetModel check"), DeformerAsset->GetModel() == TestModel);

		TestModel->Init(DeformerAsset);
		UTEST_TRUE(TEXT("GetDeformerAsset check"), TestModel->GetDeformerAsset() == DeformerAsset);

		const int32 ExpectedNumImportedVerts = 1178;
		const int32 ImportedVerts = UMLDeformerModel::ExtractNumImportedSkinnedVertices(SkeletalMesh);
		UTEST_EQUAL(TEXT("Imported verts check"), ImportedVerts, ExpectedNumImportedVerts);

		TestModel->UpdateCachedNumVertices();
		UTEST_EQUAL(TEXT("Imported verts check"), TestModel->GetNumBaseMeshVerts(), ImportedVerts);

		const USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
		bool bErrorOnInvalid = false;
		const USkeleton* ModelSkeleton = TestModel->GetSkeleton(bErrorOnInvalid, nullptr);
		UTEST_EQUAL(TEXT("Skeleton check"), ModelSkeleton, Skeleton);

		// Create a preview scene and spawn an actor.
		TUniquePtr<FPreviewScene> PreviewScene = MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues());
		UWorld* World = PreviewScene->GetWorld();
		UTEST_NOT_NULL(TEXT("World check"), World);
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), FName(TEXT("ML Deformer Test Actor")));	
		AActor* Actor = World->SpawnActor<AActor>(SpawnParams);
		Actor->SetFlags(RF_Transient);
		UTEST_NOT_NULL(TEXT("Actor check"), Actor);

		// Add a skeletal mesh component.
		USkeletalMeshComponent* SkelMeshComponent = NewObject<USkeletalMeshComponent>(Actor);
		SkelMeshComponent->SetSkeletalMesh(SkeletalMesh);
		Actor->SetRootComponent(SkelMeshComponent);
		SkelMeshComponent->RegisterComponent();
		
		// Add an ML Deformer component and activate it.
		UMLDeformerComponent* MLDeformerComponent = NewObject<UMLDeformerComponent>(Actor);
		MLDeformerComponent->SetDeformerAsset(DeformerAsset);
		MLDeformerComponent->RegisterComponent();
		MLDeformerComponent->Activate();
		UTEST_EQUAL(TEXT("MLDeformerComponent weight check"), MLDeformerComponent->GetWeight(), 1.0f);
		UTEST_TRUE(TEXT("MLDeformerComponent SkelMeshComponent check"), MLDeformerComponent->GetSkeletalMeshComponent() == SkelMeshComponent);

		// Create and init the model instance.
		UMLDeformerModelInstance* ModelInstance = MLDeformerComponent->GetModelInstance();
		UTEST_NOT_NULL(TEXT("CreateModelInstance check"), ModelInstance);
		UTEST_TRUE(TEXT("ModelInstance GetModel check"), ModelInstance->GetModel() == TestModel);
		UTEST_TRUE(TEXT("ModelInstance compatible check"), ModelInstance->IsCompatible());

		// Do some input info checks.
		UMLDeformerInputInfo* InputInfo = TestModel->GetInputInfo();
		UTEST_NOT_NULL(TEXT("Get InputInfo"), InputInfo);
		UTEST_EQUAL(TEXT("InputInfo NumBaseMeshVerts check"), InputInfo->GetNumBaseMeshVertices(), 0);
		UTEST_EQUAL(TEXT("InputInfo NumBones check"), InputInfo->GetNumBones(), 0);
		UTEST_EQUAL(TEXT("InputInfo NumCurves check"), InputInfo->GetNumCurves(), 0);
#endif	// #if WITH_EDITORONLY_DATA
		return true;
	}
} // namespace UE::MLDeformerTests

#endif // WITH_DEV_AUTOMATION_TESTS
