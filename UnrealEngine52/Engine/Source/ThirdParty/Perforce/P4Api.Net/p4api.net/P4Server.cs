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
 * Name		: P4Server.cs
 *
 * Author	: dbb
 *
 * Description	: Classes used to wrap calls in the P4Bridge DLL in C#.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Text;
using System.IO;
using System.Timers;
using System.Threading;
using System.Diagnostics;

namespace Perforce.P4
{
    /// <summary>
    /// Allows a client to monitor the execution of a command. It allow the client to 
    /// cancel the command if it takes to long to complete or display a UI to allow the 
    /// user to cancel the command.
    /// </summary>
    public interface IKeepAlive
    {
        /// <summary>
        /// A command is starting
        /// </summary>
        /// <param name="server">Server running the command</param>
        /// <param name="cmdId">CmdId of the command</param>
        /// <param name="cmdRunThread">Thread to run on</param>
        /// <param name="cmdLine">Command Line for the command for display purposes</param>
        /// <returns></returns>
        bool StartQueryCancel(P4Server server, uint cmdId, Thread cmdRunThread, string cmdLine);
        // Note, it is OK for the API might send this multiple times with the,
        // same cmdId to ensure that any UI displayed by the client is dismissed.
        /// <summary>
        /// The command has completed dismiss any UI or timeouts
        /// </summary>
        /// <param name="cmdId">CmdId of the command that completed</param>
        /// <remarks>
        /// Note, it is OK for the API might send this multiple times with the,
        /// same cmdId to ensure that any UI displayed by the client iss dismissed.
        /// </remarks>
        void CommandCompleted(uint cmdId);
    }

    /// <summary>
    /// P4Server
    /// 
    /// Represents the connection to a Perforce Server using the the P4 Bridge 
    /// DLL. It wraps the calls exported by the DLL and transforms the data
    /// types exported by the DLL as native C#.NET data types.
    /// </summary>
    public partial class P4Server : IDisposable
    {
        public IKeepAlive KeepAlive { get; set; }

        internal object Sync = new object();

        private Dictionary<int, P4CommandResult> _lastResultsCache;
        /// <summary>
        /// The results of the last command executed on this thread
        /// </summary>
        public P4CommandResult LastResults
        {
            get
            {
                if (_lastResultsCache.ContainsKey(System.Threading.Thread.CurrentThread.ManagedThreadId))
                {
                    return _lastResultsCache[System.Threading.Thread.CurrentThread.ManagedThreadId];
                }
                return null;
            }
            internal set
            {
                if (_lastResultsCache == null)
                {
                    _lastResultsCache = new Dictionary<int, P4CommandResult>();
                }
                lock (_lastResultsCache)
                {
                    if (_lastResultsCache.Count > 32)
                    {
                        // if the results cach is getting large, throw awy anything older than 10 seconds
                        int[] keys = new int[_lastResultsCache.Keys.Count];
                        _lastResultsCache.Keys.CopyTo(keys, 0);
                        DateTime old = DateTime.Now - TimeSpan.FromSeconds(10);
                        foreach (int key in keys)
                        {
                            if (_lastResultsCache[key].TimeStamp < old)
                            {
                                Debug.Trace(string.Format("Throwing away results for thread, {0}", key));

                                _lastResultsCache.Remove(key);
                            }
                        }
                    }
                    _lastResultsCache[System.Threading.Thread.CurrentThread.ManagedThreadId] = value;
                }
            }
        }

        /// <summary>
        /// Get the version of the p4.net assembly
        /// </summary>
        public static System.Version Version
        {
            get
            {
                return System.Reflection.Assembly.GetExecutingAssembly().GetName().Version;
            }
        }

        /// <summary>
        /// Get the error message generated by the previous connection (if any)
        /// </summary>
        public static P4ClientError ConnectionErrorInt
        {
            get
            {
                IntPtr pErr = P4Bridge.GetConnectionError();
                if (pErr != IntPtr.Zero)
                {
                    P4ClientError pRet = new P4ClientError(null, pErr);
                    P4Bridge.ClearConnectionError();
                    return pRet;
                }
                return null;
            }
        }

        /// <summary>
        /// Get the error message generated by the previous connection (if any)
        /// from the bridge dll.
        /// </summary>
        public P4ClientError ConnectionError { get; private set; }


        private void LogBridgeMessage(int log_level,
                                        String file,
                                        int line,
                                        String message)
        {
            Debug.Trace(String.Format("{0}:{1} {2}", file, line, message));

            // remove the full path to the source, keep just the file name
            String fileName = Path.GetFileName(file);

            string category = String.Format("P4Bridge({0}:{1})", fileName, line);

            LogFile.LogMessage(log_level, category, message);
        }

        private string _server;
        private string _user;
        private string _pass;
        private string _ws_client;
        private string _prog_name;
        private string _prog_ver;
        private string _cwd;

        // EPIC: this isn't thread safe in native code, so we use a static logging functon
        // private P4CallBacks.LogMessageDelegate logfn = null;

        /// <summary>
        /// Create a P4BridgeServer used to connect to the specified P4Server
        /// </summary>
        /// <param name="server">Host:port for the P4 server.</param>
        /// <param name="user">User name for the login. 
        ///     Can be null/blank if only running commands that do not require 
        ///     a login.</param>
        /// <param name="pass">Password for  the login. Can be null/blank if 
        ///     only running commands that do not require a login.</param>
        /// <param name="ws_client">Workspace (client) to be used by the 
        ///     connection. Can be null/blank if only running commands that do 
        ///     not require a login.</param>
        public P4Server(String server,
                         String user,
                         String pass,
                         String ws_client)
            : this(server, user, pass, ws_client, null)
        {
        }

