// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InstanceDataObjectTests.generated.h"

UENUM(BlueprintType)
enum class StudentGender : uint8
{
	Undefined,
	Male,
	Female,
};

UCLASS()
class UTestReportCardV1 : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Report Card")
	FString Name;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	StudentGender Gender = StudentGender::Undefined;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	int32 Age = -1;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float Grade = 100.f;
};

// Changing the type of Gender to FString
// renaming Grade to GPA
// adding several other properties
UCLASS()
class UTestReportCardV2 : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString Name;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	StudentGender Gender;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	int32 Age = -1;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float GPA = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float MathGrade = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	TArray<FString> MathNotes;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float ScienceGrade = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	TArray<FString> ScienceNotes;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float ArtGrade = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	TArray<FString> ArtNotes;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float EnglishGrade = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	TArray<FString> EnglishNotes;
};

USTRUCT()
struct FClassGrade
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Grade")
	FString Name;
	UPROPERTY(EditAnywhere, Category = "Grade")
	float Grade = 100.f;
	UPROPERTY(EditAnywhere, Category = "Grade")
	TArray<FString> TeacherNotes;
	UPROPERTY(EditAnywhere, Category = "Grade")
	FString MainTeacherName = FString(TEXT("Luther"));
};

// renaming Name to StudentName
// moving float grades into sub-properties of FClassGrade
UCLASS()
class UTestReportCardV3 : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString StudentName;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString Gender;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	int32 Age = -1;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float GPA = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	FClassGrade MathReport;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	FClassGrade ScienceReport;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	FClassGrade ArtReport;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	FClassGrade EnglishReport;
};

// moving class grades into a map
UCLASS()
class UTestReportCardV4 : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString StudentName;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString Gender;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	int32 Age = -1;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float GPA = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	TArray<FClassGrade> ClassGradesArray;

	UPROPERTY(EditAnywhere, Category = "Report Card")
	TMap<FString, FClassGrade> ClassGrades;
};

// changing class grade struct to include a ClassName
USTRUCT()
struct FClassGradeV2
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Grade")
	FString ClassName;
	UPROPERTY(EditAnywhere, Category = "Grade")
	float Grade = 100.f;
	UPROPERTY(EditAnywhere, Category = "Grade")
	TArray<FString> TeacherNotes;
};

UCLASS()
class UTestReportCardV5 : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString StudentName;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString Gender;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	int32 Age = -1;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float GPA = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Report Card", meta=(OriginalType="ClassGrade"))
	TArray<FClassGradeV2> ClassGradesArray;

	UPROPERTY(EditAnywhere, Category = "Report Card")
	TMap<FString, FClassGradeV2> ClassGrades;
};


UCLASS()
class UStudentInfoV1 : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString StudentName;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString Gender;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	int32 Age = -1;

	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString GradeYear;
};

UCLASS()
class UTestReportCardV6 : public UObject
{
	GENERATED_BODY()
    	
public:
	UPROPERTY(EditAnywhere, Instanced, Category = "Student Info")
	UStudentInfoV1* StudentInfo;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float Grade = 100.f;
};

UCLASS()
class UStudentInfoV2 : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString StudentName;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString Gender;
	
	UPROPERTY(EditAnywhere, Category = "Student Info")
	int32 Age = -1;

	UPROPERTY(EditAnywhere, Category = "Student Info")
	FString GradeName;
};

UCLASS()
class UTestReportCardV7 : public UObject
{
	GENERATED_BODY()
    	
public:
	UPROPERTY(EditAnywhere, Instanced, Category = "Student Info")
	UStudentInfoV2* StudentInfo;
	
	UPROPERTY(EditAnywhere, Category = "Report Card")
	float GPA = 100.f;
};

TArray<UObject*> CreateClassGradeTestObjects();