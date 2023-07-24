// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using StatsdClient;

namespace Horde.Build.Utilities
{
	/// <summary>
	/// Empty implementation that does nothing but still satisfies the interface
	/// Used when DogStatsD is not available.
	/// </summary>
	public sealed class NoOpDogStatsd : IDogStatsd
	{
		/// <inheritdoc />
		public ITelemetryCounters TelemetryCounters { get; } = null!;
		
		/// <inheritdoc />
		public void Configure(StatsdConfig config)
		{
		}

		/// <inheritdoc />
		public void Counter(string statName, double value, double sampleRate = 1, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public void Decrement(string statName, int value = 1, double sampleRate = 1, params string[] tags)
		{
		}

		/// <inheritdoc />
		public void Event(string title, string text, string alertType = null!, string aggregationKey = null!, string sourceType = null!,
			int? dateHappened = null, string priority = null!, string hostname = null!, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public void Gauge(string statName, double value, double sampleRate = 1, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public void Histogram(string statName, double value, double sampleRate = 1, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public void Distribution(string statName, double value, double sampleRate = 1, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public void Increment(string statName, int value = 1, double sampleRate = 1, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public void Set<T>(string statName, T value, double sampleRate = 1, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public void Set(string statName, string value, double sampleRate = 1, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public IDisposable StartTimer(string name, double sampleRate = 1, string[] tags = null!)
		{
			// Some random object to satisfy IDisposable
			return new MemoryStream();
		}

		/// <inheritdoc />
		public void Time(Action action, string statName, double sampleRate = 1, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public T Time<T>(Func<T> func, string statName, double sampleRate = 1, string[] tags = null!)
		{
			return func();
		}

		/// <inheritdoc />
		public void Timer(string statName, double value, double sampleRate = 1, string[] tags = null!)
		{
		}

		/// <inheritdoc />
		public void ServiceCheck(string name, Status status, int? timestamp = null, string hostname = null!, string[] tags = null!, string message = null!)
		{
		}
		
		/// <inheritdoc />
		public void Dispose()
		{
		}
	}
}