        /// <summary>
        /// Create a P4BridgeServer using the PUC specified by the environment
        ///		or a p4config file if one exists.
        /// </summary>
        /// <param name="cwd">Current working Directory. Can be null/blank if 
        ///		not connecting to the Perforce server using a P4Config file.</param>
        public P4Server(String cwd)
            : this(null, null, null, null, cwd)
        {
        }
        /// <summary>
        /// Create a P4BridgeServer used to connect to the specified P4Server
        /// </summary>
        /// <param name="server">Host:port for the P4 server.</param>
        /// <param name="user">User name for the login. 
        ///     Can be null/blank if only running commands that do not require 
        ///     a login.</param>
        /// <param name="pass">Password for  the login. Can be null/blank if 
        ///     only running commands that do not require a login.</param>
        /// <param name="ws_client">Workspace (client) to be used by the 
        ///     connection. Can be null/blank if only running commands that do 
        ///     not require a login.</param>
        /// <param name="cwd">Current working Directory. Can be null/blank if 
        ///		not connecting to the Perforce server using a P4Config file.</param>
        internal P4Server(String server,
                         String user,
                         String pass,
                         String ws_client,
                         String cwd)
        {
            RunCmdLastContactMap = new Dictionary<uint, CmdContactTimer>();
            // by default we are owned by the creating thread
            SetThreadOwner(Thread.CurrentThread.ManagedThreadId);

            if (string.IsNullOrEmpty(cwd) == false)
            {
                string tmpServer, tmpUser, tmpClient;
                ConnectionInfoFromPath(cwd, out tmpServer, out tmpUser, out tmpClient);
                server = (server == null) ? tmpServer : server;
                user = (user == null) ? tmpUser : user;
                ws_client = (ws_client == null) ? tmpClient : ws_client;
            }
            _server = server;
            _user = user;
            _pass = pass;
            _ws_client = ws_client;
            _cwd = cwd;
            isUnicode = false;

            CurrentEncodeing = P4Encoding.ASCII;

            // connect to the server without passing user/client/password, that way 
            // we can determine if the target server is Unicode enabled or not, so we
            // can use the correct encoding for those parameters

            // EPIC: removing logfn as it is not thread safe
            /*
            if (logfn == null)
            {
                logfn = new P4CallBacks.LogMessageDelegate(LogBridgeMessage);
            }
            */
            pServer = P4Bridge.ConnectA(server, _user, null, null, null /*logfn*/);
            

            if (pServer != IntPtr.Zero)
            {
                if (isUnicode = P4Bridge.IsUnicode(pServer))
                {
                    // if the server supports Unicode, encode the username, password, and workspace 
                    // name in UTF-8
                    using (PinnedByteArray pCwd = MarshalStringToIntPtr(cwd),
                                           pUser = MarshalStringToIntPtr(user),
                                           pClient = MarshalStringToIntPtr(ws_client))
                    {
                        P4Bridge.set_cwdW(pServer, pCwd);
                        P4Bridge.set_userW(pServer, pUser);
                        P4Bridge.set_clientW(pServer, pClient);
                    }
                }
                else
                {
                    // if the server does not support Unicode, pass the username, password, and 
                    // workspace name in ASCII
                    P4Bridge.set_cwdA(pServer, cwd);
                    P4Bridge.set_userA(pServer, user);
                    P4Bridge.set_clientA(pServer, ws_client);
                }
            }
            if (pServer == IntPtr.Zero)
            {

                ConnectionError = ConnectionErrorInt;
                if (ConnectionError == null)
                {
                    IntPtr pObj = P4Bridge.GetErrorResults(pServer, 0);
                    P4ClientErrorList _errorList = new P4ClientErrorList(this, pObj);

                    if ((_errorList != null) && (_errorList.Count > 0))
                    {
                        P4Exception.Throw(_errorList, GetInfoResults(0));
                    }
                    ConnectionError = new P4ClientError(ErrorSeverity.E_FATAL, string.Format("Unknown error connecting to server, {0}", _server));
                }
                P4Exception.Throw(ConnectionError);
                return;
                //throw new ApplicationException(connectionError);
            }
            else
            {
                ConnectionError = null;
            }


            if (isUnicode)
            {
                CurrentEncodeing = P4Encoding.utf8;

                string charset = CharacterSet;
                string gotCharset = P4Server.Get("P4CHARSET");
                if (charset.Equals("none") && gotCharset != null)
                {
                    charset = gotCharset;
                }
                else
                {
                    charset = "utf8";
                }

                SetCharacterSet(charset, "utf8");
            }
            apiLevel = P4Bridge.APILevel(pServer);
            requiresLogin = P4Bridge.UseLogin(pServer);

            if ((!requiresLogin) && (!string.IsNullOrEmpty(pass)))
            {
                using (PinnedByteArray pPass = MarshalStringToIntPtr(pass))
                {
                    P4Bridge.set_passwordW(pServer, pPass);
                }
            }
            // Link the callbacks from the bridge dll to their corresponding events
            SetInfoResultsCallback();
            SetTaggedOutputCallback();
            SetErrorCallback();
            SetTextResultsCallback();
            SetBinaryResultsCallback();
            SetPromptCallback();
            SetResolveCallback();
            SetResolveACallback();
            SetParallelTransferCallback();

            // If we were supplied a password, login into the server. If the 
            // server requires a login (security level >= 3), this will prompt
            // for the password. If login is not required, the command will just
            // return with a result saying that login is not required.
            //Login(pass, null);
        }

        /// <summary>
        /// Create a P4BridgeServer used to connect to the specified P4Server
        /// </summary>
        /// <param name="server">Host:port for the P4 server.</param>
        /// <param name="user">User name for the login. 
        ///     Can be null/blank if only running commands that do not require 
        ///     a login.</param>
        /// <param name="pass">Password for  the login. Can be null/blank if 
        ///     only running commands that do not require a login.</param>
        /// <param name="ws_client">Workspace (client) to be used by the 
        ///     connection. Can be null/blank if only running commands that do 
        ///     not require a login.</param>
        /// <param name="cwd">Current working directory</param>
        /// <param name="trust_flag">Trust or not</param>
        /// <param name="fingerprint">Fingerprint to trust</param>
        public P4Server(String server,
                         String user,
                         String pass,
                         String ws_client,
                         String cwd,
                         String trust_flag,
                         String fingerprint)
        {
            RunCmdLastContactMap = new Dictionary<uint, CmdContactTimer>();

            _server = server;
            _user = user;
            _pass = pass;
            _ws_client = ws_client;
            _cwd = cwd;

            CurrentEncodeing = P4Encoding.ASCII;

            // EPIC: Another broken logfn, this guy gets GC'd and is also not thread safe, so it is gone
            /*
            P4CallBacks.LogMessageDelegate logfn =
                new P4CallBacks.LogMessageDelegate(LogBridgeMessage);
            */

            // encode the username, password, and workspace name in UTF-8, we
            // won't know if the client supports Unicode until after connect 
            // returns
            using (PinnedByteArray pUser = MarshalStringToIntPtr(user),
                                   pClient = MarshalStringToIntPtr(ws_client))
            {
                // Epic: see comment above
                /* 
                IntPtr pLogFn = IntPtr.Zero;
                if (logfn != null)
                    pLogFn = Marshal.GetFunctionPointerForDelegate(logfn);
                */

                pServer = P4Bridge.TrustedConnectW(server, pUser, IntPtr.Zero, pClient, trust_flag, fingerprint, IntPtr.Zero /*pLogFn*/);

            }

            if (pServer == IntPtr.Zero)
            {

                ConnectionError = ConnectionErrorInt;
                if (ConnectionError == null)
                {
                    IntPtr pObj = P4Bridge.GetErrorResults(pServer, 0);
                    P4ClientErrorList _errorList = new P4ClientErrorList(this, pObj);

                    if ((_errorList != null) && (_errorList.Count > 0))
                    {
                        P4Exception.Throw(_errorList, GetInfoResults(0));
                    }
                    ConnectionError = new P4ClientError(ErrorSeverity.E_FAILED,
                        string.Format("Unknown error connecting to server, {0}", _server));
                }
                P4Exception.Throw(ConnectionError);
                return;
            }
            else
            {
                ConnectionError = null;
            }


            if (isUnicode = P4Bridge.IsUnicode(pServer))
            {
                CurrentEncodeing = P4Encoding.utf8;

                //SetCharacterSet(Charset[(int)CurrentEncodeing], Charset[(int)P4Encoding.utf16bom]);
                SetCharacterSet("utf8", "utf16");
            }
            apiLevel = P4Bridge.APILevel(pServer);
            requiresLogin = P4Bridge.UseLogin(pServer);

            if ((!requiresLogin) && (!string.IsNullOrEmpty(pass)))
            {
                using (PinnedByteArray pPass = MarshalStringToIntPtr(pass))
                {
                    P4Bridge.set_passwordW(pServer, pPass);
                }
            }
            // Link the callbacks from the bridge dll to their corresponding events
            SetInfoResultsCallback();
            SetTaggedOutputCallback();
            SetErrorCallback();
            SetTextResultsCallback();
            SetBinaryResultsCallback();
            SetPromptCallback();
            SetResolveCallback();
            SetResolveACallback();
            SetParallelTransferCallback();

            // If we were supplied a password, login into the server. If the 
            // server requires a login (security level >= 3), this will prompt
            // for the password. If login is not required, the command will just
            // return with a result saying that login is not required.
            //Login(pass, null);
        }

