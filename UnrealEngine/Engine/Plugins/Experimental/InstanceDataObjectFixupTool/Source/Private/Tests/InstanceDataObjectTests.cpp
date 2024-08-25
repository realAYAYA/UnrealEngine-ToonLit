// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataObjectTests.h"

#include "CoreTypes.h"
#include "InstanceDataObjectFixupToolModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Templates/Function.h"
#include "Async/Async.h"
#include "UObject/PropertyBagRepository.h"
#include "UObject/PropertyPathName.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "UnrealEngine.h"
#include "DetailsViewArgs.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "IDetailsView.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "PropertyBagTests"

static UPackage* CreateTestPackage(const UClass* Class)
{
	const FString UniqueAssetName = Class->GetName() + TEXT("_") + FGuid::NewGuid().ToString();
    return CreatePackage(*(TEXT("/Temp/") + UniqueAssetName));
}

template<typename T>
static UPackage* CreateTestPackage()
{
    return CreateTestPackage(T::StaticClass());
}

static UTestReportCardV1* CreateTestObjectV1()
{
	UTestReportCardV1* Result = NewObject<UTestReportCardV1>(CreateTestPackage<UTestReportCardV1>());
    Result->Name = TEXT("John Doe");
    Result->Age = 11;
    Result->Gender = StudentGender::Male;
	Result->Grade = 82.875f;
	return Result;
}

static UTestReportCardV2* CreateTestObjectV2()
{
	UTestReportCardV2* Result = NewObject<UTestReportCardV2>(CreateTestPackage<UTestReportCardV2>());
    Result->Name = TEXT("John Doe");
    Result->Age = 11;
    Result->Gender = StudentGender::Male;
	Result->GPA = 82.875f;
	Result->MathGrade = 75.5f;
	Result->MathNotes = {
		TEXT("Lacks initiative"),
	};
	Result->ScienceGrade = 100.f;
	Result->ScienceNotes = {
		TEXT("I love having John in my class"),
	};
	Result->ArtGrade = 95.f;
	Result->ArtNotes = {};
	Result->EnglishGrade = 61.f;
	Result->EnglishNotes = {
		TEXT("John doesn't do his homework"),
		TEXT("See me after class!"),
	};
	return Result;
}

static UTestReportCardV3* CreateTestObjectV3()
{
	UTestReportCardV3* Result = NewObject<UTestReportCardV3>(CreateTestPackage<UTestReportCardV3>());
    Result->StudentName = TEXT("John Doe");
    Result->Age = 11;
    Result->Gender = TEXT("Male");
	Result->GPA = 82.875f;
    Result->MathReport = {
    	.Name = TEXT("Math"),
    	.Grade = 75.5f,
    	.TeacherNotes = {
    		TEXT("Lacks initiative"),
    	},
    };
    Result->ScienceReport = {
    	.Name = TEXT("Science"),
    	.Grade = 100.f,
    	.TeacherNotes = {
    		TEXT("I love having John in my class"),
    	},
    };
    Result->ArtReport = {
    	.Name = TEXT("Art"),
    	.Grade = 95.f,
    	.TeacherNotes = {},
    };
    Result->EnglishReport = {
    	.Name = TEXT("English"),
    	.Grade = 61.f,
    	.TeacherNotes = {
    		TEXT("John doesn't do his homework"),
    		TEXT("See me after class!"),
    	},
    };
	return Result;
}

