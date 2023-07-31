// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AutomationTestExcludelist.generated.h"

UENUM(Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ERHI_Flags : uint8
{
	DirectX11	= 1 << 0 UMETA(DisplayName = "DirectX 11"),
	DirectX12	= 1 << 1 UMETA(DisplayName = "DirectX 12"),
	Vulkan		= 1 << 2 UMETA(DisplayName = "Vulkan"),
	Metal		= 1 << 3 UMETA(DisplayName = "Metal"),
	NUM			UMETA(Hidden)
};

ENUM_CLASS_FLAGS(ERHI_Flags);

USTRUCT()
struct FAutomationTestExcludeOptions
{
	GENERATED_BODY()

	void SetRHIFlagsFromNames(TSet<FName>& InRHIs)
	{
		int32 Num_RHIs = InRHIs.Num();
		if (Num_RHIs == 0)
			return;

		int32 InRHIs_index = 0;
		UEnum* Enum = StaticEnum<ERHI_Flags>();
		int32 Num_Flags = Enum->NumEnums();
		for (int32 i = 0; i < Num_Flags && InRHIs_index < Num_RHIs; i++)
		{
			if (InRHIs.Contains(*Enum->GetDisplayNameTextByIndex(i).ToString()))
			{
				RHIs |= (int8)Enum->GetValueByIndex(i);
				InRHIs_index++;
			}
		}
	}
	/* Name of the target test */
	UPROPERTY(VisibleAnywhere, Category = ExcludeTestOptions)
	FName Test;

	/* Reason to why the test is excluded */
	UPROPERTY(EditAnywhere, Category = ExcludeTestOptions)
	FName Reason;

	/* Option to target specific RHI. No option means it should be applied to all RHI */
	UPROPERTY(EditAnywhere, Meta = (Bitmask, BitmaskEnum = "/Script/AutomationTest.ERHI_Flags"), Category = ExcludeTestOptions)
	int8 RHIs = 0;

	/* Should the Reason be reported as a warning in the log */
	UPROPERTY(EditAnywhere, Category = ExcludeTestOptions)
	bool Warn = false;
};

USTRUCT()
struct FAutomationTestExcludelistEntry
{
	GENERATED_BODY()

	FAutomationTestExcludelistEntry() { }

	FAutomationTestExcludelistEntry(const FAutomationTestExcludeOptions& Options)
		: Test(Options.Test)
		, Reason(Options.Reason)
		, Warn(Options.Warn)
	{
		UEnum* Enum = StaticEnum<ERHI_Flags>();
		int32 Num_Flags = Enum->NumEnums();
		for (int32 i = 0; i < Num_Flags; i++)
		{
			int8 Flag = IntCastChecked<int8>(Enum->GetValueByIndex(i));
			if ((int8)ERHI_Flags::NUM == Flag)
				break; // We reach the maximum value

			if ((Options.RHIs & Flag) == Flag)
			{
				RHIs.Add(*Enum->GetDisplayNameTextByIndex(i).ToString());
			}
		}
	}

	TSharedPtr<FAutomationTestExcludeOptions> GetOptions()
	{
		TSharedPtr<FAutomationTestExcludeOptions> Options = MakeShareable(new FAutomationTestExcludeOptions());
		Options->Test = Test;
		Options->Reason = Reason;
		Options->Warn = Warn;
		Options->SetRHIFlagsFromNames(RHIs);
		
		return Options;
	}

	/* Determine if exclusion entry is propagated based on test name - used for management in test automation window */
	void SetPropagation(const FString& ForTestName)
	{
		bIsPropagated = FullTestName != ForTestName.TrimStartAndEnd().ToLower();
	}

	/* Return true if the entry is not specific */
	bool IsEmpty() const
	{
		return FullTestName.IsEmpty();
	}

	/* Reset entry to be un-specific */
	void Reset()
	{
		FullTestName.Empty();
		bIsPropagated = false;
	}

	/* Has conditional exclusion */
	bool HasConditions() const
	{
		return !RHIs.IsEmpty();
	}

	/* Remove exclusion conditions, return true if a condition was removed */
	bool RemoveConditions(const FAutomationTestExcludelistEntry& Entry)
	{
		if (!Entry.HasConditions())
		{
			return false;
		}

		bool GotRemoved = false;
		// Check RHIs
		int Length = RHIs.Num();
		if (Length > 0)
		{
			RHIs = RHIs.Difference(Entry.RHIs);
			GotRemoved = GotRemoved || Length != RHIs.Num();
		}

		return GotRemoved;
	}

	/* Hold full test name/path */
	FString FullTestName;
	/* Is the entry comes from propagation */
	bool bIsPropagated = false;

	// Use FName instead of FString to read params from config that aren't wrapped with quotes

	/* Hold the name of the target functional test map */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	FName Map;

	/* Hold the name of the target test - full test name is require here unless for functional tests */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	FName Test;

	/* Reason to why the test is excluded */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	FName Reason;

	/* Option to target specific RHI. Empty array means it should be applied to all RHI */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	TSet<FName> RHIs;

	/* Should the Reason be reported as a warning in the log */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	bool Warn = false;
};


UCLASS(config = Engine, defaultconfig)
class AUTOMATIONTEST_API UAutomationTestExcludelist : public UObject
{
	GENERATED_BODY()

public:
	UAutomationTestExcludelist() {}

	static UAutomationTestExcludelist* Get();

	void AddToExcludeTest(const FString& TestName, const FAutomationTestExcludelistEntry& ExcludelistEntry);
	void RemoveFromExcludeTest(const FString& TestName);
	bool IsTestExcluded(const FString& TestName, const FString& RHI = TEXT(""), FName* OutReason = nullptr, bool* OutWarn = nullptr);
	FAutomationTestExcludelistEntry* GetExcludeTestEntry(const FString& TestName, const FString& RHI = TEXT(""));

	void SaveConfig();
	FString GetConfigFilename() { return UObject::GetDefaultConfigFilename(); } const
	// It is called automatically when CDO is created, usually you don't need to call LoadConfig manually
	void LoadConfig() { UObject::LoadConfig(GetClass()); }

	virtual void OverrideConfigSection(FString& SectionName) override;

protected:
	virtual void PostInitProperties() override;

private:
	FString GetFullTestName(const FAutomationTestExcludelistEntry& ExcludelistEntry);

	UPROPERTY(EditDefaultsOnly, globalconfig, Category = AutomationTestExcludelist)
	TArray<FAutomationTestExcludelistEntry> ExcludeTest;
};