        /// <summary>
        /// Run a login command on the server
        /// </summary>
        /// <param name="password">User's password</param>
        /// <param name="options">options/flags</param>
        /// <returns>Success/Failure</returns>
        /// <remarks>
        /// If the server requires a login (security level >= 3), this will 
        /// prompt for the password. If login is not required, the command will
        /// just return with a result saying that login is not required.
        /// </remarks>
        public bool Login(string password, StringList options)
        {
            if (!requiresLogin)
            {
                // server does not support login command
                if (!string.IsNullOrEmpty(password))
                {
                    using (PinnedByteArray pPass = MarshalStringToIntPtr(password))
                    {
                        P4Bridge.set_passwordW(pServer, pPass);
                    }
                    return true;
                }
                return false;
            }
            P4Command login = new P4Command(this, "login", false);
            login.Responses = new Dictionary<string, string>();
            login.Responses["DefaultResponse"] = password;
            P4CommandResult results;
            try
            {
                results = login.Run(options);
            }
            catch
            {
                results = null;
                return false;
            }
            if (!results.Success)
            {
                ConnectionError = results.ErrorList[0];
            }
            else
            {
                ConnectionError = null;
            }
            return results.Success;
        }

        /// <summary>
        /// Run a logout command on the server
        /// </summary>
        /// <param name="options">The -a flag invalidates the ticket on the server.</param>
        /// <param name="user">The user to log out (requires super access)</param>
        /// <returns>Success/Failure</returns>
        /// <remarks>
        /// If the server requires a login (security level >= 3), this will 
        /// logout the user and remove the local ticket.
        /// </remarks>
        public bool Logout(StringList options, string user = null)
        {
            P4Command logout = user != null ? new P4Command(this, "logout", false, user) : new P4Command(this, "logout", false);
            P4CommandResult results;
            try
            {
                results = logout.Run(options);
            }
            catch
            {
                results = null;
                return false;
            }
            if (!results.Success)
            {
                ConnectionError = results.ErrorList[0];
            }
            else
            {
                ConnectionError = null;
            }
            return results.Success;
        }

        /// <summary>
        /// Finalizer
        /// </summary>
        ~P4Server() { Dispose(false); }

        bool _disposed = false;

        public void Dispose(bool disposing)
        {
            if (_disposed)
            {
                return;
            }
            if (!disposing)
            {
            }
            try
            {
                Close();

                TaggedOutputCallbackFn_Int = null;
                pTaggedOutputCallbackFn = IntPtr.Zero;
                P4Bridge.SetTaggedOutputCallbackFn(pServer, IntPtr.Zero);

                ErrorCallbackFn_Int = null;
                pErrorCallbackFn = IntPtr.Zero;
                P4Bridge.SetErrorCallbackFn(pServer, IntPtr.Zero);

                InfoResultsCallbackFn_Int = null;
                pInfoResultsCallbackFn = IntPtr.Zero;
                P4Bridge.SetInfoResultsCallbackFn(pServer, IntPtr.Zero);

                TextResultsCallbackFn_Int = null;
                pTextResultsCallbackFn = IntPtr.Zero;
                P4Bridge.SetTextResultsCallbackFn(pServer, IntPtr.Zero);

                BinaryResultsCallbackFn_Int = null;
                pBinaryResultsCallbackFn = IntPtr.Zero;
                P4Bridge.SetBinaryResultsCallbackFn(pServer, IntPtr.Zero);

                PromptCallbackFn_Int = null;
                pPromptCallbackFn = IntPtr.Zero;
                P4Bridge.SetPromptCallbackFn(pServer, IntPtr.Zero);

                _disposed = true;
            }
            catch (Exception ex)
            {
                LogFile.LogException("P4Server.Dispose Error", ex);
            }

        }
        #region IDisposable Members

        /// <summary>
        /// For IDispose
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }
        #endregion

        /// <summary>
        /// Reconnect to the server in the event the connection is lost
        /// </summary>
        public void Reconnect()
        {
            if (pServer != IntPtr.Zero)
            {
                P4Bridge.ReleaseConnection(pServer);
                pServer = IntPtr.Zero;
            }
            CurrentEncodeing = P4Encoding.ASCII;

            // EPIC: This is GC'd and not thread safe
            /*
            P4CallBacks.LogMessageDelegate logfn =
                new P4CallBacks.LogMessageDelegate(LogBridgeMessage);
            */

            // encode the username, password, and workspace name in UTF-8, we
            // won't know if the client supports Unicode until after connect 
            // returns
            using (PinnedByteArray pUser = MarshalStringToIntPtr(_user),
                                   pPass = MarshalStringToIntPtr(_pass),
                                   pClient = MarshalStringToIntPtr(_ws_client))
            {
                /*
                IntPtr pLogFn = IntPtr.Zero;
                if (logfn != null)
                    pLogFn = Marshal.GetFunctionPointerForDelegate(logfn);
                */

                pServer = P4Bridge.ConnectW(_server, pUser, pPass, pClient, IntPtr.Zero /*pLogFn*/);

            }

            if (pServer == IntPtr.Zero)
            {

                ConnectionError = ConnectionErrorInt;
                P4Exception.Throw(ConnectionError);
                return;
            }
            else
            {
                ConnectionError = null;
            }

            isUnicode = P4Bridge.IsUnicode(pServer);
            if (isUnicode)
            {
                CurrentEncodeing = P4Encoding.utf8;

                // SetCharacterSet(Charset[(int)CurrentEncodeing], Charset[(int)P4Encoding.utf16bom]);
                SetCharacterSet("utf8", "none");
            }

            requiresLogin = P4Bridge.UseLogin(pServer);

            // Link the callbacks from the bridge dll to their corresponding events
            SetInfoResultsCallback();
            SetTaggedOutputCallback();
            SetErrorCallback();
            SetTextResultsCallback();
            SetBinaryResultsCallback();
            SetPromptCallback();
            SetResolveCallback();
            SetResolveACallback();
            SetParallelTransferCallback();

            if (_cwd != null)
            {
                CurrentWorkingDirectory = _cwd;
            }
            ProgramName = _prog_name;
            ProgramVersion = _prog_ver;

            // *** Don't login using the login command. If we are reconnecting
            // *** after a timeout, doe not want to risk the login timing out and 
            // *** throwing us in an infinite loop
            // We've theoretically already logged in, so if using tickets, _pass
            // should hold the ticket.
            //Login(_pass, null);
        }

        /// <summary>
        /// Close the connection to a P4 Server
        /// </summary>
        /// <remarks> 
        /// Called by the Dispose() method
        /// </remarks>
        public void Close()
        {
            if (pServer != IntPtr.Zero)
            {
                lock (runLock)
                {
                    try
                    {
                        if (DisconnectTimer != null)
                        {
                            DisconnectTimer.Stop();
                            DisconnectTimer.Dispose();
                        }

                        if (lastCmdId != 0)
                        {
                            Debug.Trace(
                                string.Format("In Close(), currently running command {0}", lastCmdId));
                            // can't canel commands from the origonal list, as that might change the contents
                            // of the list and crash the enumerator

                            P4Bridge.CancelCommand(pServer, lastCmdId);
                            Thread.Sleep(TimeSpan.FromSeconds(1)); // wait for commands to exit
                        }
                    }
                    finally
                    {
                        if (P4Bridge.ReleaseConnection(pServer) == 0)
                        {
                            Debug.Trace("Could not close connection, commands are still running");
                        }
                        pServer = IntPtr.Zero;
                    }
                }
            }
        }

        /// <summary>
        /// Need to use Unicode when marshalling to/from the P4 server
        /// </summary>
        public bool UseUnicode
        {
            get
            {
                return isUnicode;
            }
        }

        /// <summary>
        /// What API level does the server support
        /// </summary>
        public int ApiLevel
        {
            get
            {
                return apiLevel;
            }
        }
        /// <summary>
        /// The server requires a client to use the login command to pass credentials.
        /// </summary>
        public bool RequiresLogin
        {
            get
            {
                return requiresLogin;
            }
        }
        System.Timers.Timer DisconnectTimer = null;

