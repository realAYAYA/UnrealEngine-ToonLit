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
		/// Constructor
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
		/// Constructor
		/// </summary>
		/// <param name="InnerException">The inner exception</param>
		/// <param name="Event">Event to construct from</param>
		public BuildLogEventException(Exception? InnerException, LogEvent Event)
			: base(InnerException, Event.ToString())
		{
			this.Event = Event;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Format">Structured logging format string</param>
		/// <param name="Args">Argument objects</param>
		public BuildLogEventException(string Format, params object[] Args)
			: this(LogEvent.Create(LogLevel.Error, Format, Args))
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InnerException">Inner exception to wrap</param>
		/// <param name="Format">Structured logging format string</param>
		/// <param name="Args">Argument objects</param>
		public BuildLogEventException(Exception? InnerException, string Format, params object[] Args)
			: this(InnerException, LogEvent.Create(LogLevel.Error, default, InnerException, Format, Args))
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="EventId">Event id for the error</param>
		/// <param name="InnerException">Inner exception to wrap</param>
		/// <param name="Format">Structured logging format string</param>
		/// <param name="Args">Argument objects</param>
		public BuildLogEventException(Exception? InnerException, EventId EventId, string Format, params object[] Args)
			: this(InnerException, LogEvent.Create(LogLevel.Error, EventId, InnerException, Format, Args))
		{
		}
	}
}

