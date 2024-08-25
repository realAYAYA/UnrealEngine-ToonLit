// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics.Metrics;
using JetBrains.Annotations;

namespace Jupiter.Common
{
	public class Gauge<T> where T : struct
	{
		[System.Diagnostics.CodeAnalysis.SuppressMessage("CodeQuality", "IDE0052:Remove unread private members", Justification = "Needed to keep the callback alive")] [UsedImplicitly]
		private readonly ObservableGauge<T> _observableGauge;
		private readonly List<Measurement<T>> _measurements = new List<Measurement<T>>();
		private readonly object _lock = new object();

		public Gauge(Meter meter, string name)
		{
			_observableGauge = meter.CreateObservableGauge(name, GetMeasurements);
		}

		private IEnumerable<Measurement<T>> GetMeasurements()
		{
			List<Measurement<T>> measurements;
			lock (_lock)
			{
				measurements = new List<Measurement<T>>(_measurements);
				_measurements.Clear();
			}
			return measurements;
		}

		public void Record(T measurement, IEnumerable<KeyValuePair<string, object?>> tags)
		{
			lock (_lock)
			{
				_measurements.Add(new Measurement<T>(measurement, tags));
			}
		}
	}

	public static class MeterExtensions
	{
		public static Gauge<T> CreateGauge<T>(this Meter m, string name) where T : struct
		{
			Gauge<T> gauge = new Gauge<T>(m, name);
			return gauge;
		}
	}
}
