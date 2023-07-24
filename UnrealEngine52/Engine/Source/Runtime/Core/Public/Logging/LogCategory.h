// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Logging/LogVerbosity.h"
#include "UObject/NameTypes.h"

#define UE_API CORE_API

#if defined(UE_LOG_LAZY_CATEGORY_NAMES) && UE_LOG_LAZY_CATEGORY_NAMES
using FLogCategoryName = FLazyName;
#else
using FLogCategoryName = FName;
#endif

/** Base class for all log categories. */
struct FLogCategoryBase
{
	/**
	 * Constructor, registers with the log suppression system and sets up the default values.
	 *
	 * @param CategoryName           Name of the category.
	 * @param DefaultVerbosity       Default verbosity used to filter this category at runtime.
	 * @param CompileTimeVerbosity   Verbosity used to filter this category at compile time.
	 */
	UE_API FLogCategoryBase(const FLogCategoryName& CategoryName, ELogVerbosity::Type DefaultVerbosity, ELogVerbosity::Type CompileTimeVerbosity);

	/** Destructor, unregisters from the log suppression system. */
	UE_API ~FLogCategoryBase();

	/** Should not generally be used directly. Tests the runtime verbosity and maybe triggers a debug break, etc. */
	FORCEINLINE constexpr bool IsSuppressed(ELogVerbosity::Type VerbosityLevel) const
	{
		return !((VerbosityLevel & ELogVerbosity::VerbosityMask) <= Verbosity);
	}

	/** Called just after a logging statement being allow to print. Checks a few things and maybe breaks into the debugger. */
	UE_API void PostTrigger(ELogVerbosity::Type VerbosityLevel);

	inline constexpr const FLogCategoryName& GetCategoryName() const { return CategoryName; }

	/** Gets the working verbosity. */
	inline constexpr ELogVerbosity::Type GetVerbosity() const { return (ELogVerbosity::Type)Verbosity; }
	
	/** Sets up the working verbosity and clamps to the compile time verbosity. */
	UE_API void SetVerbosity(ELogVerbosity::Type Verbosity);

	/** Gets the compile time verbosity. */
	inline constexpr ELogVerbosity::Type GetCompileTimeVerbosity() const { return CompileTimeVerbosity; }

private:
	friend class FLogSuppressionImplementation;
	friend class FLogScopedVerbosityOverride;

	/** Internal call to set up the working verbosity from the boot time default. */
	void ResetFromDefault();

	/** Holds the current suppression state */
	ELogVerbosity::Type Verbosity;
	/** Holds the break flag */
	bool DebugBreakOnLog;
	/** Holds default suppression */
	uint8 DefaultVerbosity;
	/** Holds compile time suppression */
	const ELogVerbosity::Type CompileTimeVerbosity;
	/** Name for this category */

	const FLogCategoryName CategoryName;
};

/** Template for log categories that transfers the compile-time constant default and compile time verbosity to the FLogCategoryBase constructor. */
template <ELogVerbosity::Type InDefaultVerbosity, ELogVerbosity::Type InCompileTimeVerbosity>
struct FLogCategory : public FLogCategoryBase
{
	static_assert((InDefaultVerbosity & ELogVerbosity::VerbosityMask) < ELogVerbosity::NumVerbosity, "Bogus default verbosity.");
	static_assert(InCompileTimeVerbosity < ELogVerbosity::NumVerbosity, "Bogus compile time verbosity.");

	static constexpr ELogVerbosity::Type CompileTimeVerbosity = InCompileTimeVerbosity;

	inline constexpr ELogVerbosity::Type GetCompileTimeVerbosity() const { return CompileTimeVerbosity; }

	FORCEINLINE FLogCategory(const FLogCategoryName& InCategoryName)
		: FLogCategoryBase(InCategoryName, InDefaultVerbosity, CompileTimeVerbosity)
	{
	}
};

#undef UE_API
