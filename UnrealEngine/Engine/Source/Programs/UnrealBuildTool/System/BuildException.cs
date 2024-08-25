// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for exceptions thrown by UBT
	/// </summary>
	public class BuildException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Message">The error message to display.</param>
		public BuildException(string Message)
			: base(Message)
		{
		}

		/// <summary>
		/// Constructor which wraps another exception
		/// </summary>
		/// <param name="InnerException">An inner exception to wrap</param>
		/// <param name="Message">The error message to display.</param>
		public BuildException(Exception? InnerException, string Message)
			: base(Message, InnerException)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Format">Formatting string for the error message</param>
		/// <param name="Arguments">Arguments for the formatting string</param>
		public BuildException(string Format, params object?[] Arguments)
			: base(String.Format(Format, Arguments))
		{
		}

		/// <summary>
		/// Constructor which wraps another exception
		/// </summary>
		/// <param name="InnerException">The inner exception being wrapped</param>
		/// <param name="Format">Format for the message string</param>
		/// <param name="Arguments">Format arguments</param>
		public BuildException(Exception InnerException, string Format, params object?[] Arguments)
			: base(String.Format(Format, Arguments), InnerException)
		{
		}

		/// <summary>
		/// Log BuildException with a provided ILogger
		/// </summary>
		/// <param name="Logger">The ILogger to use to log this exception</param>
		public virtual void LogException(ILogger Logger)
		{
			Logger.LogError(this, "{Ex}", ExceptionUtils.FormatException(this));
			Logger.LogDebug(this, "{Ex}", ExceptionUtils.FormatExceptionDetails(this));
		}

		/// <summary>
		/// Returns the string representing the exception. Our build exceptions do not show the callstack since they are used to report known error conditions.
		/// </summary>
		/// <returns>Message for the exception</returns>
		public override string ToString()
		{
			return Message;
		}
	}

	/// <summary>
	/// Implementation of <see cref="BuildException"/> that captures a full structured logging event.
	/// </summary>
	class BuildLogEventException : BuildException
	{
		/// <summary>
		/// The event object
		/// </summary>
		public LogEvent Event { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Event">Event to construct from</param>
		public BuildLogEventException(LogEvent Event)
			: this(null, Event)
		{
		}

		/// <summary>
		/// Constructor which wraps another exception
		/// </summary>
		/// <param name="InnerException">The inner exception</param>
		/// <param name="Event">Event to construct from</param>
		public BuildLogEventException(Exception? InnerException, LogEvent Event)
			: base(InnerException, Event.ToString())
		{
			this.Event = Event;
		}

		/// <inheritdoc/>
		public BuildLogEventException(string Format, params object[] Arguments)
			: this(LogEvent.Create(LogLevel.Error, Format, Arguments))
		{
		}

		/// <inheritdoc/>
		public BuildLogEventException(Exception? InnerException, string Format, params object[] Arguments)
			: this(InnerException, LogEvent.Create(LogLevel.Error, default, InnerException, Format, Arguments))
		{
		}

		/// <summary>
		/// Constructor which wraps another exception
		/// </summary>
		/// <param name="EventId">Event id for the error</param>
		/// <param name="InnerException">Inner exception to wrap</param>
		/// <param name="Format">Structured logging format string</param>
		/// <param name="Arguments">Argument objects</param>
		public BuildLogEventException(Exception? InnerException, EventId EventId, string Format, params object[] Arguments)
			: this(InnerException, LogEvent.Create(LogLevel.Error, EventId, InnerException, Format, Arguments))
		{
		}

		/// <inheritdoc/>
		public override void LogException(ILogger Logger)
		{
			Logger.Log(Event.Level, Event.Id, Event, this, (s, e) => s.ToString());
			Logger.LogDebug(this, "{Ex}", ExceptionUtils.FormatExceptionDetails(this));
		}
	}

	/// <summary>
	/// Implementation of <see cref="BuildLogEventException"/> that will return a unique exit code.
	/// </summary>
	class CompilationResultException : BuildLogEventException
	{
		/// <summary>
		/// The exit code associated with this exception
		/// </summary>
		public CompilationResult Result { get; }

		readonly bool HasMessage = true;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Result">The resulting exit code</param>
		public CompilationResultException(CompilationResult Result)
			: base(LogEvent.Create(LogLevel.Error, "{CompilationResult}", Result))
		{
			HasMessage = false;
			this.Result = Result;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Result">The resulting exit code</param>
		/// <param name="Event">Event to construct from</param>
		public CompilationResultException(CompilationResult Result, LogEvent Event)
			: base(Event)
		{
			this.Result = Result;
		}

		/// <summary>
		/// Constructor which wraps another exception
		/// </summary>
		/// <param name="Result">The resulting exit code</param>
		/// <param name="InnerException">The inner exception</param>
		/// <param name="Event">Event to construct from</param>
		public CompilationResultException(CompilationResult Result, Exception? InnerException, LogEvent Event)
			: base(InnerException, Event)
		{
			this.Result = Result;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Result">The resulting exit code</param>
		/// <param name="Format">Formatting string for the error message</param>
		/// <param name="Arguments">Arguments for the formatting string</param>
		public CompilationResultException(CompilationResult Result, string Format, params object[] Arguments)
			: base(LogEvent.Create(LogLevel.Error, Format, Arguments))
		{
			this.Result = Result;
		}

		/// <summary>
		/// Constructor which wraps another exception
		/// </summary>
		/// <param name="Result">The resulting exit code</param>
		/// <param name="InnerException">The inner exception being wrapped</param>
		/// <param name="Format">Format for the message string</param>
		/// <param name="Arguments">Format arguments</param>
		public CompilationResultException(CompilationResult Result, Exception? InnerException, string Format, params object[] Arguments)
			: base(InnerException, LogEvent.Create(LogLevel.Error, default, InnerException, Format, Arguments))
		{
			this.Result = Result;
		}

		/// <summary>
		/// Constructor which wraps another exception
		/// </summary>
		/// <param name="Result">The resulting exit code</param>
		/// <param name="EventId">Event id for the error</param>
		/// <param name="InnerException">Inner exception to wrap</param>
		/// <param name="Format">Structured logging format string</param>
		/// <param name="Arguments">Argument objects</param>
		public CompilationResultException(CompilationResult Result, Exception? InnerException, EventId EventId, string Format, params object[] Arguments)
			: base(InnerException, LogEvent.Create(LogLevel.Error, EventId, InnerException, Format, Arguments))
		{
			this.Result = Result;
		}

		/// <inheritdoc/>
		public override void LogException(ILogger Logger)
		{
			if (HasMessage)
			{
				Logger.Log(Event.Level, Event.Id, Event, this, (s, e) => s.ToString());
			}
			Logger.LogDebug(this, "{Ex}", ExceptionUtils.FormatExceptionDetails(this));
		}
	}
}