        private void OnDisconnectTimer(object source, ElapsedEventArgs e)
        {
            lock (runLock)
            {
                LogFile.LogMessage(4, "P4Server", "Handling disconnect timer");
                if (pServer == IntPtr.Zero)
                {
                    return;
                }

                // have we actually timed out, or did another command run after we acquired the lock?
                if ((DateTime.Now - lastRunCommand).TotalMilliseconds > IdleDisconnectWaitTime)
                {
                    LogFile.LogMessage(4, "P4Server", "Disconnect");
                    DisconnectTimer.Stop();
                    Disconnect();
                    LogFile.LogMessage(4, "P4Server", "Disconnect complete");
                }
                else
                {
                    LogFile.LogMessage(4, "P4Server", String.Format("skipping disconnect, a new command was run at {0}", lastRunCommand));
                }
            }
        }

        public void Disconnect()
        {
            if (pServer != IntPtr.Zero)
            {
                P4Bridge.Disconnect(pServer);
            }
        }

        public bool UrlHandled()
        {
            return P4Bridge.UrlLaunched();
        }

        public bool IsConnected()
        {
            if (pServer == IntPtr.Zero)
                return false;
            return P4Bridge.IsConnected(pServer) != 0;
        }

        private class RunCommandThreadParam
        {
            public string cmd;
            public bool tagged;
            public String[] args;
            public int argc;
            public bool Results;
            public uint cmdId;

            public Exception RunCmdException = null;
        }
        /// <summary>
        /// Time for a command to run before allowing the client/user to cancel;
        /// </summary>
        public TimeSpan KeepAliveDelay = TimeSpan.FromSeconds(5);

        [Obsolete("Use RunCmdTimeout")]
        public TimeSpan RunCmdTimout
        {
            get { return RunCmdTimeout; }
            set { RunCmdTimeout = value; }
        }

#if DEBUG
        // long delays for debugging so it won't time out / disconnect why stepping through code

        public TimeSpan RunCmdTimeout = TimeSpan.FromSeconds(5000);

        public double IdleDisconnectWaitTime = 5000;
#else
		public TimeSpan RunCmdTimeout = TimeSpan.FromSeconds(30);

		public double IdleDisconnectWaitTime = 5000;
#endif

        private Dictionary<uint, CmdContactTimer> RunCmdLastContactMap = null;

        public bool IsCommandPaused(uint cmdId)
        {
            return RunCmdLastContactMap[cmdId].IsPaused;

        }
        public void PauseRunCmdTimer(uint cmdId)
        {
            if (RunCmdLastContactMap == null || !RunCmdLastContactMap.ContainsKey(cmdId))
            {
                return;
            }
            RunCmdLastContactMap[cmdId].Pause();
        }

        public void ContinueRunCmdTimer(uint cmdId)
        {
            if (RunCmdLastContactMap == null || !RunCmdLastContactMap.ContainsKey(cmdId))
            {
                return;
            }
            RunCmdLastContactMap[cmdId].Continue();
        }

        public class CmdContactTimer : IDisposable
        {
            private uint cmdId = 0;
            private int CallbackCnt = 0;
            private Dictionary<uint, CmdContactTimer> RunCmdLastContactMap = null;
            DateTime LastContact = DateTime.MaxValue;

            private CmdContactTimer() { }
            public CmdContactTimer(Dictionary<uint, CmdContactTimer> runCmdLastContactMap, uint _cmdId)
            {
                if (runCmdLastContactMap == null)
                {
                    throw new ArgumentNullException("runCmdLastContactMap");
                }

                lock (runCmdLastContactMap)
                {
                    cmdId = _cmdId;
                    CallbackCnt = 0;
                    LastContact = DateTime.Now;

                    RunCmdLastContactMap = runCmdLastContactMap;

                    RunCmdLastContactMap[cmdId] = this;
                }
            }

            public void Pause()
            {
                lock (RunCmdLastContactMap)
                {
                    if (RunCmdLastContactMap == null || !RunCmdLastContactMap.ContainsKey(cmdId))
                    {
                        return;
                    }
                    LastContact = DateTime.MaxValue;
                    CallbackCnt++;
                }
            }

            public void Continue()
            {
                lock (RunCmdLastContactMap)
                {
                    if (RunCmdLastContactMap == null || !RunCmdLastContactMap.ContainsKey(cmdId))
                    {
                        return;
                    }
                    CallbackCnt--;

                    if (CallbackCnt == 0)
                    {
                        LastContact = DateTime.Now;
                    }
                }
            }

            #region IDisposable Members

            public void Dispose()
            {
                lock (RunCmdLastContactMap)
                {
                    if (RunCmdLastContactMap != null || RunCmdLastContactMap.ContainsKey(cmdId))
                    {
                        RunCmdLastContactMap.Remove(cmdId);
                    }
                }
            }

            public bool IsPaused
            {
                get
                {
                    lock (RunCmdLastContactMap)
                    {
                        return LastContact == DateTime.MaxValue;
                    }
                }
            }

            public bool OverDue(TimeSpan RunCmdTimeout)
            {
                lock (RunCmdLastContactMap)
                {
                    return ((DateTime.Now - RunCmdLastContactMap[cmdId].LastContact) > RunCmdTimeout);
                }

            }
            #endregion
        }

        //need a unique id to send to the Client for the IKeepAlive interface
        private int next_cmdId = 1;
        private object cmdIdSyncObj = new object();
        public uint getCmdId()
        {
            lock (cmdIdSyncObj)
            {
                if (next_cmdId >= ushort.MaxValue)
                {
                    next_cmdId = 1;
                }
                // even though this is unsigned, don't want a negative integer, so mask off the high bit
                uint v = (((uint)Thread.CurrentThread.ManagedThreadId) << 16) & 0x7FFF0000;
                v |= ((uint)Interlocked.Increment(ref next_cmdId)) & 0x0000FFFF;
                return v;
            }
        }

        // lastRunCommand track the last start/finish time of a command
        private DateTime lastRunCommand = new DateTime();
        // record the last cmdId for Close()'s cancel operation
        private uint lastCmdId = 0;
        // runLock syncronizes the idle disconnect timeout and the main execution thread
        private Object runLock = new Object();