static UTestReportCardV4* CreateTestObjectV4()
{
	UTestReportCardV4* Result = NewObject<UTestReportCardV4>(CreateTestPackage<UTestReportCardV4>());
    Result->StudentName = TEXT("John Doe");
    Result->Age = 11;
    Result->Gender = TEXT("Male");
	Result->GPA = 82.875f;
	Result->ClassGradesArray = {
		{
			.Name = TEXT("Math"),
			.Grade = 75.5f,
			.TeacherNotes = {
				TEXT("Lacks initiative"),
			},
			.MainTeacherName = FString(TEXT("Samantha"))
		},
		{
			.Name = TEXT("Science"),
			.Grade = 100.f,
			.TeacherNotes = {
				TEXT("I love having John in my class"),
			},
			.MainTeacherName = FString(TEXT("Mark"))
		},
		{
			.Name = TEXT("Art"),
			.Grade = 95.f,
			.TeacherNotes = {},
			.MainTeacherName = FString(TEXT("Frank"))
		},
		{
			.Name = TEXT("English"),
			.Grade = 61.f,
			.TeacherNotes = {
				TEXT("John doesn't do his homework"),
				TEXT("See me after class!"),
			}
		}
	};
    Result->ClassGrades.Add(TEXT("Math"), {
    	.Name = TEXT("Math"),
    	.Grade = 75.5f,
    	.TeacherNotes = {
    		TEXT("Lacks initiative"),
    	},
		.MainTeacherName = FString(TEXT("Samantha"))
    });
    Result->ClassGrades.Add(TEXT("Science"), {
    	.Name = TEXT("Science"),
    	.Grade = 100.f,
    	.TeacherNotes = {
    		TEXT("I love having John in my class"),
    	},
		.MainTeacherName = FString(TEXT("Mark"))
    });
    Result->ClassGrades.Add(TEXT("Art"), {
    	.Name = TEXT("Art"),
    	.Grade = 95.f,
    	.TeacherNotes = {},
		.MainTeacherName = FString(TEXT("Frank"))
    });
    Result->ClassGrades.Add(TEXT("English"), {
    	.Name = TEXT("English"),
    	.Grade = 61.f,
    	.TeacherNotes = {
    		TEXT("John doesn't do his homework"),
    		TEXT("See me after class!"),
    	},
    });
	return Result;
}

static UTestReportCardV5* CreateTestObjectV5()
{
	UTestReportCardV5* Result = NewObject<UTestReportCardV5>(CreateTestPackage<UTestReportCardV5>());
    Result->StudentName = TEXT("John Doe");
    Result->Age = 11;
    Result->Gender = TEXT("Male");
	Result->GPA = 82.875f;
    Result->ClassGrades.Add(TEXT("Math"), {
    	.ClassName = TEXT("Math"),
    	.Grade = 75.5f,
    	.TeacherNotes = {
    		TEXT("Lacks initiative"),
    	},
    });
    Result->ClassGrades.Add(TEXT("Science"), {
    	.ClassName = TEXT("Science"),
    	.Grade = 100.f,
    	.TeacherNotes = {
    		TEXT("I love having John in my class"),
    	},
    });
    Result->ClassGrades.Add(TEXT("Art"), {
    	.ClassName = TEXT("Art"),
    	.Grade = 95.f,
    	.TeacherNotes = {},
    });
    Result->ClassGrades.Add(TEXT("English"), {
    	.ClassName = TEXT("English"),
    	.Grade = 61.f,
    	.TeacherNotes = {
    		TEXT("John doesn't do his homework"),
    		TEXT("See me after class!"),
    	},
    });
	return Result;
}

static UTestReportCardV6* CreateTestObjectV6()
{
	UTestReportCardV6* Result = NewObject<UTestReportCardV6>(CreateTestPackage<UTestReportCardV6>());
	Result->StudentInfo = NewObject<UStudentInfoV1>(Result);
	Result->StudentInfo->StudentName = TEXT("John Doe");
	Result->StudentInfo->Age = 11;
	Result->StudentInfo->Gender = TEXT("Male");
	Result->StudentInfo->GradeYear = TEXT("6th Grade");
	Result->Grade = 82.875f;
	return Result;
}

static UTestReportCardV7* CreateTestObjectV7()
{
	UTestReportCardV7* Result = NewObject<UTestReportCardV7>(CreateTestPackage<UTestReportCardV7>());
	Result->StudentInfo = NewObject<UStudentInfoV2>(Result);
	Result->StudentInfo->StudentName = TEXT("John Doe");
	Result->StudentInfo->Age = 11;
	Result->StudentInfo->Gender = TEXT("Male");
	Result->GPA = 82.875f;
	return Result;
}


// hard coded mapping that allows us to treat different versions of a class as if they are the same class
TMap<FString, FSoftObjectPath> GetExportClasses(const UClass* Class)
{
	TMap<FString, FSoftObjectPath> Result = {{TEXT("Self"), Class}};
	
	if (Class == UTestReportCardV6::StaticClass())
	{
		Result.Add({TEXT("StudentInfo"), UStudentInfoV1::StaticClass()});
	}
	else if (Class == UTestReportCardV7::StaticClass())
	{
		Result.Add({TEXT("StudentInfo"), UStudentInfoV2::StaticClass()});
	}
	
	return Result;
}

