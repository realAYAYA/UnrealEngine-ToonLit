// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Runtime.InteropServices;

namespace EpicGames.UBA
{
	internal class LoggerImpl : ILogger
	{
		delegate void BeginScopeCallback();
		delegate void EndScopeCallback();
		delegate void LogCallback(LogEntryType type, IntPtr str, uint len);

		IntPtr _handle = IntPtr.Zero;
		readonly Microsoft.Extensions.Logging.ILogger _logger;
		readonly BeginScopeCallback _beginScopeCallbackDelegate;
		readonly EndScopeCallback _endScopeCallbackDelegate;
		readonly LogCallback _logCallbackDelegate;

		#region DllImport
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr GetDefaultLogWriter();

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr CreateCallbackLogWriter(BeginScopeCallback begin, EndScopeCallback end, LogCallback log);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void DestroyCallbackLogWriter(IntPtr logger);
		#endregion

		public LoggerImpl(Microsoft.Extensions.Logging.ILogger logger, bool showDetail)
		{
			_logger = logger;
			_beginScopeCallbackDelegate = BeginScope;
			_endScopeCallbackDelegate = EndScope;
			_logCallbackDelegate = Log;
			_handle = CreateCallbackLogWriter(_beginScopeCallbackDelegate, _endScopeCallbackDelegate, _logCallbackDelegate);
		}

		#region IDisposable
		~LoggerImpl() => Dispose(false);

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
			}

			if (_handle != IntPtr.Zero)
			{
				DestroyCallbackLogWriter(_handle);
				_handle = IntPtr.Zero;
			}
		}
		#endregion

		#region ILogger

		public IntPtr GetHandle() => _handle;

		public void BeginScope()
		{
		}

		public void EndScope()
		{
		}

		public void Log(LogEntryType type, string message)
		{
			switch (type)
			{
				case LogEntryType.Error: _logger.LogError("{Message}", message); break;
				case LogEntryType.Warning: _logger.LogWarning("{Message}", message); break;
				case LogEntryType.Info: _logger.LogInformation("{Message}", message); break;
				case LogEntryType.Detail: _logger.LogDebug("{Message}", message); break;
				case LogEntryType.Debug: _logger.LogDebug("{Message}", message); break;
			}
		}
		#endregion

		void Log(LogEntryType type, IntPtr ptr, uint len) => Log(type, Marshal.PtrToStringAuto(ptr, (int)len) ?? String.Empty);
	}
}