        /// <summary>
        /// Run a P4 command on the P4 Server
        /// </summary>
        /// <remarks>
        /// If the command fails, the error output will contain one or more 
        /// errors generated by the P4 server.
        /// </remarks>
        /// <param name="cmd">Command code</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        /// <param name="tagged">Use tagged output for the results</param>
        /// <param name="args">Arguments for the command</param>
        /// <param name="argc">Argument count</param>
        /// <returns>Success/Failure</returns>
        public bool RunCommand(string cmd,
                                uint cmdId,
                                bool tagged,
                                String[] args,
                                int argc)
        {
            // if there is an owner thread id present, make sure it matches ours
            if (ownerThread >= 0 && Thread.CurrentThread.ManagedThreadId != ownerThread)
            {
                // TODO: throw an error?
                throw new P4.P4Exception(ErrorSeverity.E_FATAL, String.Format("Mismatched thread id: {0} should be {1}", Thread.CurrentThread.ManagedThreadId, ownerThread));
            }

            if (_disposed)
            {
                throw new P4.P4Exception(ErrorSeverity.E_FATAL, "trying to run a command on a disposed server");
            }

            bool results = false;

            RunCommandThreadParam CmdParams = new RunCommandThreadParam();
            CmdParams.Results = false;
            CmdParams.cmd = cmd;
            CmdParams.tagged = tagged;
            CmdParams.args = args;
            CmdParams.argc = argc;
            CmdParams.cmdId = cmdId;

            lock (runLock)
            {
                lastRunCommand = DateTime.Now;
                lastCmdId = cmdId;
                try
                {
                    // reset the parallel errors and servers lists
                    parallelErrors = null;
                    parallelServers = null;

                    // set the transfer function now that we have locked the runner
                    if (ParallelTransferCallbackFn != null)
                        P4Bridge.SetParallelTransferCallbackFn(pServer, ParallelTransferCallbackFn);

                    if (DisconnectTimer != null)
                        DisconnectTimer.Stop();

                    if (!isUnicode)
                    {
                        results = P4Bridge.RunCommandA(pServer,
                                                        cmd,
                                                        cmdId,
                                                        tagged,
                                                        args,
                                                        argc);
                    }
                    else
                    {
                        using (PinnedByteArrays args_b = MarshalStringArrayToIntPtrArray(args,
                                                                                argc))
                        {
                            results = P4Bridge.RunCommandW(pServer,
                                                            cmd,
                                                            cmdId,
                                                            tagged,
                                                            (IntPtr[])args_b,
                                                            argc);
                        }
                    }

                    if (!results)
                    {
                        Debug.Trace(string.Format("RunCommand Command [{0}] failed!", cmdId));
                        // error 
                        IntPtr pObj = P4Bridge.GetErrorResults(pServer, cmdId);

                        if (pObj == IntPtr.Zero)
                        {
                            // no errors from command, so check for a connection error
                            ConnectionError = ConnectionErrorInt;
                            if (ConnectionError == null)
                            {
                                Debug.Trace(string.Format("RunCommand Command [{0}] failed! ConnectionError=null", cmdId));
                                P4Exception.Throw(cmd, args, ErrorSeverity.E_FATAL, "Unknown Problem, can't continue");
                            }
                            else
                            {
                                Debug.Trace(string.Format("RunCommand Command [{1}] failed! ConnectionError={0}", ConnectionError, cmdId));
                                P4Exception.Throw(cmd, args, ConnectionError);
                            }
                        }
                        else
                        {
                            P4ClientErrorList _errorList = new P4ClientErrorList(this, pObj);
                            // when we cancel a command, throw the cancelled connection exception
                            // the client error is  MsgRpc::Break - "TCP receive interrupted by client"
                            foreach (P4ClientError e in _errorList)
                            {
                                if (e.ErrorCode == P4ClientError.MsgRpc_Break)
                                    throw new P4CommandCanceledException(String.Format("Command {0} cancelled", cmdId));
                            }
                            P4Exception.Throw(cmd, args, _errorList, GetInfoResults(cmdId));
                        }
                        return false;
                    }
                    else
                    {
                        // may be some warnings in the list, so fetch it if it is not null
                        IntPtr pObj = P4Bridge.GetErrorResults(pServer, cmdId);
                        P4ClientErrorList _errorList = null;
                        if (pObj != IntPtr.Zero)
                        {
                            _errorList = new P4ClientErrorList(this, pObj);
                        }
                        ConnectionError = null;
                    }
                }
                finally
                {
                    // update the lastRunCommand
                    lastRunCommand = DateTime.Now;
                    lastCmdId = 0;
                    if (IdleDisconnectWaitTime > 0)
                    {
                        if (DisconnectTimer == null)
                        {
                            DisconnectTimer = new System.Timers.Timer(IdleDisconnectWaitTime);
                            DisconnectTimer.AutoReset = true;
                            DisconnectTimer.Elapsed += new ElapsedEventHandler(OnDisconnectTimer);
                            DisconnectTimer.Start();
                        }
                        else if (DisconnectTimer.Enabled == false)
                        {
                            DisconnectTimer.Start();
                        }
                    }
                }
            }
            return results;
        }

        /// <summary>
        /// Mostly for testing, provide a way to tell if a parallel operation has started
        /// </summary>
        public int GetParallelOperationCount()
        {
            return (parallelServers == null) ? 0 : parallelServers.Count;
        }

        /// <summary>
        /// Cancel a running command
        /// </summary>
        /// <param name="CmdId">Unique Id for the run of the command</param>
        public void CancelCommand(uint CmdId)
        {
            
            // special case for parallel operations
            if (parallelServers != null && parallelServers.Count > 0)
            {
                foreach (P4Server s in parallelServers)
                {
                    s.CancelCommand(CmdId);
                }
            }

            P4Bridge.CancelCommand(pServer, CmdId);
        }

        /// <summary>
        /// Delegate used to send tagged output as it is generated.
        /// </summary>
        /// <remarks>
        /// This delegate will send a complete object after all of its fields 
        /// have been received by the callback from the bridge dll.
        /// </remarks>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        /// <param name="ObjId"></param>
        /// <param name="Obj"></param>
        public delegate void TaggedOutputDelegate(uint cmdId,
                                                    int ObjId,
                                                    TaggedObject Obj);

        /// <summary>
        /// Event to broadcast tagged output
        /// </summary>
        public event TaggedOutputDelegate TaggedOutputReceived;

        /// <summary>
        /// Get the tagged output generated by a command
        /// </summary>
        /// <returns>A list of TaggedObjects comprising the tagged output.
        /// </returns>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        public TaggedObjectList GetTaggedOutput(uint cmdId)
        {
            int nData = P4Bridge.GetTaggedOutputCount(pServer, cmdId);
            IntPtr pData = P4Bridge.GetTaggedOutput(pServer, cmdId);

            if (pData == IntPtr.Zero)
            {
                return null;
            }

            // use a StrDictListIterator to return all of the objects and 
            //  their keys.
            StrDictListIterator data = new StrDictListIterator(this, pData);
            TaggedObjectList objects = new TaggedObjectList(nData);

            while (data.NextItem())
            {
                TaggedObject currentObject = new TaggedObject();
                objects.Add(currentObject);

                KeyValuePair kv = null;
                while ((kv = data.NextEntry()) != null)
                {
                    if (string.IsNullOrEmpty(kv.Key) == false)
                    {
                        currentObject[kv.Key] = kv.Value;
                    }
                }
            }

            // clean up the pData object pointer in the bridge
            P4Bridge.Release(pData);

            return (TaggedObjectList)objects;
        }

        /// <summary>
        /// Delegate used to send errors as they are generated.
        /// </summary>
        /// <remarks>
        /// This delegate will send a block of data for each call received by 
        /// the callback from the bridge dll.
        /// </remarks>
        /// <param name="cmdId">Command Id of the command causing the error</param>
        /// <param name="severity">Severity of the error</param>
        /// <param name="errorNumber">Error number for the error</param>
        /// <param name="data">Error message</param>
        public delegate void ErrorDelegate(uint cmdId, int severity, int errorNumber, String data);
        /// <summary>
        /// Holds the call back passed to the bridge used to receive the 
        /// raw  data 
        /// </summary>
        P4CallBacks.ErrorDelegate
            ErrorCallbackFn_Int = null;

        /// <summary>
        /// Broadcast errors received
        /// </summary>
        public event ErrorDelegate ErrorReceived;

        /// <summary>
        /// Get a list of errors (if any) generated by a command
        /// </summary>
        /// <returns>A list of P4ClientErrors, null if no errors</returns>
        public P4ClientErrorList GetErrorResults(uint cmdId)
        {
            // special case for parallel operations, return the parallel list if non-empty
            if (parallelErrors != null && parallelErrors.Count > 0)
                return parallelErrors;

            IntPtr pErr = P4Bridge.GetErrorResults(pServer, cmdId);

            if (pErr != IntPtr.Zero)
            {
                return new P4ClientErrorList(this, pErr);
            }
            return null;
        }