TArray<FCoreRedirect> BuildTestRedirects(const UClass* OldClass, const UClass* NewClass)
{
	TMap<FString, FSoftObjectPath> OldExportClasses = GetExportClasses(OldClass);
	TMap<FString, FSoftObjectPath> NewExportClasses = GetExportClasses(NewClass);
	TArray<FCoreRedirect> Redirects;
	for (const TPair<FString, FSoftObjectPath>& OldClassInfo : OldExportClasses)
	{
		if (const FSoftObjectPath* NewPath = NewExportClasses.Find(OldClassInfo.Key))
		{
			Redirects.Emplace(ECoreRedirectFlags::Type_Class, OldClassInfo.Value.GetAssetPath(), NewPath->GetAssetPath());
		}
	}
	return Redirects;
}

TArray<UObject*> CreateClassGradeTestObjects()
{
	return {
		CreateTestObjectV1(),
		CreateTestObjectV2(),
		CreateTestObjectV3(),
		CreateTestObjectV4(),
		CreateTestObjectV5(),
		CreateTestObjectV6(),
		CreateTestObjectV7(),
	};
}

FString SaveInstanceDataObjectObject(UObject* ObjectToSave)
{
	UPackage* TempPackage = CreateTestPackage(ObjectToSave->GetClass());

	FObjectDuplicationParameters DupParams = InitStaticDuplicateObjectParams(ObjectToSave, TempPackage);
	UObject* DuplicateObjectToSave = StaticDuplicateObjectEx(DupParams);
	DuplicateObjectToSave->SetFlags(RF_Public);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public;
	SaveArgs.bWarnOfLongFilename = false;
	SaveArgs.bSlowTask = false;

	const FString PackagePathName = TempPackage->GetPathName();
	const FString Filename = FPackageName::LongPackageNameToFilename(PackagePathName, FPackageName::GetAssetPackageExtension());
	const FSavePackageResultStruct SavePackageResult = UPackage::Save(TempPackage, nullptr, *Filename, SaveArgs);
	return SavePackageResult.IsSuccessful() && IFileManager::Get().FileExists(*Filename) ? Filename : FString();
}

static UObject* CreateInstanceDataObjectWithSubObjects(UObject* Owner)
{
	TArray<UObject*> SubObjects = {Owner};
	GetObjectsWithOuter(Owner, SubObjects);
	for (const UObject* SubObject : SubObjects)
	{
		UE::FPropertyBagRepository::Get().CreateInstanceDataObject(SubObject);
	}
	return UE::FPropertyBagRepository::Get().FindInstanceDataObject(Owner);
}


static UObject* GenerateTestInstanceDataObject(UObject* ObjectOld, UClass* NewClass)
{
	// 1) simulate OldObject being serialized
	FString Filename = SaveInstanceDataObjectObject(ObjectOld);
	
	// 2) simulate class changing
	TArray<FCoreRedirect> Redirects = BuildTestRedirects(ObjectOld->GetClass(), NewClass);
	FCoreRedirects::AddRedirectList(Redirects, TEXT("InstanceDataObjectTests"));
	
	// 3) simulate OldObject being deserialized into ObjectNew. This will create a property bag.
	UObject* ObjectNew = nullptr;
	{
		FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();

		// scoped flag changes to load context to support property bag generation
		TGuardValue<bool> ScopedTrackSerializedPropertyPath(LoadContext->bTrackSerializedPropertyPath, true);
		TGuardValue<bool> ScopedSerializeUnknownProperty(LoadContext->bSerializeUnknownProperty, true);
		TGuardValue<bool> ScopedImpersonateProperties(LoadContext->bImpersonateProperties, true);
		
		UPackage* TempPackage = CreateTestPackage(NewClass);
		LoadPackage(TempPackage, *Filename, LOAD_None);
		ObjectNew = FindObject<UObject>(TempPackage, *ObjectOld->GetName());
	}

	// 4) Construct an InstanceDataObject from the NewObject
	UObject* InstanceDataObjectObject = CreateInstanceDataObjectWithSubObjects(ObjectNew);

	// revert step 2 so we don't break other tests
	FCoreRedirects::RemoveRedirectList(Redirects, TEXT("InstanceDataObjectTests"));
	
	return InstanceDataObjectObject;
}

