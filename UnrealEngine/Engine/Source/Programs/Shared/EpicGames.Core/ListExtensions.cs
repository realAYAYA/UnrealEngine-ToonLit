// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace EpicGames.Core
{
	/// <summary>
	/// Extension methods for lists
	/// </summary>s
	public static class ListExtensions
	{
		/// <summary>
		/// Wrapper around a list to implement an immutable <see cref="IList{T}"/> and <see cref="IReadOnlyList{T}"/>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		class ReadOnlyList<T> : IList<T>, IReadOnlyList<T>
		{
			readonly IList<T> _inner;

			public ReadOnlyList(IList<T> inner) => _inner = inner;

			static NotSupportedException CreateNotSupportedException()
				=> new NotSupportedException("Collection is read-only.");

			public T this[int index]
			{
				get => _inner[index];
				set => throw CreateNotSupportedException();
			}

			public int Count => _inner.Count;
			public bool IsReadOnly => _inner.IsReadOnly;

			public void Add(T item) => throw CreateNotSupportedException();
			public void Clear() => throw CreateNotSupportedException();
			public bool Contains(T item) => _inner.Contains(item);
			public void CopyTo(T[] array, int arrayIndex) => _inner.CopyTo(array, arrayIndex);
			public IEnumerator<T> GetEnumerator() => _inner.GetEnumerator();
			public int IndexOf(T item) => _inner.IndexOf(item);
			public void Insert(int index, T item) => throw CreateNotSupportedException();
			public bool Remove(T item) => throw CreateNotSupportedException();
			public void RemoveAt(int index) => throw CreateNotSupportedException();

			IEnumerator IEnumerable.GetEnumerator() => ((IEnumerable)_inner).GetEnumerator();
		}

		/// <summary>
		/// Create a read-only wrapper around a list
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="list"></param>
		/// <returns></returns>
		public static IList<T> AsReadOnly<T>(this IList<T> list) => new ReadOnlyList<T>(list);

		/// <summary>
		/// Create a read-only wrapper around a list
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="list"></param>
		/// <returns></returns>
		public static IReadOnlyList<T> AsReadOnlyList<T>(this IList<T> list) => new ReadOnlyList<T>(list);

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

		/// <summary>
		/// Add all arguments to the list
		/// </summary>
		/// <typeparam name="TElement">List element</typeparam>
		/// <param name="list">List to add to</param>
		/// <param name="arguments">Arguments to add to list</param>
		public static void AddAll<TElement>(this List<TElement> list, params TElement[] arguments)
		{
			list.AddRange(arguments.AsEnumerable());
		}

		/// <summary>
		/// Convert all elements of a list to a base type
		/// </summary>
		/// <typeparam name="TInput">Input element type</typeparam>
		/// <typeparam name="TOutput">Output element type</typeparam>
		/// <param name="input">List to convert</param>
		/// <returns>Converted list</returns>
		public static List<TOutput> ConvertAll<TInput, TOutput>(this List<TInput> input) where TInput : TOutput
		{
			List<TOutput> output = new List<TOutput>(input.Count);
			foreach (TInput inputItem in input)
			{
				output.Add(inputItem);
			}
			return output;
		}

		/// <summary>
		/// Convert all elements of a list to a base type
		/// </summary>
		/// <typeparam name="TInput">Input element type</typeparam>
		/// <typeparam name="TOutput">Output element type</typeparam>
		/// <param name="input">List to convert</param>
		/// <returns>Converted list</returns>
		public static async Task<List<TOutput>> ConvertAllAsync<TInput, TOutput>(this Task<List<TInput>> input) where TInput : TOutput
		{
			return ConvertAll<TInput, TOutput>(await input);
		}

		/// <summary>
		/// Convert all elements of a list to a base type
		/// </summary>
		/// <typeparam name="TInput">Input element type</typeparam>
		/// <typeparam name="TOutput">Output element type</typeparam>
		/// <param name="input">List to convert</param>
		/// <param name="converter">Converter for elements</param>
		/// <returns>Converted list</returns>
		public static async Task<List<TOutput>> ConvertAllAsync<TInput, TOutput>(this Task<List<TInput>> input, Converter<TInput, TOutput> converter)
		{
			return (await input).ConvertAll(converter);
		}
	}
}
