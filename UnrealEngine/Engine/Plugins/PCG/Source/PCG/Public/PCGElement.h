// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogVerbosity.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UPCGSettingsInterface;
struct FPCGContext;
struct FPCGCrc;
struct FPCGDataCollection;

class IPCGElement;
class UPCGComponent;
class UPCGSettings;
class UPCGNode;

typedef TSharedPtr<IPCGElement, ESPMode::ThreadSafe> FPCGElementPtr;

namespace EPCGElementLogMode
{
	enum Type : uint8
	{
		/** Output to log. */
		LogOnly = 0,
		/** Display errors/warnings on graph as well as writing to log. */
		GraphAndLog,

		NumLogModes,

		// Used below in log macros to silence PVS by making the log mode comparison 'look' non-trivial by adding a trivial mask op (an identical
		// mechanism as the one employed in the macro UE_ASYNC_PACKAGE_LOG in AsyncLoading2.cpp).
		// Warning V501: There are identical sub-expressions 'EPCGElementLogMode::GraphAndLog' to the left and to the right of the '==' operator.
		// The warning disable comment can can't be used in a macro: //-V501 
		LogModeMask = 0xff
	};
};

#define PCGE_LOG_BASE(Verbosity, CustomContext, Message) \
	UE_LOG(LogPCG, \
		Verbosity, \
		TEXT("[%s - %s]: %s"), \
		(CustomContext) ? *((CustomContext)->GetComponentName()) : TEXT("UnknownComponent"), \
		(CustomContext) ? *((CustomContext)->GetTaskName()) : TEXT("UnknownTask"), \
		*Message.ToString())

#if WITH_EDITOR
// Output to PCG log and optionally also display warnings/errors on graph.
#define PCGE_LOG(Verbosity, LogMode, Message) do { \
		if (((EPCGElementLogMode::LogMode) & EPCGElementLogMode::LogModeMask) == EPCGElementLogMode::GraphAndLog && (Context)) { (Context)->LogVisual(ELogVerbosity::Verbosity, Message); } \
		if (ShouldLog()) { PCGE_LOG_BASE(Verbosity, Context, Message); } \
	} while (0)
// Output to PCG log and optionally also display warnings/errors on graph. Takes context as argument.
#define PCGE_LOG_C(Verbosity, LogMode, CustomContext, Message) do { \
		if (((EPCGElementLogMode::LogMode) & EPCGElementLogMode::LogModeMask) == EPCGElementLogMode::GraphAndLog && (CustomContext)) { (CustomContext)->LogVisual(ELogVerbosity::Verbosity, Message); } \
		PCGE_LOG_BASE(Verbosity, CustomContext, Message); \
	} while (0)
#else
#define PCGE_LOG(Verbosity, LogMode, Message) PCGE_LOG_BASE(Verbosity, Context, Message)
#define PCGE_LOG_C(Verbosity, LogMode, CustomContext, Message) PCGE_LOG_BASE(Verbosity, CustomContext, Message)
#endif

/**
* Base class for the processing bit of a PCG node/settings
*/
class PCG_API IPCGElement
{
public:
	virtual ~IPCGElement() = default;

	/** Creates a custom context object paired to this element */
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node);

	/** Returns true if the element, in its current phase can be executed only from the main thread */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const { return false; }

	/** Returns true if the node can be cached - also checks for instance flags, if any. */
	bool IsCacheableInstance(const UPCGSettingsInterface* InSettingsInterface) const;

	/** Returns true if the node can be cached (e.g. does not create artifacts & does not depend on untracked data */
	virtual bool IsCacheable(const UPCGSettings* InSettings) const { return true; }

	/** Whether to do a 'deep' fine-grained CRC of the output data to pass to downstream nodes. Can be expensive so should be used sparingly. */
	virtual bool ShouldComputeFullOutputDataCrc(FPCGContext* Context) const { return false; }

	/**
	 * Calculate a Crc that provides a receipt for the input data that can be paired with output data from the cache. If any dependency (setting, node input or
	 * external data) changes then this value should change. For some elements it is inefficient or not possible to output a Crc here. These can return an invalid
	 * Crc and the Crc can either be computed during execution, or afterwards based on output data.
	 */
	virtual void GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const;

	/** Public function that executes the element on the appropriately created context.
	* The caller should call the Execute function until it returns true.
	*/
	bool Execute(FPCGContext* Context) const;

	/** Public function called when an element is cancelled, passing its current context if any. */
	void Abort(FPCGContext* Context) const;

#if WITH_EDITOR
	/** Is this element used for marshalling data across higen grids. Used as rudimentary RTTI for element object comparisons (editor only). */
	virtual bool IsGridLinkage() const { return false; }

	/** Note: must be called from the main thread. */
	void DebugDisplay(FPCGContext* Context) const;
#endif

protected:
	/** This function will be called once and once only, at the beginning of an execution */
	void PreExecute(FPCGContext* Context) const;
	/** The prepare data phase is one where it is more likely to be able to multithread */
	virtual bool PrepareDataInternal(FPCGContext* Context) const;
	/** Core execution method for the given element. Will be called until it returns true. */
	virtual bool ExecuteInternal(FPCGContext* Context) const = 0;
	/** This function will be called once and once only, at the end of an execution */
	void PostExecute(FPCGContext* Context) const;
	/** This function will be called once and only once if the element is aborted. The Context can be used to retrieve the current phase if needed. */
	virtual void AbortInternal(FPCGContext* Context) const {};

	/** Controls whether an element can skip its execution wholly when the input data has the cancelled tag */
	virtual bool IsCancellable() const { return true; }
	/** Used to specify that the element passes through the data without any manipulation - used to correct target pins, etc. */
	virtual bool IsPassthrough(const UPCGSettings* InSettings) const { return false; }

	/** Passes through data when the element is Disabled. Can be implemented to override what gets passed through. */
	virtual void DisabledPassThroughData(FPCGContext* Context) const;

	/** Let each element optionally act as a concrete factory for its own context */
	virtual FPCGContext* CreateContext();

#if WITH_EDITOR
	virtual bool ShouldLog() const { return true; }
#endif

private:
	void CleanupAndValidateOutput(FPCGContext* Context) const;
};

/**
* Convenience class for element with custom context that can be default constructed (need no initialization)
*/
template <typename ContextType, typename = typename std::enable_if_t<std::is_base_of_v<FPCGContext, ContextType> && std::is_default_constructible_v<ContextType>>>
class PCG_API IPCGElementWithCustomContext : public IPCGElement
{
public:
	virtual FPCGContext* CreateContext() { return new ContextType(); }
};

class UE_DEPRECATED(5.4, "This class has been deprecated. Please inherit from IPCGElement directly.") PCG_API FSimplePCGElement : public IPCGElement
{};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Misc/OutputDeviceRedirector.h"
#include "PCGContext.h"
#include "PCGData.h"
#endif
