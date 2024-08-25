// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Jupiter.Implementation
{
	public interface ILastAccessTracker<T>
	{
		Task TrackUsed(T record);
	}

	public interface ILastAccessCache<T>
	{
		Task<List<(T, DateTime)>> GetLastAccessedRecords();
	}
}
