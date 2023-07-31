// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using Horde.Build.Utilities;

namespace Horde.Build.Issues
{
	/// <summary>
	/// Fingerprint for an issue
	/// </summary>
	public interface IIssueFingerprint
	{
		/// <summary>
		/// The type of issue, which defines the handler to use for it
		/// </summary>
		public string Type { get; }

		/// <summary>
		/// List of keys which identify this issue.
		/// </summary>
		public CaseInsensitiveStringSet Keys { get; }

		/// <summary>
		/// Set of keys which should trigger a negative match
		/// </summary>
		public CaseInsensitiveStringSet? RejectKeys { get; }

		/// <summary>
		/// Collection of additional metadata added by the handler
		/// </summary>
		public CaseInsensitiveStringSet? Metadata { get; }
	}

	/// <summary>
	/// Extension methods for <see cref="IIssueFingerprint"/>
	/// </summary>
	public static class IssueFingerprintExtensions
	{
		/// <summary>
		/// Checks if a fingerprint matches another fingerprint
		/// </summary>
		/// <param name="fingerprint">The first fingerprint to compare</param>
		/// <param name="other">The other fingerprint to compare to</param>
		/// <returns>True is the fingerprints match</returns>
		public static bool IsMatch(this IIssueFingerprint fingerprint, IIssueFingerprint other)
		{
			if (!fingerprint.Type.Equals(other.Type, StringComparison.Ordinal))
			{
				return false;
			}
			if (fingerprint.Keys.Count == 0)
			{
				if (other.Keys.Count > 0)
				{
					return false;
				}
			}
			else
			{
				if (!fingerprint.Keys.Any(x => other.Keys.Contains(x)))
				{
					return false;
				}
			}
			if (fingerprint.RejectKeys != null && fingerprint.RejectKeys.Any(x => other.Keys.Contains(x)))
			{
				return false;
			}
			return true;
		}

		/// <summary>
		/// Checks if a fingerprint matches another fingerprint for creating a new span
		/// </summary>
		/// <param name="fingerprint">The first fingerprint to compare</param>
		/// <param name="other">The other fingerprint to compare to</param>
		/// <returns>True is the fingerprints match</returns>
		public static bool IsMatchForNewSpan(this IIssueFingerprint fingerprint, IIssueFingerprint other)
		{
			if (!fingerprint.Type.Equals(other.Type, StringComparison.Ordinal))
			{
				return false;
			}
			if (fingerprint.RejectKeys != null && fingerprint.RejectKeys.Any(x => other.Keys.Contains(x)))
			{
				return false;
			}
			return true;
		}

		/// <summary>
		/// Gets all the metadata values with a given key
		/// </summary>
		/// <param name="fingerprint">Fingerprint to find values for</param>
		/// <param name="value">Key name to search for</param>
		/// <returns>All values with the given key</returns>
		public static bool HasMetadataValue(this IIssueFingerprint fingerprint, string value)
		{
			return fingerprint.Metadata != null && fingerprint.Metadata.Contains(value);
		}

		/// <summary>
		/// Gets all the metadata values with a given key
		/// </summary>
		/// <param name="fingerprint">Fingerprint to find values for</param>
		/// <param name="key">Key name to search for</param>
		/// <returns>All values with the given key</returns>
		public static IEnumerable<string> GetMetadataValues(this IIssueFingerprint fingerprint, string key)
		{
			if (fingerprint.Metadata != null)
			{
				foreach (string element in fingerprint.Metadata)
				{
					if (element.StartsWith(key, StringComparison.OrdinalIgnoreCase) && element.Length > key.Length && element[key.Length] == '=')
					{
						yield return element.Substring(key.Length + 1);
					}
				}
			}
		}
	}
}