        /// <summary>
        /// Delegate used to send Info Results as they are generated.
        /// </summary>
        /// <remarks>
        /// This delegate will send a block of data for each call received by 
        /// the callback from the bridge dll.
        /// </remarks>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        /// <param name="msgId"></param>
        /// <param name="level">Server supplied message level</param>
        /// <param name="data">Server supplied message data</param>
        public delegate void InfoResultsDelegate(uint cmdId, int msgId, int level, String data);

        /// <summary>
        /// Broadcast event for info results
        /// </summary>
        public event InfoResultsDelegate InfoResultsReceived;

        /// <summary>
        /// Get the information messages generated by the previous command
        /// </summary>
        /// <remarks>
        /// Each message is formatted as follows
        /// l:Message text
        ///     where l is a single digit representing the message level
        /// </remarks>
        /// <returns>List of messages</returns>
        public P4ClientInfoMessageList GetInfoResults(uint cmdId)
        {
            int nInfoOut = P4Bridge.GetInfoResultsCount(pServer, cmdId);
            IntPtr pInfoOut = P4Bridge.GetInfoResults(pServer, cmdId);

            if (pInfoOut != IntPtr.Zero)
            {
                return new P4ClientInfoMessageList(this, pInfoOut, nInfoOut);
            }
            return null;
        }

        /// <summary>
        /// Delegate used to send Text Results as they are generated.
        /// </summary>
        /// <remarks>
        /// This delegate will send a block of data for each call received by 
        /// the callback from the bridge dll.
        /// </remarks>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        /// <param name="data">Text results generated by the command</param>
        public delegate void TextResultsDelegate(uint cmdId, String data);

        /// <summary>
        /// Broadcast event for text results
        /// </summary>
        public event TextResultsDelegate TextResultsReceived;

        /// <summary>
        /// Get the complete text results for the last command
        /// </summary>
        /// <returns></returns>
        /// <param name="cmdID">Unique Id for the run of the command</param>
        public String GetTextResults(uint cmdID)
        {
            IntPtr pTextOut = P4Bridge.GetTextResults(pServer, cmdID);

            return MarshalPtrToString(pTextOut);
        }

        /// <summary>
        /// Delegate used to send binary output as it is generated.
        /// </summary>
        /// <remarks>
        /// This delegate will send a block of data for each call received by 
        /// the callback from the bridge dll.
        /// </remarks>
        /// <param name="data">Binary data generated by a command</param>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        public delegate void BinaryResultsDelegate(uint cmdId, byte[] data);

        /// <summary>
        /// Broadcast event for binary data
        /// </summary>
        public event BinaryResultsDelegate BinaryResultsReceived;

        /// <summary>
        /// Get the complete binary results for the last command
        /// </summary>
        /// <returns>The binary data</returns>
        public byte[] GetBinaryResults(uint cmdId)
        {
            int byteCount = P4Bridge.GetBinaryResultsCount(pServer, cmdId);
            if (byteCount <= 0)
                return null;

            IntPtr pData = P4Bridge.GetBinaryResults(pServer, cmdId);
            if (pData == IntPtr.Zero)
                return null;

            return MarshalPtrToByteArrary(pData, byteCount);
        }


        /// <summary>
        /// Delegate used to commands as they are executed.
        /// </summary>
        /// <param name="data">Command line executed by the command</param>
        public delegate void CommandEchoDelegate(String data);

        /// <summary>
        /// Broadcast event for text results
        /// </summary>
        public event CommandEchoDelegate CommandEcho;

        /// <summary>
        /// Broadcast a the command line (cmd and args) on the CommandEcho event
        /// </summary>
        /// <remarks>
        /// Used to echo an executed command line back to the client
        /// </remarks>
        /// <param name="cmd">The P4 command.</param>
        /// <param name="args">The flags and parameters for the command.</param>
        public void EchoCommand(string cmd, StringList args)
        {
            if (CommandEcho != null)
            {
                string commandLine = cmd;
                if (args != null)
                {
                    for (int idx = 0; idx < args.Count; idx++)
                    {
                        if (args[idx] != null)
                        {
                            commandLine += " " + args[idx];
                        }
                    }
                }
                CommandEcho(commandLine);
            }
        }

        /// <summary>
        /// Broadcast a string on the CommandEcho event
        /// </summary>
        /// <remarks>
        /// Used to echo command data back to the client
        /// </remarks>
        /// <param name="str">The string.</param>
        public void EchoCommand(string str)
        {
            if (CommandEcho != null)
            {
                CommandEcho(str);
            }
        }

        /// <summary>
        /// The data set for use by a command
        /// </summary>
        /// <remarks>
        /// If a command requires data not passed on the command line, such as 
        /// a client spec, it is passed to the P$ server by setting the data
        /// set in the P4 api.
        /// </remarks>
        public void SetDataSet(uint cmdId, string value)
        {
            if (isUnicode)
            {
                using (PinnedByteArray pData = MarshalStringToIntPtr(value))
                {
                    P4Bridge.SetDataSetW(pServer, cmdId, pData);
                }
            }
            else
            {
                P4Bridge.SetDataSetA(pServer, cmdId, value);
            }
        }

        // Epic
        public void SetConnectionHost(string hostname)
        {
            if (!IsConnected())
            {
                throw new Exception("Must be connected to set connection host");
            }

            P4Bridge.SetConnectionHost(pServer, hostname);
        }


        public String GetDataSet(uint cmdId)
        {
            IntPtr pData = P4Bridge.GetDataSet(pServer, cmdId);

            return MarshalPtrToString(pData);
        }

        /// <summary>
        /// Delegate used to provide a custom handler for input prompts from the p4api.
        /// </summary>
        /// <param name="cmdId">Unique Id for the run of the command</param>
        /// <param name="msg"></param>
        /// <param name="displayText"></param>
        /// <returns></returns>
        public delegate String PromptHandlerDelegate(uint cmdId, String msg, bool displayText);

        /// <summary>
        /// Delegate used to process prompts for input from the server.
        /// </summary>
        public PromptHandlerDelegate PromptHandler;

