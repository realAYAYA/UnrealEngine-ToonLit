/*******************************************************************************

Copyright (c) 2011-12, Perforce Software, Inc.  All rights reserved.

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
 * Name		: Connection.cs
 *
 * Author(s)	: dbb
 *
 * Description	: Class used to abstract a server connection.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

using Perforce.P4;

namespace Perforce.P4
{
    /// <summary>
    /// Flags for the server connection status.
    /// </summary>
    [Flags]
    public enum ConnectionStatus
    {
        /// <summary>
        /// Disconnected from server.
        /// </summary>
        Disconnected = 0x0000,
        /// <summary>
        /// Connected to server.
        /// </summary>
        Connected = 0x0001
    }
    /// <summary>
    /// Represents the logical connection between a specific Perforce
    /// Server instance and a specific client application. 
    /// </summary>
    public class Connection : IDisposable
    {
        private String _cachedCharacterSet = "none";
        private bool multithreaded { get; set; }

        /// <summary>
        /// Construct a Connection given a Server
        /// </summary>
        /// <param name="server">server to connect to</param>
        /// <param name="_multithreaded">Use a multithreaded connection</param>
        public Connection(Server server, bool _multithreaded = true)
        {
            Server = server;
            multithreaded = _multithreaded;
        }

        /// <summary>
        /// Property to access Connection Status
        /// </summary>
        public ConnectionStatus Status { get; set; }

        /// <summary>
        /// Property to access Server associated with connection
        /// </summary>
        public Server Server { get; private set; }

        /// <summary>
        /// Property to access Client Metadata
        /// </summary>
        public ClientMetadata ClientMetadata { get; private set; }

        /// <summary>
        /// Property to access User Name
        /// </summary>
        public string UserName
        {
            get { return username; }
            set
            {
                if (connectionEstablished())
                {
                    getP4Server().User = value != null ? value : string.Empty;
                }
                username = value;
            }
        }

        /// <summary>
        /// returns true if the connection has been made
        /// </summary>
        /// <returns>returns true if the connection has been made</returns>
        public bool connectionEstablished()
        {
            if (multithreaded)
                return _p4serverMT != null;
            else
                return _p4serverST != null;
        }

        /// <summary>
        /// Property to access Credentials
        /// </summary>
        public Credential Credential
        {
            get { return credential; }
            set
            {
                if (connectionEstablished())
                {
                    if (value!=null)
                    {
                        getP4Server().Password = value.Ticket != null ? value.Ticket : string.Empty;
                    }
                    else
                    {
                        getP4Server().Password = null;
                    }
                }
                credential = value;
            }
        }

        /// <summary>
        /// Property to access client object
        /// </summary>
        public Client Client
        {
            get { return client; }
            set
            {
                client = value;

                if (connectionEstablished())
                {
                    if ((client != null) && (client.Name != null))
                    {
                        getP4Server().Client = client.Name;
                        client.Initialize(this);
                    }
                    else
                    {
                        getP4Server().Client = string.Empty;
                    }
                }
            }
        }

        /// <summary>
        /// Set CurrentWorkingDirectory for this server
        /// </summary>
        public string CurrentWorkingDirectory
        {
            get { return _cwd; }
            set
            {
                if (connectionEstablished())
                {
                    getP4Server().CurrentWorkingDirectory = value;
                }
                _cwd = value;
            }
        }

        /// <summary>
        /// Set the client for this connection
        /// </summary>
        /// <param name="clientId">client name</param>
        public void SetClient(string clientId)
        {
            Client c = new Client();
            c.Name = clientId;

            Client = c;
        }

        /// <summary>
        /// The CharacterSet to use for this connection
        /// </summary>
        public string CharacterSetName
        {
            get
            {
                if (connectionEstablished())
                {
                    return getP4Server().CharacterSet;
                }
                return _cachedCharacterSet;
            }
            set
            {
                if (connectionEstablished())
                {
                    getP4Server().CharacterSet = value;
                } 
                _cachedCharacterSet = value;
            }
        }

        // a multithreaded server connection pool
        private P4ServerMT _p4serverMT;
        // a single threaded server
        private P4Server _p4serverST;

        /// <summary>
        /// Return the server connetion
        /// </summary>
        public P4Server getP4Server()
        {
            if (multithreaded)
                return _p4serverMT.getServer();
            else
                return _p4serverST;
        }

        /// <summary>
        /// Handler for Info Results
        /// </summary>
        public event P4Server.InfoResultsDelegate InfoResultsReceived
        {
            add { if (connectionEstablished()) { getP4Server().InfoResultsReceived += value; } }
            remove { if (connectionEstablished()) { getP4Server().InfoResultsReceived -= value; } }
        }

        /// <summary>
        /// Handler for Error Results
        /// </summary>
        public event P4Server.ErrorDelegate ErrorReceived
        {
            add { if (connectionEstablished()) { getP4Server().ErrorReceived += value; } }
            remove { if (connectionEstablished()) { getP4Server().ErrorReceived -= value; } }
        }

        /// <summary>
        /// Handler for Text Results
        /// </summary>
        public event P4Server.TextResultsDelegate TextResultsReceived
        {
            add { if (connectionEstablished()) { getP4Server().TextResultsReceived += value; } }
            remove { if (connectionEstablished()) { getP4Server().TextResultsReceived -= value; } }
        }

        /// <summary>
        /// Handler for Tagged Output
        /// </summary>
        public event P4Server.TaggedOutputDelegate TaggedOutputReceived
        {
            add { if (connectionEstablished()) { getP4Server().TaggedOutputReceived += value; } }
            remove { if (connectionEstablished()) { getP4Server().TaggedOutputReceived -= value; } }
        }

        /// <summary>
        /// Handler for Command Echo
        /// </summary>
        public event P4Server.CommandEchoDelegate CommandEcho
        {
            add { if (connectionEstablished()) { getP4Server().CommandEcho += value; } }
            remove { if (connectionEstablished()) { getP4Server().CommandEcho -= value; } }
        }

        private Client client;
        private string username;
        private string _cwd;
        private Credential credential;

        /// <summary>
        /// What API level does the server support?
        /// </summary>
        public int ApiLevel
        {
            get { return getP4Server().ApiLevel; }
        }

        /// <summary>
        /// Connect to the server with the given options
        /// </summary>
        /// <param name="options">Options for the connection</param>
        /// <returns>true if the connection succeeds</returns>
        public bool Connect(Options options)
        {
            lock (this)
            {
                if (connectionEstablished())
                {
                    getP4Server().Dispose();
                }

                string password = null;
                string ticket = null;


                if (options != null)
                {
                    if (options.Keys.Contains("Ticket"))
                    {
                        ticket = options["Ticket"];
                    }
                
                    if (options.Keys.Contains("Password"))
                    {
                        password = options["Password"];
                    }
                }

                string clientName = null;
                if ((Client != null) && (string.IsNullOrEmpty(Client.Name) == false))
                {
                    clientName = Client.Name;
                }
                try
                {
                    if ((options != null) && (options.Keys.Contains("cwd")))
                    {
                        _cwd = options["cwd"];
                        if (multithreaded)
                            _p4serverMT = new P4ServerMT(_cwd);
                        else
                        {
                            _p4serverST = new P4Server(_cwd);
                            // allow the developer to manage the threading problems on their own
                            _p4serverST.SetThreadOwner(-1);
                        }

                        if (connectionEstablished())
                        {
                            Server = new Server(new ServerAddress(getP4Server().Port));
                        }
                        else
                        {
                            throw new P4Exception(ErrorSeverity.E_FATAL, "Can't connect to server");
                        }
                    }
                    else
                    {
                        if (multithreaded)
                        {
                            _p4serverMT = new P4ServerMT(Server.Address.Uri, UserName, password,
                                clientName, _cwd, null, null);
                        }

                        else
                        {
                            
                            _p4serverST = new P4Server(Server.Address.Uri, UserName, password,
                                clientName, _cwd);
                            // allow the developer to manage the threading problems on their own
                            _p4serverST.SetThreadOwner(-1);
                        }
                    }
                    //  Set the character set if it is not the default
                    if (_cachedCharacterSet != "none")
                    {
                        getP4Server().CharacterSet = _cachedCharacterSet;
                    }                  

                    if (_commandTimeout != TimeSpan.Zero)
                    {
                        getP4Server().RunCmdTimeout = _commandTimeout;
                    }
                    try
                    {
                        // set the program name and version before the first command is run
                        if ((options != null) && (options.Keys.Contains("ProgramName")))
                        {
                            getP4Server().ProgramName = options["ProgramName"];
                        }
                        if ((options != null) && (options.Keys.Contains("ProgramVersion")))
                        {
                            getP4Server().ProgramVersion = options["ProgramVersion"];
                        }
                        // run a help command
                        getP4Server().RunCommand("help", 0, false, null, 0);
                    }
                    finally
                    {
                        // getP4Server().Disconnect();
                    }
                }
                catch (Exception e)
                {
                    LogFile.LogException("P4API.NET", e);
                    Server.SetState(ServerState.Offline);
                    throw;
                }
                if ((connectionEstablished()) && (getP4Server().pServer != IntPtr.Zero))
                {
                    if (ticket != null)
                    {
                        getP4Server().Password = ticket;
                    }
                    Status = ConnectionStatus.Connected;
                    Server.SetState(ServerState.Online);
                    if (Server.Metadata == null)
                    {
                        try
                        {
                            ServerMetaData value = new ServerMetaData();
                            if (getP4Server().ApiLevel >= 30)
                            {
                                string[] args = new string[1];
                                args[0] = "-s";
                                getP4Server().RunCommand("info", 0, true, args, 1);
                            }
                            else
                            {
                                getP4Server().RunCommand("info", 0, true, null, 0);
                            }
                            TaggedObjectList results = getP4Server().GetTaggedOutput(0);
                            if (results != null && results.Count > 0)
                            {
                                value.FromGetServerMetaDataCmdTaggedOutput(results[0]);
                            }
#if DEBUG
                            else
                            {
                                P4ClientInfoMessageList infoOut = getP4Server().GetInfoResults(0);
                                if (infoOut != null && infoOut.Count > 0)
                                {
                                    foreach (P4ClientInfoMessage info in infoOut)
                                    {
                                        System.Diagnostics.Trace.TraceInformation(info.Message);
                                    }
                                }
                                P4.P4ClientErrorList errors = getP4Server().GetErrorResults(0);
                                if (errors != null && errors.Count > 0)
                                {
                                    foreach (P4.P4ClientError error in errors)
                                    {
                                        System.Diagnostics.Trace.TraceInformation(error.ErrorMessage);
                                    }
                                }
                            }
#endif
                            Server.SetMetadata(value);
                        }
                        finally
                        {
                            // getP4Server().Disconnect();
                        }
                    }

                    if ((Server.Address == null) || (string.IsNullOrEmpty(Server.Address.Uri)))
                    {
                        string newUri = getP4Server().Port;
                        Server.Address = new ServerAddress(newUri);
                    }
                    if (string.IsNullOrEmpty(UserName))
                    {
                        UserName = getP4Server().User;
                    }
                    if (string.IsNullOrEmpty(clientName))
                    {
                        clientName = getP4Server().Client;
                        if (Client == null)
                        {
                            Client newClient = new Client();
                            newClient.Name = clientName;

                            try
                            {
                                Client = newClient;
                            }
                            catch
                            {
                                // bad default client use a null client
                                Client = null;
                            }
                        }
                        else
                        {
                            Client.Name = clientName;
                        }
                    }

                    if ((Client != null) && (string.IsNullOrEmpty(Client.Name) == false))
                    {
                        try
                        {
                            Client.Initialize(this);
                        }
                        catch (Exception ex)
                        {
                            LogFile.LogException("P4API.NET", ex);
                            if ((!connectionEstablished()) || (getP4Server().pServer == IntPtr.Zero))
                            {
                                // Connection failed and was discarderd, so rethrow the error
                                throw;
                            }
                            // can't initialize yet, probably need to login
                            // so ignore this error, we'll init the client later.
                            P4Exception p4ex = ex as P4Exception;
                            // Could be an actual error, check for a fail
                            if (p4ex.ErrorCode == P4ClientError.MsgDm_NoProtect)
                            {
                                throw new P4Exception(ErrorSeverity.E_FAILED, p4ex.Message);
                            }
                        }

                        if ((Client.Initialized) && (string.IsNullOrEmpty(getP4Server().CurrentWorkingDirectory)))
                        {
                            if ((string.IsNullOrEmpty(Client.Root) == false) && (System.IO.Directory.Exists(Client.Root)))
                            {
                                getP4Server().CurrentWorkingDirectory = Client.Root;
                            }
                            else
                            {
                                if (Client.AltRoots != null)
                                {
                                    foreach (string altRoot in Client.AltRoots)
                                    {
                                        if ((string.IsNullOrEmpty(altRoot) == false) && (System.IO.Directory.Exists(altRoot)))
                                        {
                                            getP4Server().CurrentWorkingDirectory = Client.Root;
                                            return true;
                                        }
                                    }
                                    throw new P4Exception(ErrorSeverity.E_WARN, "The client root and alternate roots do not exist on this system");
                                }
                            }
                        }
                    }
                    //if ((options != null) && (options.Keys.Contains("ProgramName")))
                    //{
                    //    getP4Server().ProgramName = options["ProgramName"];
                    //}
                    //if ((options != null) && (options.Keys.Contains("ProgramVersion")))
                    //{
                    //    getP4Server().ProgramVersion = options["ProgramVersion"];
                    //}
                    return true;
                }
                Server.SetState(ServerState.Offline);
                return false;
            }
        }

        /// <summary>
        /// Connect to an ssl server, handle trust handshake
        /// </summary>
        /// <param name="options">connect options</param>
        /// <param name="trustFlag">arguments to the "trust" command</param>
        /// <param name="fingerprint">fingerprint to install</param>
        /// <returns>true if the connection succeeds</returns>
        public bool TrustAndConnect(Options options, string trustFlag, string fingerprint)
        {
            lock (this)
            {
                if (connectionEstablished())
                {
                    Disconnect();
                }

                string password = null;
                string ticket = null;


                if ((options != null) && (options.Keys.Contains("Ticket")))
                {
                    ticket = options["Ticket"];
                }
                else
                {
                    if ((options != null) && (options.Keys.Contains("Password")))
                    {
                        password = options["Password"];
                    }
                }

                string clientName = null;
                if ((Client != null) && (string.IsNullOrEmpty(Client.Name) == false))
                {
                    clientName = Client.Name;
                }
                try
                {
                    if (multithreaded)
                        _p4serverMT = new P4ServerMT(Server.Address.Uri, UserName, password, clientName, _cwd, trustFlag, fingerprint);
                    else
                    {
                        _p4serverST = new P4Server(Server.Address.Uri, UserName, password, clientName, _cwd, trustFlag, fingerprint);
                        // allow the developer to manage the threading problems on their own
                        _p4serverST.SetThreadOwner(-1);
                    }

                    if (_commandTimeout != null)
                    {
                        getP4Server().RunCmdTimeout = _commandTimeout;
                    }
                }
                catch (Exception)
                {
                    Server.SetState(ServerState.Offline);
                    throw;
                }
                if ((connectionEstablished()) && (getP4Server().pServer != IntPtr.Zero))
                {
                    if (ticket != null)
                    {
                        getP4Server().Password = ticket;
                    }
                    Status = ConnectionStatus.Connected;
                    Server.SetState(ServerState.Online);

                    if (Server.Metadata == null)
                    {
                        try
                        {
                            ServerMetaData value = new ServerMetaData();
                            if (getP4Server().ApiLevel >= 30)
                            {
                                string[] args = new string[1];
                                args[0] = "-s";
                                getP4Server().RunCommand("info", 0, true, args, 1);
                            }
                            else
                            {
                                getP4Server().RunCommand("info", 0, true, null, 0);
                            }
                            TaggedObjectList results = getP4Server().GetTaggedOutput(0);
                            if (results != null)
                            {
                                value.FromGetServerMetaDataCmdTaggedOutput(results[0]);
                            }
#if DEBUG
                            else
                            {
                                P4ClientInfoMessageList infoOut = getP4Server().GetInfoResults(0);
                                if (infoOut != null && infoOut.Count > 0)
                                {
                                    foreach (P4ClientInfoMessage info in infoOut)
                                    {
                                        System.Diagnostics.Trace.TraceInformation(info.Message);
                                    }
                                }
                                P4.P4ClientErrorList errors = getP4Server().GetErrorResults(0);
                                if (errors != null && errors.Count > 0)
                                {
                                    foreach (P4.P4ClientError error in errors)
                                    {
                                        System.Diagnostics.Trace.TraceInformation(error.ErrorMessage);
                                    }
                                }
                            }
#endif
                            Server.SetMetadata(value);
                        }
                        finally
                        {
                            // getP4Server().Disconnect();
                        }
                    }

                    if (string.IsNullOrEmpty(Server.Address.Uri))
                    {
                        string newUri = getP4Server().Port;
                        Server.Address = new ServerAddress(newUri);
                    }
                    if (string.IsNullOrEmpty(UserName))
                    {
                        UserName = getP4Server().User;
                    }
                    if (string.IsNullOrEmpty(clientName))
                    {
                        clientName = getP4Server().Client;
                        if (Client == null)
                        {
                            Client newClient = new Client();
                            newClient.Name = clientName;

                            try
                            {
                                Client = newClient;
                            }
                            catch
                            {
                                // bad default client use a null client
                                Client = null;
                            }
                        }
                        else
                        {
                            Client.Name = clientName;
                        }
                    }

                    if ((Client != null) && (string.IsNullOrEmpty(Client.Name) == false))
                    {
                        try
                        {
                            Client.Initialize(this);
                        }
                        catch { } // can't initialize yet, probably need to login

                        if (Client.Initialized)
                        {
                            if ((string.IsNullOrEmpty(Client.Root) == false) && (System.IO.Directory.Exists(Client.Root)))
                            {
                                getP4Server().CurrentWorkingDirectory = Client.Root;
                                return true;
                            }
                            else
                            {
                                if (Client.AltRoots != null)
                                {
                                    foreach (string altRoot in Client.AltRoots)
                                    {
                                        if ((string.IsNullOrEmpty(altRoot) == false) && (System.IO.Directory.Exists(altRoot)))
                                        {
                                            getP4Server().CurrentWorkingDirectory = Client.Root;
                                            return true;
                                        }
                                    }
                                    throw new P4Exception(ErrorSeverity.E_WARN, "The client root and alternate roots do not exist on this system");
                                }
                            }
                        }
                    }
                    return true;
                }
                Server.SetState(ServerState.Offline);
                return false;
            }
        }

        /// <summary>
        /// Release the connection held by the bridge to the server. This will cause the 
        /// bridge to call init before the next command is run, forcing it to reinitialize 
        /// any cached connection settings.
        /// </summary>
        public void ReleaseConnection()
        {
            getP4Server().Disconnect();
        }

        /// <summary>
        /// Disconnect from the server
        /// </summary>
        /// <returns>true if server was disconnected</returns>
        public bool Disconnect()
        {
            return Disconnect(null);
        }

        /// <summary>
        /// Disconnect from the server
        /// </summary>
        /// <param name="options">Unused</param>
        /// <returns>true if server was disconnected</returns>
        public bool Disconnect(Options options)
        {
            lock (this)
            {
                if (!connectionEstablished())
                {
                    return false;
                }
                if (_p4serverMT != null) _p4serverMT.Dispose();
                if (_p4serverST != null) _p4serverST.Dispose();
                _p4serverMT = null;
                _p4serverST = null;
                Status = ConnectionStatus.Disconnected;
                return true;
            }
        }
        /// <summary>
        /// Run a Login on the Perforce Server
        /// </summary>
        /// <param name="password">User' password</param>
        /// <param name="options">Login options (see remarks in help file)</param>
        /// <param name="user">Login as user</param>
        /// <returns>Success/Failure</returns>
        /// <remarks>
        /// <br/><b>p4 help login</b>
        /// <br/> 
        /// <br/>     login -- Log in to Perforce by obtaining a session ticket
        /// <br/> 
        /// <br/>     p4 login [-a -p] [-h &lt;host&gt;] [user]
        /// <br/>     p4 login [-s]
        /// <br/>     p4 login [-r remote spec]
        /// <br/> 
        /// <br/> 	The login command enables a user to access Perforce until the session
        /// <br/> 	expires or the user logs out.
        /// <br/> 
        /// <br/> 	When a user logs in to Perforce, they are prompted for a password
        /// <br/> 	If they enter the correct password, they are issued a ticket.  The
        /// <br/> 	ticket expires when the default timeout value has been reached and
        /// <br/> 	is valid only for the host machine where the 'login' command was
        /// <br/> 	executed (see below for exception).
        /// <br/> 
        /// <br/> 	The ticket can be used anywhere that a password can be used.
        /// <br/> 
        /// <br/> 	Example: p4 -P &lt;ticket value&gt; changes -m1
        /// <br/> 
        /// <br/> 	The -a flag causes the server to issue a ticket that is valid on all
        /// <br/> 	host machines.
        /// <br/> 
        /// <br/> 	The -h flag causes the server to issue a ticket that is valid on the
        /// <br/> 	specified host (IP address).  This flag is typically used together
        /// <br/> 	with the -p flag to acquire a ticket that can be used on a different
        /// <br/> 	machine.
        /// <br/> 
        /// <br/> 	The -p flag displays the ticket, but does not store it on the client
        /// <br/> 	machine.
        /// <br/> 
        /// <br/> 	The -s flag displays the status of the current ticket (if there is
        /// <br/> 	one).
        /// <br/> 
        /// <br/> 	Specifying a username as an argument to 'p4 login' requires 'super'
        /// <br/> 	access, which is granted by 'p4 protect'.  In this case, 'p4 login'
        /// <br/> 	does not prompt for the password (you must already be logged in).
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public Credential Login(string password, Options options, string user)
        {

            if (getP4Server().RequiresLogin)
            {
                string tkt = string.Empty;
                string usr = UserName;
                DateTime exp = DateTime.MaxValue;

                // Login into the server. The login command will prompt
                // for the password. If user does not have a password, 
                // the command will just return with a result saying 
                // that login is not required.
                P4Command login = null;
                if (user == null)
                {
                    login = new P4Command(this, "login", true);
                }
                else
                {
                    login = new P4Command(this, "login", true, user);
                    usr = user;
                }
                login.Responses = new Dictionary<string, string>();
                login.Responses["DefaultResponse"] = password;
                P4CommandResult results;

                try
                {
                    results = login.Run(options);
                    if (results.Success == false)
                    {
                        return null;
                    }
                    if ((results.Success) && (results.InfoOutput != null) && (results.InfoOutput.Count > 0))
                    {
                        if ((results.InfoOutput[0].MessageCode == P4.P4ClientError.MsgServer_LoginNotRequired) ||
                                (results.InfoOutput[0].MessageCode == P4.P4ClientError.MsgServer_LoginUser))
                        {
                            return new Credential(usr, tkt, exp);
                        }
                        else if (options.ContainsKey("-p"))
                        {
                            tkt = results.InfoOutput[0].Message;
                        }
                        else
                        {
                            tkt = password;
                        }
                    }
                    if ((results.TaggedOutput != null) && (results.TaggedOutput.Count > 0))
                    {
                        if (results.TaggedOutput[0].ContainsKey("TicketExpiration"))
                        {
                            string expStr = string.Empty;
                            expStr = results.TaggedOutput[0]["TicketExpiration"];
                            long seconds = 0;
                            long.TryParse(expStr, out seconds);
                            exp = DateTime.Now.AddSeconds(seconds);
                        }
                        if (results.TaggedOutput[0].ContainsKey("User"))
                        {
                            usr = results.TaggedOutput[0]["User"];
                        }
                    }
                    if ((results.TaggedOutput != null) && (options.ContainsKey("-s")))
                    {
                        // if this was a dsplay status reques, we can get the active ticket
                        // by calling GetPassword() on the C++ api server
                        tkt = getP4Server().Password;
                    }
                    return new Credential(UserName, tkt, exp);
                }
                catch
                {
                    return null;
                }
            }
            getP4Server().User = UserName;
            getP4Server().Password = password;
            return new Credential(UserName, password);
        }

        /// <summary>
        /// Login to the Perforce Server
        /// </summary>
        /// <param name="password">User' password</param>
        /// <param name="options">Login options (see remarks in help file)</param>
        /// <returns>Success/Failure</returns>
        /// <remarks>
        /// <br/><b>p4 help login</b>
        /// <br/> 
        /// <br/>     login -- Log in to Perforce by obtaining a session ticket
        /// <br/> 
        /// <br/>     p4 login [-a -p] [-h &lt;host&gt;] [user]
        /// <br/>     p4 login [-s]
        /// <br/> 
        /// <br/> 	The login command enables a user to access Perforce until the session
        /// <br/> 	expires or the user logs out.
        /// <br/> 
        /// <br/> 	When a user logs in to Perforce, they are prompted for a password
        /// <br/> 	If they enter the correct password, they are issued a ticket.  The
        /// <br/> 	ticket expires when the default timeout value has been reached and
        /// <br/> 	is valid only for the host machine where the 'login' command was
        /// <br/> 	executed (see below for exception).
        /// <br/> 
        /// <br/> 	The ticket can be used anywhere that a password can be used.
        /// <br/> 
        /// <br/> 	Example: p4 -P &lt;ticket value&gt; changes -m1
        /// <br/> 
        /// <br/> 	The -a flag causes the server to issue a ticket that is valid on all
        /// <br/> 	host machines.
        /// <br/> 
        /// <br/> 	The -h flag causes the server to issue a ticket that is valid on the
        /// <br/> 	specified host (IP address).  This flag is typically used together
        /// <br/> 	with the -p flag to acquire a ticket that can be used on a different
        /// <br/> 	machine.
        /// <br/> 
        /// <br/> 	The -p flag displays the ticket, but does not store it on the client
        /// <br/> 	machine.
        /// <br/> 
        /// <br/> 	The -s flag displays the status of the current ticket (if there is
        /// <br/> 	one).
        /// <br/> 
        /// <br/> 	Specifying a username as an argument to 'p4 login' requires 'super'
        /// <br/> 	access, which is granted by 'p4 protect'.  In this case, 'p4 login'
        /// <br/> 	does not prompt for the password (you must already be logged in).
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public Credential Login(string password, Options options)
        {
            return Login(password, options, null);
        }

        /// <summary>
        /// Automate the Login to the Perforce Server
        /// </summary>
        /// <param name="password">User password</param>
        /// <returns>Success/Failure</returns>
        /// <remarks>
        /// Runs the login process. If the server is using ticket based 
        /// authentication, actually runs the logon three times. Once to 
        /// login and update the ticket file, once to get the ticket from
        /// the server and finally once to get the ticket expiration data.
        /// </remarks>

        public Credential Login(string password)
        {
            return Login(password, false);
        }

        /// <summary>
        /// Runs the login process. If the server is using ticket based 
        /// authentication, actually runs the logon three times. Once to 
        /// login and update the ticket file, once to get the ticket from
        /// the server and finally once to get the ticket expiration data.
        /// </summary>
        /// <param name="password">User password</param>
        /// <param name="allHostTicket">true if the login is intended for all hosts</param>
        /// <returns>Success/Failure</returns>
        public Credential Login(string password, bool allHostTicket)
        {
            //    return Login(password, allHostTicket, false);
            //}
            //public Credential Login(string password, bool allHostTicket, bool GetHostAlias )
            //{
            //IDictionary<string, string> OldTickets = null;
            //if (GetHostAlias)
            //{
            //    // Read the ticket file before logging in so we can see what is added or changed.
            //    OldTickets = LoadTicketFile();
            //}
            if (getP4Server().RequiresLogin)
            {
                // Login into the server. The login command will prompt
                // for the password. If user does not have a password, 
                // the command will just return with a result saying 
                // that login is not required.
                P4Command login = new P4Command(this, "login", true);

                login.Responses = new Dictionary<string, string>();
                login.Responses["DefaultResponse"] = password;
                P4CommandResult results;

                string tkt = string.Empty;
                string usr = UserName;
                DateTime exp = DateTime.MaxValue;

                try
                {
                    Options opt = null;

                    if (allHostTicket)
                    {
                        opt = new Options();
                        opt["-a"] = null;
                    }
                    // this login will write the ticket into the ticket file
                    results = login.Run(opt);

                    if ((results.InfoOutput != null) && (results.InfoOutput.Count > 0) &&
                        ((results.InfoOutput[0].MessageCode == P4ClientError.MsgServer_LoginNotRequired) ||
                         (results.InfoOutput[0].MessageCode == P4ClientError.MsgServer_LoginUser)) )
                    {
                        return new Credential(usr, tkt, exp);
                    }
                    //else if (GetHostAlias)
                    //{
                    //    // Read the ticket file after logging in so we can see what is added or changed.
                    //    IDictionary<string, string> newTickets = LoadTicketFile();
                    //    if (newTickets != null)
                    //    {
                    //        foreach (string hostId in newTickets.Keys)
                    //        {
                    //            if ((OldTickets == null) || (OldTickets.ContainsKey(hostId) == false) || (newTickets[hostId] != OldTickets[hostId]))
                    //            {
                    //                // either no tickets before, so whatever was added was the new ticket, 
                    //                // or new hostId, so must be from this login, or the value
                    //                // has changed so must be updated ticket from this login
                    //                Credential cred = new Credential(usr, newTickets[hostId], exp);
                    //                string[] parts = hostId.Split(new char[] { '=' }, 2, StringSplitOptions.RemoveEmptyEntries);
                    //                if ((parts != null) && (parts.Length > 0))
                    //                {
                    //                    cred.TicketHost = parts[0];
                    //                }
                    //                return cred;
                    //            }
                    //        }
                    //    }
                    //}
                }
                catch (Exception)
                {
                    throw;
                }
                if (results.Success)
                {
                    try
                    {
                        Options opt = new Options();

                        // this login return a ticket that does not go in the ticket file
                        opt["-p"] = null;
                        if (allHostTicket)
                        {
                            opt["-a"] = null;
                        }
                        results = login.Run(opt);

                        if ((results.InfoOutput != null) && (results.InfoOutput.Count > 0) &&
                            (results.InfoOutput[0].MessageCode == P4.P4ClientError.MsgServer_LoginNotRequired))
                        {
                            return new Credential(usr, tkt, exp);
                        }
                        else if (results.InfoOutput != null)
                        {
                            tkt = results.InfoOutput[results.InfoOutput.Count - 1].Message;

                            getP4Server().Password = tkt;
                        }

                        login = new P4Command(getP4Server(), "login", true);

                        opt = new Options();
                        opt["-s"] = null;
                        results = login.Run(opt);

                        if ((results.TaggedOutput != null) && (results.TaggedOutput.Count > 0))
                        {
                            if (results.TaggedOutput[0].ContainsKey("TicketExpiration"))
                            {
                                string expStr = string.Empty;
                                expStr = results.TaggedOutput[0]["TicketExpiration"];
                                long seconds = 0;
                                long.TryParse(expStr, out seconds);
                                exp = DateTime.Now.AddSeconds(seconds);
                            }
                            if (results.TaggedOutput[0].ContainsKey("User"))
                            {
                                usr = results.TaggedOutput[0]["User"];
                            }
                        }
                        else if (results.InfoOutput != null)
                        {
                            string line = results.InfoOutput[0].Message;

                            int idx = line.IndexOf("ticket");
                            if (idx < 0)
                                return null;

                            // "user " is 5 characters (with space)
                            usr = line.Substring(5, idx - 5).Trim();

                            string hStr;
                            idx = line.IndexOf("expires in ");
                            if (idx < 0)
                                return null;
                            idx += 11;
                            int idx2 = line.IndexOf(" hours");
                            hStr = line.Substring(idx, idx2 - idx).Trim();

                            int hours;
                            int.TryParse(hStr, out hours);

                            string mStr;
                            idx = idx2 + 6; // "hours " is 6 chars
                            if (idx < 0)
                                return null;
                            idx2 = line.IndexOf(" minutes");
                            mStr = line.Substring(idx, idx2 - idx).Trim();

                            int minutes;
                            int.TryParse(mStr, out minutes);

                            exp = DateTime.Now.AddHours(hours).AddMinutes(minutes);
                        }
                        if ((Client != null) && (Client.Initialized == false) &&
                            (string.IsNullOrEmpty(Client.Name) == false))
                        {
                            try
                            {
                                Client.Initialize(this);
                            }
                            catch { } // can't initialize yet, probably need to login

                            if (Client.Initialized)
                            {
                                if ((string.IsNullOrEmpty(Client.Root) == false) && (System.IO.Directory.Exists(Client.Root)))
                                {
                                    getP4Server().CurrentWorkingDirectory = Client.Root;
                                }
                                else
                                {
                                    if (Client.AltRoots != null)
                                    {
                                        foreach (string altRoot in Client.AltRoots)
                                        {
                                            if ((string.IsNullOrEmpty(altRoot) == false) && (System.IO.Directory.Exists(altRoot)))
                                            {
                                                getP4Server().CurrentWorkingDirectory = Client.Root;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                    }
                    catch
                    {
                        return null;
                    }

                    return new Credential(usr, tkt, exp);
                }
                return null;
            }

            getP4Server().User = UserName;
            getP4Server().Password = password;
            return new Credential(UserName, password);
        }

        /// <summary>
        /// Logout of the Perforce server
        /// </summary>
        /// <param name="options">Logout options (see remarks in help file)</param>
        /// <param name="user">user name to log out, (requires Super access)</param>
        /// <returns>Success/Failure</returns>
        /// <remarks>
        /// <br/><b>p4 help logout</b>
        /// <br/> 
        /// <br/>     logout -- Log out from Perforce by removing or invalidating a ticket.
        /// <br/> 
        /// <br/>     p4 logout [-a] [user]
        /// <br/> 
        /// <br/> 	The logout command removes the ticket on the client. To resume using
        /// <br/> 	Perforce, the user must log in again.
        /// <br/> 
        /// <br/> 	If you are logged in to Perforce from more than one machine, you can
        /// <br/> 	log out of Perforce from all machines from which you were logged in
        /// <br/> 	by specifying the -a flag. The -a flag invalidates the ticket on the
        /// <br/> 	server. All of your Perforce tickets are invalidated and you are
        /// <br/> 	logged out.
        /// <br/> 	
        /// <br/> 	Specifying a username as an argument to 'p4 logout' requires 'super'
        /// <br/> 	access, which is granted by 'p4 protect'.  Note that the '-a' flag
        /// <br/> 	must be used to effectively log out a user who has a valid ticket on
        /// <br/> 	another machine.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public bool Logout(Options options, string user = null)
        {
            bool results = false;
            if (getP4Server().RequiresLogin)
            {
                results = getP4Server().Logout(options, user);
            }
            else
            {
                getP4Server().User = string.Empty;
                getP4Server().Password = string.Empty;
            }
            return results;
        }

        /// <summary>
        /// Run the client side command trust
        /// </summary>
        /// <param name="options">trust options (see remarks in help file)</param>
        /// <param name="fingerprint">fingerprint to install</param>
        /// <returns>Success/Failure</returns>
        /// <remarks>
        /// <br/><b>p4 trust -h</b>
        /// <br/> 
        /// <br/>         trust -- Establish trust of an SSL connection
        /// <br/> 
        /// <br/>         p4 trust [ -l -y -n -d -f -r -i &lt;fingerprint&gt; ]
        /// <br/> 
        /// <br/>         Establish trust of an SSL connection.  This client command manages
        /// <br/>         the p4 trust file.  This file contains fingerprints of the keys
        /// <br/>         received on ssl connections.  When an SSL connection is made, this
        /// <br/>         file is examined to determine if the SSL connection has been used
        /// <br/>         before and if the key is the same as a previously seen key for that
        /// <br/>         connection.  Establishing trust with a connection prevents undetected
        /// <br/>         communication interception (man-in-the-middle) attacks.
        /// <br/> 
        /// <br/>         Most options are mutually exclusive.  Only the -r and -f options
        /// <br/>         can be combined with the others.
        /// <br/> 
        /// <br/>         The -l flag lists existing known fingerprints.
        /// <br/> 
        /// <br/>         Without options, this command will make a connection to a server
        /// <br/>         and examine the key if present, if one cannot be found this command
        /// <br/>         will show a fingerprint and ask if this connection should be trusted.
        /// <br/>         If a fingerprint exists and does not match, an error that a possible
        /// <br/>         security problems exists will be displayed.
        /// <br/> 
        /// <br/>         The -y flag will cause prompts to be automatically accepted.
        /// <br/> 
        /// <br/>         The -n flag will cause prompts to be automatically refused.
        /// <br/> 
        /// <br/>         The -d flag will remove an existing trusted fingerprint of a connection.
        /// <br/> 
        /// <br/>         The -f flag will force the replacement of a mismatched fingerprint.
        /// <br/> 
        /// <br/>         The -i flag will allow a specific fingerprint to be installed.
        /// <br/> 
        /// <br/>         The -r flag specifies that a replacement fingerprint is to be
        /// <br/>         affected.  Replacement fingerprints can be used in anticipation
        /// <br/>         of a server replacing its key.  If a replacement fingerprint
        /// <br/>         exists for a connection and the primary fingerprint does not match
        /// <br/>         while the replacement fnigerprint does, the replacement fingerprint
        /// <br/>         will replace the primary.  This flag can be combined with -l, -i,
        /// <br/>         or -d. 
        /// </remarks>
        public bool Trust(Options options, string fingerprint)
        {
            P4.P4Command trustCmd = null;
            if (string.IsNullOrEmpty(fingerprint))
            {
                trustCmd = new P4Command(this, "trust", false);
            }
            else
            {
                trustCmd = new P4Command(this, "trust", false, fingerprint);
            }
            P4.P4CommandResult r = trustCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return false;
            }
            return true;
        }

        /// <summary>
        /// Set the current user's password on the Perforce server.
        /// </summary>
        /// <param name="OldPassword">User's old password</param>
        /// <param name="NewPassword">User's new password</param>
        /// <returns>Success/Failure</returns>
        /// <remarks>
        /// <br/><b>p4 help passwd</b>
        /// <br/> 
        /// <br/>     passwd -- Set the user's password on the server (and Windows client)
        /// <br/> 
        /// <br/>     p4 passwd [-O oldPassword -P newPassword] [user]
        /// <br/> 
        /// <br/> 	'p4 passwd' sets the user's password on the server.
        /// <br/> 
        /// <br/> 	After a password is set for a user, the same password must be set on
        /// <br/> 	the client in the environment variable $P4PASSWD to enable the user
        /// <br/> 	to use all Perforce client applications on that machine. (On Windows,
        /// <br/> 	you can use 'p4 passwd' to configure the password in the environment.)
        /// <br/> 
        /// <br/> 	'p4 passwd' prompts for both the old password and the new password
        /// <br/> 	with character echoing turned off.  To delete the password, set it to
        /// <br/> 	an empty string.
        /// <br/> 
        /// <br/> 	The -O flag provides the old password, avoiding prompting. If you
        /// <br/> 	specify -O, you must also specify -P.
        /// <br/> 
        /// <br/> 	The -P flag provides the new password, avoiding prompting.
        /// <br/> 
        /// <br/> 	If you are using ticket-based authentication, changing your password
        /// <br/> 	automatically invalidates all of your tickets and logs you out.
        /// <br/> 
        /// <br/> 	Specifying a username as an argument to 'p4 passwd' requires 'super'
        /// <br/> 	access granted by 'p4 protect'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public bool SetPassword(string OldPassword, string NewPassword)
        {
            return SetPassword(OldPassword, NewPassword, null);
        }

        /// <summary>
        /// Set the a user's password on the Perforce server.
        /// </summary>
        /// <param name="OldPassword">User's old password</param>
        /// <param name="NewPassword">User's new password</param>
        /// <param name="User">User receiving new password</param>
        /// <returns>Success/Failure</returns>
        /// <remarks>
        /// <br/><b>p4 help passwd</b>
        /// <br/> 
        /// <br/>     passwd -- Set the user's password on the server (and Windows client)
        /// <br/> 
        /// <br/>     p4 passwd [-O oldPassword -P newPassword] [user]
        /// <br/> 
        /// <br/> 	'p4 passwd' sets the user's password on the server.
        /// <br/> 
        /// <br/> 	After a password is set for a user, the same password must be set on
        /// <br/> 	the client in the environment variable $P4PASSWD to enable the user
        /// <br/> 	to use all Perforce client applications on that machine. (On Windows,
        /// <br/> 	you can use 'p4 passwd' to configure the password in the environment.)
        /// <br/> 
        /// <br/> 	'p4 passwd' prompts for both the old password and the new password
        /// <br/> 	with character echoing turned off.  To delete the password, set it to
        /// <br/> 	an empty string.
        /// <br/> 
        /// <br/> 	The -O flag provides the old password, avoiding prompting. If you
        /// <br/> 	specify -O, you must also specify -P.
        /// <br/> 
        /// <br/> 	The -P flag provides the new password, avoiding prompting.
        /// <br/> 
        /// <br/> 	If you are using ticket-based authentication, changing your password
        /// <br/> 	automatically invalidates all of your tickets and logs you out.
        /// <br/> 
        /// <br/> 	Specifying a username as an argument to 'p4 passwd' requires 'super'
        /// <br/> 	access granted by 'p4 protect'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public bool SetPassword(string OldPassword, string NewPassword, string User)
        {
            P4Command passwd = null;
            if (User == null)
            {
                passwd = new P4Command(this, "passwd", true);
            }
            else
            {
                passwd = new P4Command(this, "passwd", true, User);
            }
            passwd.Responses = new Dictionary<string, string>();
            passwd.Responses["Enter old password: "] = OldPassword;
            passwd.Responses["Enter new password: "] = NewPassword;
            passwd.Responses["Re-enter new password: "] = NewPassword;

            P4CommandResult results = passwd.Run();

            // login using the new password to refresh the credentials used by the connection
            Login(NewPassword);

            return results.Success;
        }

        /// <summary>
        /// Returns the ticket used by the current connection
        /// </summary>
        /// <returns></returns>
        public string GetActiveTicket()
        {
            return getP4Server().Password;
        }

        /// <summary>
        /// The results of the last command executed
        /// </summary>
        public P4CommandResult LastResults
        {
            get
            {
                if (connectionEstablished())
                {
                    return getP4Server().LastResults;
                }
                return null;
            }
        }

        /// <summary>
        /// Create a P4Command that can be run on the connection
        /// </summary>
        /// <param name="cmd">Command name, i.e. 'sync'</param>
        /// <param name="tagged">Flag to create tggged output</param>
        /// <param name="args">The arguments for the command</param>
        /// <returns></returns>
        public P4Command CreateCommand(string cmd, bool tagged, params string[] args)
        {
            return new P4Command(this, cmd, tagged, args);
        }

        /// <summary>
        /// Cancel a running command
        /// </summary>
        /// <param name="cmd">Command to cancel</param>
        public void CancelCommand(P4Command cmd)
        {
            if (connectionEstablished())
            {
                getP4Server().CancelCommand(cmd.CommandId);
            }
        }

        /// <summary>
        /// Cancel a running command
        /// </summary>
        /// <param name="cmd">Unique Id for the run of the command (P4Command.CommandId)</param>
        public void CancelCommand(uint cmd)
        {
            if (connectionEstablished())
            {
                getP4Server().CancelCommand(cmd);
            }
        }

        /// <summary>
        /// Create a P4.P4MapApi object to be used on the current server connection
        /// </summary>
        /// <returns></returns>
        public P4.P4MapApi GetMapApi()
        {
            if (connectionEstablished())
            {
                return new P4MapApi(getP4Server());
            }
            return null;
        }

        /// <summary>
        /// Callback to allow the client/user to cancel a long running command
        /// </summary>
        public IKeepAlive KeepAlive
        {
            get { return getP4Server().KeepAlive; }
            set { getP4Server().KeepAlive = value; }
        }

        /// <summary>
        /// Time for a command to run before allowing the client/user to cancel a long running command
        /// </summary>
        public TimeSpan KeepAliveDelay
        {
            get { return (connectionEstablished()) ? getP4Server().KeepAliveDelay : TimeSpan.Zero; }
            set { if (connectionEstablished()) { getP4Server().KeepAliveDelay = value; } }
        }


        private TimeSpan _commandTimeout = TimeSpan.Zero;

        public TimeSpan CommandTimeout
        {
            get { return (connectionEstablished()) ? getP4Server().RunCmdTimeout : TimeSpan.Zero; }
            set
            {
                _commandTimeout = value;
                if (connectionEstablished()) { getP4Server().RunCmdTimeout = value; }
            }
        }

        // Get the value of an P4 environment variable
        // equivalent of command line "p4 set <var>"
        /// <summary>
        /// Get the value of an P4 environment variable
        /// </summary>
        /// <param name="var"></param>
        /// <returns></returns>
        /// <remarks>equivalent of command line "p4 set var"</remarks>
        public string GetP4EnvironmentVar(string var)
        {
            if (!connectionEstablished())
            {
                return null;
            }
            return P4Server.Get(var);
        }

        // Set the value of an P4 environment variable
        // equivalent ofcommand line "p4 set <var>=<val>"
        /// <summary>
        /// Set the value of an P4 environment variable
        /// </summary>
        /// <param name="var"></param>
        /// <param name="val"></param>
        /// <remarks>equivalent ofcommand line "p4 set var=val"</remarks>
        public void SetP4EnvironmentVar(string var, string val)
        {
            if (!connectionEstablished())
            {
                return;
            }
            P4Server.Set(var, val);
        }

        /// <summary>
        /// The config file used by the connection (if any)
        /// </summary>
        public string GetP4ConfigFile()
        {
            if (!connectionEstablished())
            {
                return null;
            }
            return getP4Server().Config;
        }


        /// <summary>
        /// The config file that will be used by a given directory (if any)
        /// </summary>
        public string GetP4ConfigFile(string cwd)
        {
            if (!connectionEstablished())
            {
                return null;
            }
            return P4Server.GetConfig(cwd);
        }

        /// <summary>
        /// Is this file ignored based on the file  specified by p4ignore (if any)
        /// </summary>
        /// <param name="path"></param>
        /// <returns></returns>
        public bool IsFileIgnored(string path)
        {
            if (!connectionEstablished())
            {
                return false;
            }
            return P4Server.IsIgnored(path);
        }

        //public IDictionary<string, string> LoadTicketFile()
        //{
        //    Dictionary<string, string> TicketCache = new Dictionary<string, string>();

        //    string path = P4Server.GetTicketFile();

        //    if (System.IO.File.Exists(path) == false)
        //    {
        //        // no ticket File
        //        return null;
        //    }
        //    using (StreamReader sr = new StreamReader(path))
        //    {
        //        if (sr == null)
        //        {
        //            return null;
        //        }
        //        string line;
        //        while ((line = sr.ReadLine()) != null)
        //        {
        //            int idx = line.LastIndexOf(':');
        //            string key = line.Substring(0, idx);
        //            string token = line.Substring(idx + 1);

        //            TicketCache[key] = token;
        //        }
        //    }
        //    return TicketCache;
        //}

        /// <summary>
        /// Get the existing ticket if any for a user on the secified server
        /// </summary>
        /// <param name="user">user name</param>
        /// <returns></returns>
        public string GetExistingTicket(string user)
        {
            if (string.IsNullOrEmpty(user))
            {
                throw new ArgumentException("Cannot be null or empty", "user");
            }

            //IDictionary<string, string> TicketCache = LoadTicketFile();

            //if (this.Server.Metadata == null)
            //{
            //    P4Command cmd = new P4Command(this, "info", true);

            //    P4CommandResult results = cmd.Run(null);
            //    if (results.Success)
            //    {
            //        if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
            //        {
            //            return null;
            //        }
            //        ServerMetaData value = new ServerMetaData();
            //        foreach (TaggedObject obj in results.TaggedOutput)
            //        {
            //            value.FromGetServerMetaDataCmdTaggedOutput(obj);
            //        }
            //        this.Server.SetMetadata(value);
            //    }
            //    else
            //    {
            //        P4Exception.Throw(results.ErrorList);
            //    }
            //}
            //           this.Server.SetMetadata()
            string ServerAdd = this.Server.Metadata.Address.ToString();

            //string tokenKey = string.Format("{0}={1}", ServerDNS, user);

            //if (TicketCache.ContainsKey(tokenKey))
            //{
            //    return TicketCache[tokenKey];
            //}
            //return null;

            return P4Server.GetTicket(ServerAdd, user);
        }

        #region IDisposable Members

        /// <summary>
        /// Dispose of connection.  Forces disconnect
        /// </summary>
        public void Dispose()
        {
            Disconnect(null);
        }

        #endregion
    }
}
