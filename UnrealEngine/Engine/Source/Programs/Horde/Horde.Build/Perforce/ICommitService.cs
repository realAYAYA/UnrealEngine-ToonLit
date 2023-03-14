// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Build.Streams;
using Horde.Build.Utilities;

namespace Horde.Build.Perforce
{
	/// <summary>
	/// Provides information about commits
	/// </summary>
	public interface ICommitService
	{
		/// <summary>
		/// Registers a delegate to be called when a new commit is added
		/// </summary>
		/// <param name="onAddCommit">Callback for a new commit being added</param>
		/// <returns>Disposable handler.</returns>
		IAsyncDisposable AddListener(Func<ICommit, Task> onAddCommit);
	}
}