        /// <summary>
        /// Delegate definition for the parallel operations callback.
        /// </summary>
        /// <param name="pServer">Pointer the the bridge server (needed?)</param>
        /// <param name="cmd">Prompt message from the server</param>
        /// <param name="args">array of arguments</param>
        /// <param name="argCount">number of arguments</param>
        /// <param name="dictIter">dictionary of variables</param>
        /// <param name="threads">number of threads to launch</param>
        public delegate int ParallelTransferDelegate(IntPtr pServer, String cmd, [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 3)] String[] args, uint argCount, IntPtr dictIter, uint threads);
       
        /// <summary>
        /// Delegate used to provide a custom handler for Resolve callbacks passing a ClientMerge object from the p4api.
        /// </summary>
        /// <returns></returns>
        public delegate P4ClientMerge.MergeStatus ResolveHandlerDelegate(uint cmdId, P4ClientMerge Merger);

        /// <summary>
        /// Delegate used to provide a custom handler for Resolve callbacks passing a ClientMerge object from the p4api.
        /// </summary>
        public ResolveHandlerDelegate ResolveHandler;

        /// <summary>
        /// Delegate used to provide a custom handler for Resolve callbacks passing a ClientResolve object from the p4api.
        /// </summary>
        /// <returns></returns>
        public delegate P4ClientMerge.MergeStatus ResolveAHandlerDelegate(uint cmdId, P4ClientResolve Resolver);

        /// <summary>
        /// Delegate used to provide a custom handler for Resolve callbacks passing a ClientResolve object from the p4api.
        /// </summary>
        public ResolveAHandlerDelegate ResolveAHandler;

        /// <summary>
        /// The parameters used by the connection
        /// </summary>
        /// <remarks>
        /// The  properties, client, port, user, and password, 
        /// represent the criteria used to connect to a P4 server. If one or
        /// more is changed, the bridge will drop the current connection if any
        /// and attempt to connect to the (possibly different) P4 server when
        /// the next command is executed. If it is desirable to validate the
        /// connection, execute a command.
        /// </remarks>
        public void SetConnectionData(string port, string user, string password, string client)
        {
            _ws_client = client;
            if (isUnicode)
            {
                using (PinnedByteArray pPort = MarshalStringToIntPtr(port),
                        pUser = MarshalStringToIntPtr(user),
                        pPassword = MarshalStringToIntPtr(password),
                        pClient = MarshalStringToIntPtr(client))
                {
                    P4Bridge.set_connectionW(pServer, pPort, pUser, pPassword, pClient);
                }
            }
            else
            {
                P4Bridge.set_connectionA(pServer, port, user, password, client);
            }
        }

        /// <summary>
        /// The client workspace used by the connection
        /// </summary>
        /// <remarks>
        /// The  properties, client, port, user, and password, 
        /// represent the criteria used to connect to a P4 server. If one or
        /// more is changed, the bridge will drop the current connection if any
        /// and attempt to connect to the (possibly different) P4 server when
        /// the next command is executed. If it is desirable to validate the
        /// connection, execute a command.
        /// </remarks>
        public String Client
        {
            get
            {
                IntPtr pval = P4Bridge.get_client(pServer);

                string ret = MarshalPtrToString(pval);
                P4Bridge.ReleaseString(pval);
                return ret;
            }
            set
            {
                _ws_client = value;
                if (isUnicode)
                {
                    using (PinnedByteArray pData = MarshalStringToIntPtr(value))
                    {
                        P4Bridge.set_clientW(pServer, pData);
                    }
                }
                else
                {
                    P4Bridge.set_clientA(pServer, value);
                }
            }
        }

        /// <summary>
        /// The user name used by the connection
        /// </summary>
        /// <remarks>
        /// The  properties, client, port, user, and password, 
        /// represent the criteria used to connect to a P4 server. If one or
        /// more is changed, the bridge will drop the current connection if any
        /// and attempt to connect to the (possibly different) P4 server when
        /// the next command is executed. If it is desirable to validate the
        /// connection, execute a command.
        /// </remarks>
        public String User
        {
            get
            {
                IntPtr pval = P4Bridge.get_user(pServer);

                string ret = MarshalPtrToString(pval);
                P4Bridge.ReleaseString(pval);
                return ret;
            }
            set
            {
                _user = value;
                if (isUnicode)
                {
                    using (PinnedByteArray pData = MarshalStringToIntPtr(value))
                    {
                        P4Bridge.set_userW(pServer, pData);
                    }
                }
                else
                {
                    P4Bridge.set_userA(pServer, value);
                }
            }
        }

        /// <summary>
        /// The hostname:port used by the connection
        /// </summary>
        /// <remarks>
        /// The  properties, client, port, user, and password, 
        /// represent the criteria used to connect to a P4 server. If one or
        /// more is changed, the bridge will drop the current connection if any
        /// and attempt to connect to the (possibly different) P4 server when
        /// the next command is executed. If it is desirable to validate the
        /// connection, execute a command.
        /// </remarks>
        public String Port
        {
            get
            {
                IntPtr pval = P4Bridge.get_port(pServer);

                string ret = MarshalPtrToString(pval);
                P4Bridge.ReleaseString(pval);
                return ret;
            }
            set
            {
                _server = value;
                if (isUnicode)
                {
                    using (PinnedByteArray pData = MarshalStringToIntPtr(value))
                    {
                        P4Bridge.set_portW(pServer, pData);
                    }
                }
                else
                {
                    P4Bridge.set_portA(pServer, value);
                }
            }
        }

        /// <summary>
        /// The user's password used by the connection
        /// </summary>
        /// <remarks>
        /// The  properties, client, port, user, and password, 
        /// represent the criteria used to connect to a P4 server. If one or
        /// more is changed, the bridge will drop the current connection if any
        /// and attempt to connect to the (possibly different) P4 server when
        /// the next command is executed. If it is desirable to validate the
        /// connection, execute a command.
        /// </remarks>
        public String Password
        {
            get
            {
                IntPtr pval = P4Bridge.get_password(pServer);

                string ret = MarshalPtrToString(pval);
                P4Bridge.ReleaseString(pval);
                return ret;
            }
            set
            {
                _pass = value;
                if (isUnicode)
                {
                    using (PinnedByteArray pData = MarshalStringToIntPtr(value))
                    {
                        P4Bridge.set_passwordW(pServer, pData);
                    }
                }
                else
                {
                    P4Bridge.set_passwordA(pServer, value);
                }
            }
        }

        /// <summary>
        /// The program name used by the connection
        /// </summary>
        /// <remarks>
        /// The program name and version are recorded in the server logs when
        /// accessed by the client
        /// </remarks>
        public String ProgramName
        {
            get
            {
                IntPtr pval = P4Bridge.get_programName(pServer);

                string ret = MarshalPtrToString(pval);
                P4Bridge.ReleaseString(pval);
                return ret;
            }
            set
            {
                _prog_name = value;
                if (isUnicode)
                {
                    using (PinnedByteArray pData = MarshalStringToIntPtr(_prog_name))
                    {
                        P4Bridge.set_programNameW(pServer, pData);
                    }
                }
                else
                {
                    P4Bridge.set_programNameA(pServer, _prog_name);
                }
            }
        }

        /// <summary>
        /// The program version used by the connection
        /// </summary>
        /// <remarks>
        /// The program name and version are recorded in the server logs when
        /// accessed by the client
        /// </remarks>
        public String ProgramVersion
        {
            get
            {
                IntPtr pval = P4Bridge.get_programVer(pServer);

                string ret = MarshalPtrToString(pval);
                P4Bridge.ReleaseString(pval);
                return ret;
            }
            set
            {
                _prog_ver = value;
                if (isUnicode)
                {
                    using (PinnedByteArray pData = MarshalStringToIntPtr(_prog_ver))
                    {
                        P4Bridge.set_programVerW(pServer, pData);
                    }
                }
                else
                {
                    P4Bridge.set_programVerA(pServer, _prog_ver);
                }
            }
        }

        /// <summary>
        /// The current working directory (cwd) used by the p4 server
        /// </summary>
        /// <remarks>
        /// The  properties, client, port, user, and password, 
        /// represent the criteria used to connect to a P4 server. If one or
        /// more is changed, the bridge will drop the current connection if any
        /// and attempt to connect to the (possibly different) P4 server when
        /// the next command is executed. If it is desirable to validate the
        /// connection, execute a command.
        /// </remarks>
        public String CurrentWorkingDirectory
        {
            get
            {
                IntPtr pval = P4Bridge.get_cwd(pServer);
                String ret = MarshalPtrToString(pval);
                P4Bridge.ReleaseString(pval);
                return ret;
            }
            set
            {
                try
                {
                    _cwd = value;
                    if (isUnicode)
                    {
                        using (PinnedByteArray pData = MarshalStringToIntPtr(_cwd))
                        {
                            P4Bridge.set_cwdW(pServer, pData);
                        }
                    }
                    else
                    {
                        P4Bridge.set_cwdA(pServer, _cwd);
                    }
                }
                catch (Exception ex)
                {
                    LogFile.LogException("P4Bridge Error", ex);
                }
            }
        }


        /// <summary>
        /// The character set used by the connection
        /// </summary>
        /// <remarks>
        /// The character set used to connect to Unicode servers is set by the
        /// bridge dll automatically (possibly overridden by P4CHARSET) based 
        /// on the current Windows code page.
        /// </remarks>
        public String CharacterSet
        {
            get
            {
                IntPtr pval = P4Bridge.get_charset(pServer);

                string ret = Marshal.PtrToStringAnsi(pval);
                P4Bridge.ReleaseString(pval);
                return ret;
            }

            set
            {
                P4Bridge.set_charset(pServer, value, "none");  // Set Contents Charset,  Filenames stay at utf8
            }
        }

        /// <summary>
        /// The config file used by the connection
        /// </summary>
        public String Config
        {
            get
            {
                IntPtr pval = P4Bridge.get_config(pServer);

                string val = MarshalPtrToStringUtf8_Int(pval);
                P4Bridge.ReleaseString(pval);

                return val;
            }
        }

        static public void ConnectionInfoFromPath(String dir, out String port, out String user, out String client)
        {
            IntPtr tmpConnection = P4Bridge.ConnectionFromPath(dir);
            user = MarshalAndRelease(P4Bridge.get_user(tmpConnection));
            port = MarshalAndRelease(P4Bridge.get_port(tmpConnection));
            client = MarshalAndRelease(P4Bridge.get_client(tmpConnection));
            P4Bridge.ReleaseConnection(tmpConnection);
        }

        /// <summary>
        /// The config file that will be used by a given directory
        /// </summary>
        static public String GetConfig(string cwd)
        {
            IntPtr pval = IntPtr.Zero;
            using (PinnedByteArray pData = MarshalStringToIntPtr(Encoding.UTF8, cwd))
            {
                pval = P4Bridge.get_configW(pData);
            }
            if (pval != IntPtr.Zero)
            {
                string val = MarshalPtrToStringUtf8_Int(pval);
                P4Bridge.ReleaseString(pval);
                return val;
            }
            return null;
        }

        /// <summary>
        /// Get an environment setting used by the server, such as user, client, ..
        /// </summary>
        /// <param name="var">The name of the environment varible</param>
        /// <returns></returns>
        static public String Get(string var)
        {
            IntPtr pval = IntPtr.Zero;
            using (PinnedByteArray pData = MarshalStringToIntPtr(Encoding.Default, var))
            {
                pval = P4Bridge.GetW(pData);
            }
            if (pval != IntPtr.Zero)
            {
                string val = MarshalAndRelease(pval);
                return val;
            }
            return null;
        }

        /// <summary>
        /// Set an environment setting used by the server, such as user, client, ..
        /// </summary>
        /// <param name="var">The name of the environment variable</param>
        /// <param name="val">The new value for the environment variable</param>
        /// <returns></returns>
        public static void Set(string var, string val)
        {
            using (PinnedByteArray pData1 = MarshalStringToIntPtr(Encoding.Default, var),
                                    pData2 = MarshalStringToIntPtr(Encoding.Default, val))
            {
                P4Bridge.SetW(pData1, pData2);
            }
        }

        /// <summary>
        /// Update an environment setting used by the server, such as user, client, ..
        /// Makes a local only change which overrides registry, environment etc...
        /// And does not get written into the registry or the environment
        /// use "null" value to remove an existing Update setting.
        /// </summary>
        /// <param name="var">The name of the environment variable</param>
        /// <param name="val">The new value for the environment variable</param>
        public static void Update(string var, string val)
        {
            using (PinnedByteArray pData1 = MarshalStringToIntPtr(Encoding.UTF8, var),
                                    pData2 = MarshalStringToIntPtr(Encoding.UTF8, val))
            {
                P4Bridge.UpdateW(pData1, pData2);
            }
        }

        /// <summary>
        /// Use the C++ API to determine if a file will be ignored
        /// </summary>
        /// <param name="path">The local path of the file</param>
        /// <returns>true if the file will be ignored</returns>
        static public bool IsIgnored(string path)
        {
            using (PinnedByteArray pData = MarshalStringToIntPtr(Encoding.UTF8, path))
            {
                return P4Bridge.IsIgnoredW(pData);
            }
        }

        /// <summary>
        /// Use the C++ API to set the path of the ticket file
        /// </summary>
        /// <param name="ticketFile">full path of the ticket file</param>
        /// <returns>null</returns>
        public void SetTicketFile(string ticketFile)
        {
            using (PinnedByteArray pPass = MarshalStringToIntPtr(ticketFile))
            {
                P4Bridge.set_ticketFileW(pServer, pPass);
            }
            return;
        }

        /// <summary>
        /// Use the C++ API to determin the path of the ticket file
        /// for the current connection
        /// </summary>
        /// <returns>The path for the ticket file for the current connection</returns>
        public string GetTicketFile()
        {
            IntPtr pval = P4Bridge.get_ticketFileW(pServer);
            string ret = MarshalPtrToString(pval);
            P4Bridge.ReleaseString(pval);
            return ret;
        }

        /// <summary>
        /// Use the C++ API to get the ticket
        /// </summary>
        /// <param name="path">path to the ticket file</param>
        /// <param name="port">server:port for the connection</param>
        /// <param name="user">user name for the connection</param>
        /// <returns>The existing ticket if any</returns>
        public string GetTicket(string path, string port, string user)
        {
            using (PinnedByteArray pPath = MarshalStringToIntPtr(path),
                 pPort = MarshalStringToIntPtr(port),
             pUser = MarshalStringToIntPtr(user))
            {
                IntPtr pval = P4Bridge.get_ticket(pPath, pPort, pUser);
                string ret = MarshalPtrToString(pval);
                P4Bridge.ReleaseString(pval);
                return ret;
            }
        }

        /// <summary>
        /// Use the C++ API to determine the path of the ticket file
        /// set in the environment
        /// </summary>
        /// <returns>The path for the ticket file as set in P4TICKETS</returns>
        static public string GetEnvironmentTicketFile()
        {
            IntPtr pval = P4Bridge.GetTicketFile();
            if (pval != IntPtr.Zero)
            {
                string val = MarshalPtrToStringUtf8_Int(pval);
                P4Bridge.ReleaseString(pval);
                return val;
            }
            return null;
        }

        /// <summary>
        /// Use the C++ API to find an existing ticket if a file will be ignored
        /// </summary>
        /// <param name="port">server:port for the connection</param>
        /// <param name="user">user name for the connection</param>
        /// <returns>The existing ticket if any</returns>
        static public string GetTicket(string port, string user)
        {
            using (PinnedByteArray pData1 = MarshalStringToIntPtr(Encoding.UTF8, port),
                                    pData2 = MarshalStringToIntPtr(Encoding.UTF8, user))
            {
                IntPtr pval = P4Bridge.GetTicket(pData1, pData2);
                if (pval != IntPtr.Zero)
                {
                    string val = MarshalPtrToStringUtf8_Int(pval);
                    P4Bridge.ReleaseString(pval);
                    return val;
                }
            }
            return null;
        }

        /// <summary>
        /// Use the C++ API to find an existing ticket if a file will be ignored
        /// </summary>
        /// <param name="port">server:port for the connection</param>
        /// <param name="user">user name for the connection</param>
        /// <returns>The existing ticket if any</returns>
        static public Credential GetTicketFileCredential(string port, string user)
        {
            using (PinnedByteArray pData1 = MarshalStringToIntPtr(Encoding.UTF8, port),
                                    pData2 = MarshalStringToIntPtr(Encoding.UTF8, user))
            {
                IntPtr pval = P4Bridge.GetTicket(pData1, pData2);
                if (pval != IntPtr.Zero)
                {
                    string val = MarshalPtrToStringUtf8_Int(pval);
                    P4Bridge.ReleaseString(pval);
                    return new Credential(user, val);
                }
            }
            return null;
        }

        private int ownerThread = -1;   // managed thread ids are >= 0
        public void SetThreadOwner(int threadId)
        {
            ownerThread = threadId;
        }

        public void SetProtocol(String key, String val)
        {
            // Note that this only works if pServer is current disconnected or you reconnect after this call
            P4Bridge.SetProtocol(pServer, key, val);
        }
    }
}

