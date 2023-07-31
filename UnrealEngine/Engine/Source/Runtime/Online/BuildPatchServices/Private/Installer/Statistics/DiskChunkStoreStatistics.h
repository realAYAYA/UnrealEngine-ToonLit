// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/DiskChunkStore.h"

namespace BuildPatchServices
{
	class IInstallerAnalytics;
	class IFileOperationTracker;

	/**
	 * Interface to the statistics class which provides access to tracked values from a disk chunk source stat.
	 */
	class IDiskChunkStoreStatistics
		: public IDiskChunkStoreStat
	{
	public:
		/**
		 * @return the number of chunks which were loaded from the disk store.
		 */
		virtual int32 GetNumSuccessfulLoads() const = 0;

		/**
		 * @return the number of chunks which failed to load from the disk store.
		 */
		virtual int32 GetNumFailedLoads() const = 0;

		/**
		 * @return the number of chunks which were saved to the disk store.
		 */
		virtual int32 GetNumSuccessfulSaves() const = 0;

		/**
		 * @return the number of chunks which failed to save to the disk store.
		 */
		virtual int32 GetNumFailedSaves() const = 0;
	};

	/**
	 * A factory for creating an IDiskChunkStoreStatistics instance.
	 */
	class FDiskChunkStoreStatisticsFactory
	{
	public:
		/**
		 * Creates the disk chunk store's dependency interface and exposes additional information.
		 * @param InstallerAnalytics        The analytics implementation for reporting the installer events.
		 * @param FileOperationTracker      The file operation tracker which will be used to update data states.
		 * @return the new IDiskChunkStoreStatistics instance created.
		 */
		static IDiskChunkStoreStatistics* Create(IInstallerAnalytics* InstallerAnalytics, IFileOperationTracker* FileOperationTracker);
	};
}