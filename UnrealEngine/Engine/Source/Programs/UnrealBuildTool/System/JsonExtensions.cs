// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics.CodeAnalysis;
using EpicGames.Core;

namespace JsonExtensions
{
	/// <summary>
	/// Extension methods for JsonObject to provide some deprecated field helpers
	/// </summary>
	public static class DeprecatedFieldHelpers
	{
		/// <summary>
		/// Tries to read a string array field by the given name from the object; if that's not found, checks for an older deprecated name
		/// </summary>
		/// <param name="Obj">JSON object to check</param>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="DeprecatedFieldName">Backup field name to check</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public static bool TryGetStringArrayFieldWithDeprecatedFallback(this JsonObject Obj, string FieldName, string DeprecatedFieldName, [NotNullWhen(true)] out string[]? Result)
		{
			if (Obj.TryGetStringArrayField(FieldName, out Result))
			{
				return true;
			}
			else if (Obj.TryGetStringArrayField(DeprecatedFieldName, out Result))
			{
				//@TODO: Warn here?
				return true;
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// Tries to read an enum array field by the given name from the object; if that's not found, checks for an older deprecated name
		/// </summary>
		/// <param name="Obj">JSON object to check</param>
		/// <param name="FieldName">Name of the field to get</param>
		/// <param name="DeprecatedFieldName">Backup field name to check</param>
		/// <param name="Result">On success, receives the field value</param>
		/// <returns>True if the field could be read, false otherwise</returns>
		public static bool TryGetEnumArrayFieldWithDeprecatedFallback<T>(this JsonObject Obj, string FieldName, string DeprecatedFieldName, [NotNullWhen(true)] out T[]? Result) where T : struct
		{
			if (Obj.TryGetEnumArrayField<T>(FieldName, out Result))
			{
				return true;
			}
			else if (Obj.TryGetEnumArrayField<T>(DeprecatedFieldName, out Result))
			{
				//@TODO: Warn here?
				return true;
			}
			else
			{
				return false;
			}
		}
	}
}