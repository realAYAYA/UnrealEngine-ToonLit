// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterTestsDevicePropertyTester.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterTestDeviceProperty, Log, All);

AShooterTestsDevicePropertyTester::AShooterTestsDevicePropertyTester()
{
	RootComponent = CollisionVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionVolume"));
	CollisionVolume->InitCapsuleSize(80.f, 80.f);
	CollisionVolume->OnComponentBeginOverlap.AddDynamic(this, &AShooterTestsDevicePropertyTester::OnOverlapBegin);
	CollisionVolume->OnComponentEndOverlap.AddDynamic(this, &AShooterTestsDevicePropertyTester::OnEndOverlap);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
}

void AShooterTestsDevicePropertyTester::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		ApplyDeviceProperties(Pawn->GetPlatformUserId());
	}
}

void AShooterTestsDevicePropertyTester::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (const APawn* Character = Cast<APawn>(OtherActor))
	{
		RemoveDeviceProperties();
	}
}

void AShooterTestsDevicePropertyTester::ApplyDeviceProperties(const FPlatformUserId UserId)
{
	if (!UserId.IsValid())
	{
		UE_LOG(LogShooterTestDeviceProperty, Error, TEXT("Cannot apply device properties to an invalid Platform User!"));
		return;
	}

	if (UInputDeviceSubsystem* System = UInputDeviceSubsystem::Get())
	{
		FActivateDevicePropertyParams Params = {};
		Params.UserId = UserId;
		
		for (TSubclassOf<UInputDeviceProperty> DevicePropClass : DeviceProperties)
		{
			ActivePropertyHandles.Emplace(System->ActivateDevicePropertyOfClass(DevicePropClass, Params));			
		}
	}
}

void AShooterTestsDevicePropertyTester::RemoveDeviceProperties()
{
	// Remove any device properties that have been applied
	if (UInputDeviceSubsystem* InputDeviceSubsystem = UInputDeviceSubsystem::Get())
	{
		InputDeviceSubsystem->RemoveDevicePropertyHandles(ActivePropertyHandles);
	}
	
	ActivePropertyHandles.Empty();
}