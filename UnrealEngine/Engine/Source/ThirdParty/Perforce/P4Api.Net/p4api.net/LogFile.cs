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
 * Name		: LogFile.cs
 *
 * Author	: dbb
 *
 * Description	: Classes used to log diagnostic messages.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace Perforce.P4
{
    /// <summary>
    /// Generic interface used to an event logger. In short, messages are
    /// logged by:
    /// 1) Level. The lower the level, the more sever the level,
    ///     0   fatal
    ///     1   error
    ///     2   warning
    ///     3   information
    ///     4+  debugging messages
    /// 2) Source. A string specifying the origin of the message, ie P4Server
    /// 3) Message. The text of the message.
    /// </summary>
    public class LogFile
    {
        /// <summary>
        /// A user defined Logging Delegate should use this prototype
        /// </summary>
        /// <param name="logLevel">Log level</param>
        /// <param name="source">Source location</param>
        /// <param name="message">Log Message</param>
        public delegate void LogMessageDelgate( int logLevel,
                                                String source,
                                                String message );

        /// <summary>
        /// Function pointer to a LogMessageDelegate
        /// </summary>
        public static LogMessageDelgate ExternalLogFn;

        /// <summary>
        /// Set the logging function to use
        /// </summary>
        /// <param name="logFn">LogMessage function delegate</param>
        public static void SetLoggingFunction( LogMessageDelgate logFn)
        {
            ExternalLogFn = logFn;
        }

        /// <summary>
        /// Create a Log Message
        /// </summary>
        /// <param name="logLevel">Log Level</param>
        /// <param name="source">Source of message</param>
        /// <param name="message">Message to log</param>
        public static void LogMessage(  int logLevel,
                                        String source,
                                        String message )
        {
            try
            {
                if (ExternalLogFn != null)
                {
                    // use user supplied external logging function
                    ExternalLogFn(logLevel, source, message);
                    return;
                }
            }
            catch {} // never fail because of an error writing a log message

            // TODO: Implement an internal logging function
            /*
            DateTime now = DateTime.Now;
            String msg = String.Format("[{0}:{1}] {2} : {3}", 
                logLevel, source,  now.ToString("dd/MM/yyyy HH:mm:ss.ffff"), message);
            */
        }

        /// <summary>
        /// Log an Exception 
        /// </summary>
        /// <param name="category">Category of the exception</param>
        /// <param name="ex">The Exception to log</param>
        public static void LogException( String category, Exception ex)
        {
            try
            {
                String msg = String.Format("{0}:{1}\r\n{2}",
                    ex.GetType().ToString(),
                    ex.Message,
                    ex.StackTrace);
                LogMessage(0, category, msg);

				if (ex.InnerException != null)
					LogException("Inner Exception", ex.InnerException);

				P4Exception p4ex = ex as P4Exception;
				if ((p4ex != null) && (p4ex.NextError != null))
				{
					LogException("Next Exception", p4ex.NextError);
				}
			}
            catch { } // never fail because of an error writing a log message
        }

    }
}
