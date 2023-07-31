// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

namespace EpicGames.Core
{
	/// <summary>
	/// Extension methods for lists
	/// </summary>s
	public static class ListExtensions
	{
		/// <summary>
		/// Sorts a list by a particular field
		/// </summary>
		/// <typeparam name="TElement">List element</typeparam>
		/// <typeparam name="TField">Field type to sort by</typeparam>
		/// <param name="list">List to sort</param>
		/// <param name="selector">Selects a field from the element</param>
		public static void SortBy<TElement, TField>(this List<TElement> list, Func<TElement, TField> selector)
		{
			IComparer<TField> defaultComparer = Comparer<TField>.Default;
			SortBy(list, selector, defaultComparer);
		}

		/// <summary>
		/// Sorts a list by a particular field
		/// </summary>
		/// <typeparam name="TElement">List element</typeparam>
		/// <typeparam name="TField">Field type to sort by</typeparam>
		/// <param name="list">List to sort</param>
		/// <param name="selector">Selects a field from the element</param>
		/// <param name="comparer">Comparer for fields</param>
		public static void SortBy<TElement, TField>(this List<TElement> list, Func<TElement, TField> selector, IComparer<TField> comparer)
		{
			list.Sort((x, y) => comparer.Compare(selector(x), selector(y)));
		}
	}
}
