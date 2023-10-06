// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace TypedElementDataStorage
{
	enum class EQueryTickPhase : uint8
	{
		PrePhysics, //< Queries are executed before physics simulation starts.
		DuringPhysics, //< Queries that can be run in parallel with physics simulation work.
		PostPhysics, //< Queries that need rigid body and cloth simulation to be completed before being executed.
		FrameEnd, //< Catchall for queries demoted to the last possible moment.

		Max //< Value indicating the maximum value in this enum. Not to be used as an enum value.
	};

	enum class EQueryTickGroups : uint8
	{
		/** The standard group to run work in. */
		Default,

		/**
		 * The group for queries that need to sync data from external sources such as subsystems or the world into
		 * the Data Storage. These typically run early in a phase.
		 */
		SyncExternalToDataStorage,
		/**
		 * The group for queries that need to sync data from the Data Storage to external sources such as subsystems
		 * or the world into. These typically run late in a phase.
		 */
		SyncDataStorageToExternal,
		/**
		 * Queries grouped under this name will sync data to/from widgets.
		 */
		SyncWidgets,

		Max //< Value indicating the maximum value in this enum. Not to be used as an enum value.
	};

	enum class EQueryCallbackType : uint8
	{
		/** No callback provided. */
		None,
		/** The query will be run every tick if at least one row matches. */
		Processor,
		/** The query will be run when a row is added that matches the query. The first recorded column will be actively monitored for changes. */
		ObserveAdd,
		/** The query will be run when a row is removed that matches the query. The first recorded column will be actively monitored for changes. */
		ObserveRemove,
		/**
		 * At the start of the assigned phase this query will run if there are any matches. These queries will have any deferred operations such as
		 * adding/removing rows/columns executed before the phase starts. This introduces sync points that hinder performance and are therefore
		 * recommended only for queries that save on work later in the phase such as repeated checks for validity.
		 */
		PhasePreparation,
		/**
		 * At the end of the assigned phase this query will run if there are any matches. These queries will have any deferred operations such as
		 * adding/removing rows/columns executed before the phase ends. This introduces sync points that hinder performance and are therefore
		 * recommended only cases where delaying deferred operations is not possible e.g. when tables are known to be referenced outside the update
		 * cycle.
		 */
		PhaseFinalization,

		Max //< Value indicating the maximum value in this enum. Not to be used as an enum value.
	};

	enum class EQueryAccessType : bool
	{
		ReadOnly,
		ReadWrite
	};

	enum class EQueryDependencyFlags : uint8
	{
		None = 0,
		/** If set the dependency is accessed as read-only. If not set the dependency requires Read/Write access. */
		ReadOnly = 1 << 0,
		/** If set the dependency can only be used from the game thread, otherwise it can be accessed from any thread. */
		GameThreadBound = 1 << 1,
		/** If set the dependency will be re-fetched every iteration, otherwise only if not fetched before. */
		AlwaysRefresh = 1 << 2
	};
	ENUM_CLASS_FLAGS(EQueryDependencyFlags);

	struct FQueryResult final
	{
		enum class ECompletion
		{
			/** Query could be fully executed. */
			Fully,
			/** Only portions of the query were executed. This is caused by a problem that was encountered partway through processing. */
			Partially,
			/**
			 * The back-end doesn't support the particular query. This may be a limitation in how/where the query is run or because
			 * the query contains actions and/or operations that are not supported.
			 */
			Unsupported,
			/** The provided query is no longer available. */
			Unavailable,
			/** One or more dependencies declared on the query could not be retrieved. */
			MissingDependency
		};

		uint32 Count{ 0 }; /** The number of rows were processed. */
		ECompletion Completed{ ECompletion::Unavailable };
	};
} // namespace TypedElementDataStorage
