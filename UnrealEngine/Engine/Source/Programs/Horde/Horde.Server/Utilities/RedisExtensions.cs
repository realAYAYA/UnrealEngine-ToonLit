// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using StackExchange.Redis;

namespace Horde.Server.Utilities;

/// <summary>
/// Extensions for Redis classes
/// </summary>
public static class RedisExtensions
{
	/// <summary>
	/// Await given tasks and swallow any task cancellation exceptions.
	/// Useful to do after (failing) transaction to ensure no trailing tasks are left unawaited
	/// </summary>
	/// <param name="transaction"></param>
	/// <param name="tasks">Tasks to await</param>
	public static async Task WaitAndIgnoreCancellationsAsync(this ITransaction transaction, params Task[] tasks)
	{
		_ = transaction;
		try
		{
			await Task.WhenAll(tasks);
		}
		catch (TaskCanceledException)
		{
			/* Ignore */
		}
	}
}