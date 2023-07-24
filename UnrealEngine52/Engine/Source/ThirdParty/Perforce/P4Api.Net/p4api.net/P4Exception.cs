/*******************************************************************************

Copyright (c) 2010, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: P4Exception.cs
 *
 * Author	: dbb
 *
 * Description	: Classes used to provide typed exception handling.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Base class for exceptions caused by run time errors from the server.
	/// They can be disabled, by setting the MinThrowLevel to 
	/// ErrorSeverity.E_NOEXC.
	/// </summary>
	public class P4Exception : Exception
	{
		static ErrorSeverity minLevel = ErrorSeverity.E_FAILED;

		private P4Exception nextError;
		private ErrorSeverity errorLevel;
		private int errorCode;
		private string message;
		private P4ClientInfoMessageList details;
		private string cmdLine;

		/// <summary>
		/// If more than one error was returned by a command, the next
		/// error in the list.
		/// </summary>
		public P4Exception NextError { get { return nextError; } }
		/// <summary>
		/// Severity of the error
		/// </summary>
		public ErrorSeverity ErrorLevel { get { return errorLevel; } }
		/// <summary>
		/// Error number from the C++ API
		/// </summary>
		public int ErrorCode { get { return errorCode; } }
		/// <summary>
		/// Error message
		/// </summary>
		public override string Message { get { return message; } }
		/// <summary>
		/// Error message
		/// </summary>
		public P4ClientInfoMessageList Details { get { return details; } }
		/// <summary>
		/// Error message
		/// </summary>
		public string CmdLine { get { return cmdLine; } }

		/// <summary>
		/// Create a new P4Exception
		/// </summary>
		/// <param name="nLevel">Severity level</param>
		/// <param name="nMessage">Error message</param>
		public P4Exception(ErrorSeverity nLevel, String nMessage)
		{
			errorCode = 0;
			errorLevel = nLevel;
			message = nMessage;
			nextError = null;
		}

		/// <summary>
		/// Create a new P4Exception
		/// </summary>
		/// <param name="nLevel">Severity level</param>
		/// <param name="nMessage">Error message</param>
		/// <param name="NextError">Next error in sequence</param>
		public P4Exception(ErrorSeverity nLevel, String nMessage, P4Exception NextError)
		{
			errorCode = 0;
			errorLevel = nLevel;
			message = nMessage;
			nextError = NextError;
		}

        /// <summary>
        /// Create a new P4Exception
        /// </summary>
        /// <param name="cmd">command which was run</param>
        /// <param name="args">arguments to the command</param>
        /// <param name="nLevel">Severity level</param>
        /// <param name="nMessage">Error message</param>
        public P4Exception(string cmd, string[] args, ErrorSeverity nLevel, String nMessage)
		{
			errorCode = 0;
			errorLevel = nLevel;
			message = nMessage;
			nextError = null;

			cmdLine = cmd;
			if (args != null)
			{
				for (int idx = 0; idx < args.Length; idx++)
				{
					if (string.IsNullOrEmpty(args[idx]) == false)
					{
						cmdLine += " " + args[idx];
					}
				}
			}
		}

		/// <summary>
		/// Create a new P4Exception
		/// </summary>
		/// <param name="error">Client error causing the exception</param>
		public P4Exception(P4ClientError error)
		{
			errorCode = error.ErrorCode;
			errorLevel = error.SeverityLevel;
			message = error.ErrorMessage;
			nextError = null;
		}

        /// <summary>
        /// Create a new P4Exception
        /// </summary>
        /// <param name="cmd">command which was run</param>
        /// <param name="args">arguments passed to the command</param>
        /// <param name="error">Client error causing the exception</param>
        public P4Exception(string cmd, string[] args, P4ClientError error)
		{
			errorCode = error.ErrorCode;
			errorLevel = error.SeverityLevel;
			message = error.ErrorMessage;
			nextError = null;

			cmdLine = cmd;
			if (args != null)
			{
				for (int idx = 0; idx < args.Length; idx++)
				{
					if (string.IsNullOrEmpty(args[idx]) == false)
					{
						cmdLine += " " + args[idx];
					}
				}
			}
		}

		/// <summary>
		/// Create a list of new P4Exceptions
		/// </summary>
		/// <param name="errors">The list of errors which caused the exception</param>
		public P4Exception(P4ClientErrorList errors)
		{
			if (errors.Count < 1)
				return;
			errorLevel = errors[0].SeverityLevel;
			errorCode = errors[0].ErrorCode;
			message = errors[0].ErrorMessage;
			P4Exception currentException = this;
			for (int idx = 1; idx < errors.Count; idx++)
			{
				currentException.nextError = new P4Exception(errors[idx]);
				currentException = currentException.nextError;
			}
		}


        /// <summary>
        /// Create a list of new P4Exceptions
        /// </summary>
        /// <param name="cmd">command which was run</param>
        /// <param name="args">arguments to the command</param>
        /// <param name="errors">The list of errors which caused the exception</param>
        public P4Exception(string cmd, string[] args, P4ClientErrorList errors)
		{
			if (errors.Count < 1)
				return;
			cmdLine = cmd;
			if (args != null)
			{
				for (int idx = 0; idx < args.Length; idx++)
				{
					if (string.IsNullOrEmpty(args[idx]) == false)
					{
						cmdLine += " " + args[idx];
					}
				}
			}
			errorLevel = errors[0].SeverityLevel;
			errorCode = errors[0].ErrorCode;
			message = errors[0].ErrorMessage;
			P4Exception currentException = this;
			for (int idx = 1; idx < errors.Count; idx++)
			{
				currentException.nextError = new P4Exception(errors[idx]);
				currentException = currentException.nextError;
			}
		}

		/// <summary>
		/// Create a list of new P4Exceptions
		/// </summary>
		/// <param name="errors">The list of errors which caused the exception</param>
		/// <param name="nDetails">The info output of the command which caused the exception</param>
		internal P4Exception(P4ClientErrorList errors, P4ClientInfoMessageList nDetails)
		{
			if (errors.Count < 1)
				return;
			errorLevel = errors[0].SeverityLevel;
			errorCode = errors[0].ErrorCode;
			message = errors[0].ErrorMessage;
			P4Exception currentException = this;
			for (int idx = 1; idx < errors.Count; idx++)
			{
				currentException.nextError = new P4Exception(errors[idx]);
				currentException = currentException.nextError;
			}
			details = nDetails;
		}

        /// <summary>
        /// Create a list of new P4Exceptions
        /// </summary>
        /// <param name="cmd">command which was run</param>
        /// <param name="args">arguments to the command</param>
        /// <param name="errors">The list of errors which caused the exception</param>
        /// <param name="nDetails">The info output of the command which caused the exception</param>
        internal P4Exception(string cmd, string[] args, P4ClientErrorList errors, P4ClientInfoMessageList nDetails)
		{
			if (errors.Count < 1)
				return;
			cmdLine = cmd;
			if (args != null)
			{
				for (int idx = 0; idx < args.Length; idx++)
				{
					if (string.IsNullOrEmpty(args[idx]) == false)
					{
						cmdLine += " " + args[idx];
					}
				}
			}
			errorLevel = errors[0].SeverityLevel;
			errorCode = errors[0].ErrorCode;
			message = errors[0].ErrorMessage;
			P4Exception currentException = this;
			for (int idx = 1; idx < errors.Count; idx++)
			{
				currentException.nextError = new P4Exception(errors[idx]);
				currentException = currentException.nextError;
			}
			details = nDetails;
		}

		/// <summary>
		/// Minimum error to cause an exception to be thrown
		/// </summary>
		public static ErrorSeverity MinThrowLevel
		{
			get { return minLevel; }
			set { minLevel = value; }
		}

		/// <summary>
		/// Create and throw an exception if it exceeds the MinThrowLevel
		/// </summary>
		/// <param name="nLevel">Severity level</param>
		/// <param name="nMessage">Error message</param>
		public static void Throw(ErrorSeverity nLevel, String nMessage)
		{
			if (nLevel >= minLevel)
				throw new P4Exception(nLevel, nMessage);
		}

        /// <summary>
        /// Create and throw an exception if it exceeds the MinThrowLevel
        /// </summary>
        /// <param name="cmd">command which was run</param>
        /// <param name="args">arguments to the command</param>
        /// <param name="nLevel">Severity level</param>
        /// <param name="nMessage">Error message</param>
        public static void Throw(string cmd, string[] args, ErrorSeverity nLevel, String nMessage)
		{
			if (nLevel >= minLevel)
				throw new P4Exception(cmd, args, nLevel, nMessage);
		}

		/// <summary>
		/// Create and throw an exception if it exceeds the MinThrowLevel
		/// </summary>
		/// <param name="error">Client error causing the exception</param>
		public static void Throw(P4ClientError error)
		{
			if (error.SeverityLevel >= minLevel)
				throw new P4Exception(error);
		}

        /// <summary>
        /// Create and throw an exception if it exceeds the MinThrowLevel
        /// </summary>
        /// <param name="cmd">command which was run</param>
        /// <param name="args">arguments to the command</param>
        /// <param name="error">Client error causing the exception</param>
        public static void Throw(string cmd, string[] args, P4ClientError error)
		{
			if (error.SeverityLevel >= minLevel)
				throw new P4Exception(cmd, args, error);
		}

		/// <summary>
		/// Throw if any error in the list exceeds minLevel
		/// </summary>
		/// <param name="errors">List of client errors causing the exception</param>
		internal static void Throw(P4ClientErrorList errors)
		{
			foreach (P4ClientError current in errors)
			{
				if (current.SeverityLevel >= minLevel)
					throw new P4Exception(errors);
			}
		}

        /// <summary>
        /// Throw if any error in the list exceeds minLevel
        /// </summary>
        /// <param name="cmd">command which was run</param>
        /// <param name="args">arguments to the command</param>
        /// <param name="errors">List of client errors causing the exception</param>
        internal static void Throw(string cmd, string[] args, P4ClientErrorList errors)
		{
			foreach (P4ClientError current in errors)
			{
				if (current.SeverityLevel >= minLevel)
					throw new P4Exception(cmd, args, errors);
			}
		}

        /// <summary>
        /// Throw if any error in the list exceeds minLevel
        /// </summary>
        /// <param name="errors">List of client errors causing the exception</param>
        /// <param name="details">P4ClientInfoMessageList details</param>
        internal static void Throw(P4ClientErrorList errors, P4ClientInfoMessageList details)
		{
			foreach (P4ClientError current in errors)
			{
				if (current.SeverityLevel >= minLevel)
					throw new P4Exception(errors, details);
			}
		}

        /// <summary>
        /// Throw if any error in the list exceeds minLevel
        /// </summary>
        /// <param name="cmd"></param>
        /// <param name="args"></param>
        /// <param name="errors">List of client errors causing the exception</param>
        /// <param name="details">P4ClientInfoMessageList details</param>
        internal static void Throw(string cmd, string[] args, P4ClientErrorList errors, P4ClientInfoMessageList details)
		{
			foreach (P4ClientError current in errors)
			{
				if (current.SeverityLevel >= minLevel)
					throw new P4Exception(cmd, args, errors, details);
			}
		}
	}
    /// <summary>
    /// Specialized Exception for lost connection
    /// </summary>
	public class P4LostConnectionException : P4Exception
	{
        /// <summary>
        /// Construct a P4LostConnectionException
        /// </summary>
        /// <param name="nLevel">severity level</param>
        /// <param name="nMessage">exception message</param>
		public P4LostConnectionException(ErrorSeverity nLevel, String nMessage)
			: base(nLevel, nMessage)
		{
		}
	}

    /// <summary>
    /// Specialized Exception for command time out
    /// </summary>
	public class P4CommandTimeOutException : P4Exception
	{
        /// <summary>
        /// Construct a P4CommandTimeOutException
        /// </summary>
        /// <param name="nLevel">severity level</param>
        /// <param name="nMessage">exception message</param>
		public P4CommandTimeOutException(ErrorSeverity nLevel, String nMessage)
			: base(nLevel, nMessage)
		{
		}
	}

    /// <summary>
    /// Specialized Exception for canceled command
    /// </summary>
    public class P4CommandCanceledException : P4Exception
    {
        /// <summary>
        /// Construct a P4CommandCanceled Exception
        /// </summary>
        /// <param name="nMessage">exception message</param>
        public P4CommandCanceledException(String nMessage)
            : base(ErrorSeverity.E_FATAL, nMessage)
        {
        }
    }

    /// <summary>
    /// Specialized Exception for Hung command
    /// </summary>
    public class P4HungCommandCancelException : P4Exception
    {
        /// <summary>
        /// Construct a P4HungCommandCanceled Exception
        /// </summary>
        /// <param name="nMessage">exception message</param>
        public P4HungCommandCancelException(String nMessage)
            : base(ErrorSeverity.E_FATAL, nMessage)
        {
        }
    }

    /// <summary>
    /// Specialized Exception for Can't close exception
    /// </summary>
	public class P4CantCloseConnectionException : P4Exception
	{
        /// <summary>
        /// Construct a P4CantCloseConnectionException
        /// </summary>
		public P4CantCloseConnectionException()
			: base(ErrorSeverity.E_FAILED, "Can't close conection, the server is still running commands")
		{
		}
	}

    /// <summary>
    /// Specialized Exception to handle Trust failures
    /// </summary>
	public class P4TrustException : P4Exception
	{
        /// <summary>
        /// Create a P4TrustException
        /// </summary>
		public P4TrustException()
			: base(ErrorSeverity.E_FATAL, "Trust Issue")
		{
		}

        /// <summary>
        /// Create a P4TrustException
        /// </summary>
        /// <param name="nLevel">severity level</param>
        /// <param name="nMessage">exception message</param>
		public P4TrustException(ErrorSeverity nLevel, String nMessage)
			: base(nLevel, nMessage)
		{
		}
	}
}
