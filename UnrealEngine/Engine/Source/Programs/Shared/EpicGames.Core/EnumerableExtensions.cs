// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

namespace EpicGames.Core
{
	/// <summary>
	/// Extension methods for <see cref="IEnumerable{T}"/>
	/// </summary>
	public static class EnumerableExtensions
	{
		/// <summary>
		/// Split the sequence into batches of at most the given size
		/// </summary>
		/// <typeparam name="TElement">The element type</typeparam>
		/// <param name="sequence">Sequence to split into batches</param>
		/// <param name="batchSize">Maximum size of each batch</param>
		/// <returns>Sequence of batches</returns>
		public static IEnumerable<IReadOnlyList<TElement>> Batch<TElement>(this IEnumerable<TElement> sequence, int batchSize)
		{
			List<TElement> elements = new List<TElement>(batchSize);
			foreach (TElement element in sequence)
			{
				elements.Add(element);
				if (elements.Count == batchSize)
				{
					yield return elements;
					elements.Clear();
				}
			}
			if (elements.Count > 0)
			{
				yield return elements;
			}
		}

		/// <summary>
		/// Finds the minimum element by a given field
		/// </summary>
		/// <typeparam name="TElement"></typeparam>
		/// <param name="sequence"></param>
		/// <param name="selector"></param>
		/// <returns></returns>
		public static TElement MinBy<TElement>(this IEnumerable<TElement> sequence, Func<TElement, int> selector)
		{
			IEnumerator<TElement> enumerator = sequence.GetEnumerator();
			if (!enumerator.MoveNext())
			{
				throw new Exception("Collection is empty");
			}

			TElement minElement = enumerator.Current;

			int minValue = selector(minElement);
			while (enumerator.MoveNext())
			{
				int value = selector(enumerator.Current);
				if (value < minValue)
				{
					minElement = enumerator.Current;
				}
			}

			return minElement;
		}

		/// <summary>
		/// Finds the maximum element by a given field
		/// </summary>
		/// <typeparam name="TElement"></typeparam>
		/// <param name="sequence"></param>
		/// <param name="selector"></param>
		/// <returns></returns>
		public static TElement MaxBy<TElement>(this IEnumerable<TElement> sequence, Func<TElement, int> selector)
		{
			IEnumerator<TElement> enumerator = sequence.GetEnumerator();
			if (!enumerator.MoveNext())
			{
				throw new Exception("Collection is empty");
			}

			TElement maxElement = enumerator.Current;

			int maxValue = selector(maxElement);
			while (enumerator.MoveNext())
			{
				int value = selector(enumerator.Current);
				if (value > maxValue)
				{
					maxElement = enumerator.Current;
				}
			}

			return maxElement;
		}
	}
}