static void RunInstanceDataObjectTestCommand(const TArray<FString>& Parameters)
{
	TArray<UObject*> ObjectUpgrades = CreateClassGradeTestObjects();

	if (Parameters.Num() != 2)
	{
		UE_LOG(LogEngine, Display, TEXT("provide two integer parameters between 1 and %i"), ObjectUpgrades.Num());
		return;
	}

	const int32 OldClassVersion = FCString::Atoi(*Parameters[0]) - 1;
	const int32 NewClassVersion = FCString::Atoi(*Parameters[1]) - 1;
	if (!ObjectUpgrades.IsValidIndex(OldClassVersion) || !ObjectUpgrades.IsValidIndex(NewClassVersion))
	{
		UE_LOG(LogEngine, Display, TEXT("provide two integer parameters between 1 and %i"), ObjectUpgrades.Num());
		return;
	}
	
	UObject* ObjectOld = ObjectUpgrades[OldClassVersion];

	// simulate class changing and an InstanceDataObject being generated
	UClass* NewClass = ObjectUpgrades[NewClassVersion]->GetClass();
	UObject* InstanceDataObjectObject = GenerateTestInstanceDataObject(ObjectOld, NewClass);

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bHideSelectionTip = true;
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	
	DetailsView->SetObject(InstanceDataObjectObject);
	
	const FName TabName = FName(TEXT("InstanceDataObjectsTest"));
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName, FOnSpawnTab::CreateLambda([DetailsView](const FSpawnTabArgs& TabArgs)
	{
		return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			DetailsView
		];
	}))
		.SetDisplayName(LOCTEXT("InstanceDataObjectsTestTitle", "Verse InstanceDataObject Test"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	const TSharedPtr<SDockTab> DockTab = FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TabName));
	DockTab->DrawAttention();
}

static void RunInstanceDataObjectFixupCommand(const TArray<FString>& Parameters)
{
	TArray<UObject*> ObjectUpgrades = CreateClassGradeTestObjects();

	if (Parameters.Num() != 2)
	{
		UE_LOG(LogEngine, Display, TEXT("provide two integer parameters between 1 and %i"), ObjectUpgrades.Num());
		return;
	}

	const int32 OldClassVersion = FCString::Atoi(*Parameters[0]) - 1;
	const int32 NewClassVersion = FCString::Atoi(*Parameters[1]) - 1;
	if (!ObjectUpgrades.IsValidIndex(OldClassVersion) || !ObjectUpgrades.IsValidIndex(NewClassVersion))
	{
		UE_LOG(LogEngine, Display, TEXT("provide two integer parameters between 1 and %i"), ObjectUpgrades.Num());
		return;
	}
	
	UObject* ObjectOld = ObjectUpgrades[OldClassVersion];
	
	// simulate class changing and an InstanceDataObject being generated
	UClass* NewClass = ObjectUpgrades[NewClassVersion]->GetClass();
	UObject* InstanceDataObjectObject = GenerateTestInstanceDataObject(ObjectOld, NewClass);

	const FName TabName = FName(TEXT("InstanceDataObjectsTestFixup"));
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName, FOnSpawnTab::CreateLambda([InstanceDataObjectObject](const FSpawnTabArgs& TabArgs)
	{
		return FInstanceDataObjectFixupToolModule::Get().CreateInstanceDataObjectFixupTab(TabArgs, {InstanceDataObjectObject});
	}))
		.SetDisplayName(LOCTEXT("InstanceDataObjectsFixupTestTitle", "Verse InstanceDataObject Fixup"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	const TSharedPtr<SDockTab> DockTab = FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TabName));
	DockTab->DrawAttention();
}

FAutoConsoleCommand TestInstanceDataObjectsCommand(
	TEXT("TestInstanceDataObject"),
	TEXT("syntax: TestInstanceDataObject <OldClassVersion> <NewClassVersion>"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&RunInstanceDataObjectTestCommand),
	ECVF_Default
);

FAutoConsoleCommand TestInstanceDataObjectFixupCommand(
	TEXT("TestInstanceDataObjectFixup"),
	TEXT("syntax: TestInstanceDataObjectFixup <OldClassVersion> <NewClassVersion>"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&RunInstanceDataObjectFixupCommand),
	ECVF_Default
);

#undef LOCTEXT_NAMESPACE
