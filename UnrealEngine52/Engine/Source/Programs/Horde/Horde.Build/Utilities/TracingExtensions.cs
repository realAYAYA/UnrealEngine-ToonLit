// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;
using Horde.Build.Jobs;
using OpenTracing;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Extensions to handle Horde specific data types in the OpenTracing library
	/// </summary>
	public static class OpenTracingSpanExtensions
	{
		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag<T>(this ISpan span, string key, StringId<T>? value)
		{
			if (value != null)
			{
				span.SetTag(key, value.ToString());
			}
			return span;
		}
		
		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag<T>(this ISpan span, string key, StringId<T> value)
		{
			span.SetTag(key, value.ToString());
			return span;
		}

		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag<T>(this ISpan span, string key, IEnumerable<StringId<T>>? value)
		{
			if (value != null)
			{
				span.SetTag(key, String.Join(',', value));
			}
			return span;
		}

		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag<T>(this ISpan span, string key, ObjectId<T>? value)
		{
			if (value != null)
			{
				span.SetTag(key, value.ToString());
			}
			return span;
		}
		
		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag<T>(this ISpan span, string key, ObjectId<T> value)
		{
			span.SetTag(key, value.ToString());
			return span;
		}
		
		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag(this ISpan span, string key, ContentHash value)
		{
			span.SetTag(key, value.ToString());
			return span;
		}
		
		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag(this ISpan span, string key, SubResourceId value)
		{
			span.SetTag(key, value.ToString());
			return span;
		}
		
		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag<T>(this ISpan span, string key, IEnumerable<ObjectId<T>>? value)
		{
			if (value != null)
			{
				span.SetTag(key, String.Join(',', value));
			}
			return span;
		}
		
		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag(this ISpan span, string key, int? value)
		{
			if (value != null)
			{
				span.SetTag(key, value.Value);
			}
			return span;
		}
		
		/// <summary>Set a key:value tag on the span</summary>
		/// <returns>This span instance, for chaining</returns>
		public static ISpan SetTag(this ISpan span, string key, DateTimeOffset? value)
		{
			if (value != null)
			{
				span.SetTag(key, value.ToString());
			}
			return span;
		}
	}
}