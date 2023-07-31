using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// The address of the Perforce server.
	/// </summary>
	public class ServerAddress
	{
        /// <summary>
        /// Construct ServerAddress from string
        /// </summary>
        /// <param name="uri">string uri  host:port</param>
		public ServerAddress(string uri) { Uri = uri; }

        /// <summary>
        /// Property for the string URI
        /// </summary>
		public string Uri { get; private set; }

        /// <summary>
        /// Compare ServerAddresses
        /// </summary>
        /// <param name="obj">ServerAddress to compare to</param>
        /// <returns>true if both ServerAddresses are equal</returns>
		public override bool Equals(object obj)
		{
			if ((obj is ServerAddress) == false)
			{
				return false;
			}
			ServerAddress o = obj as ServerAddress;

			if (o.Uri != null)
			{
				if (o.Uri.Equals(this.Uri) == false)
				{ return false; }
			}
			else
			{
				if (this.Uri != null)
				{ return false; }
			}
			return true;
		}

        /// <summary>
        /// Get Hashcode
        /// </summary>
        /// <returns>hash code for ServerAddress</returns>
        public override int GetHashCode() { return Uri.GetHashCode(); }
        
        /// <summary>
        /// Convert ServerAddress to String
        /// </summary>
        /// <returns>string URI for ServerAddress</returns>
        public override string ToString()
		{
			return Uri;
		}
	}

	/// <summary>
	/// The Perforce server's version information. 
	/// </summary>
	public class ServerVersion
	{
        /// <summary>
        /// Default Constructor
        /// </summary>
		public ServerVersion()
		{

		}

        /// <summary>
        /// Parameterized Constructor
        /// </summary>
        /// <param name="product">product name</param>
        /// <param name="platform">build platform</param>
        /// <param name="major">major version</param>
        /// <param name="minor">minor version</param>
        /// <param name="date">datetime of release</param>
		public ServerVersion(string product, string platform, string major,
			string minor, DateTime date)
		{
			Product = product;
			Platform = platform;
			Major = major;
			Minor = minor;
			Date = date;
		}

        /// <summary>
        /// Product name
        /// </summary>
		public string Product { get; private set; }

        /// <summary>
        /// Platform name
        /// </summary>
		public string Platform { get; private set; }

        /// <summary>
        /// Major Version
        /// </summary>
		public string Major { get; private set; }

        /// <summary>
        /// Minor Version
        /// </summary>
		public string Minor { get; private set; }

        /// <summary>
        /// Release date
        /// </summary>
		public DateTime Date { get; private set; }
	}

	/// <summary>
	/// The Perforce server's license information.
	/// </summary>
	public class ServerLicense
	{
		public ServerLicense(int users, DateTime expires)
		{
			Users = users;
			Expires = expires;
		}
		public int Users { get; private set; }
		public DateTime Expires { get; private set; }
	}
	/// <summary>
	/// Defines useful metadata about a Perforce server.
	/// </summary>
	public class ServerMetaData
	{
		public ServerMetaData()
		{

		}
		public ServerMetaData(	string name,
								ServerAddress address,
								string root,
								DateTime date,
                                string dateTimeOffset,
								int uptime,
								ServerVersion version,
								ServerLicense license,
								string licenseIp,
								bool caseSensitive,
								bool unicodeEnabled,
								bool moveEnabled
							)
		{
			Name = name;
			Address=address;
			Root=root;
			Date=date;
		    DateTimeOffset = dateTimeOffset;
			Uptime=uptime;
			Version=version;
			License=license;
			LicenseIp=licenseIp;
			CaseSensitive=caseSensitive;
			UnicodeEnabled = unicodeEnabled;
			MoveEnabled = moveEnabled;
		}
		public ServerMetaData(ServerAddress address)
		{
			Name = null;
			Address = address;
			Root = null;
			Date = DateTime.Now;
		    DateTimeOffset = null;
			Uptime = -1;
			Version = null;
			License = null;
			LicenseIp = null;
			CaseSensitive = true;
			UnicodeEnabled = false;
			MoveEnabled = true;
		}
		public string Name { get; private set; }
		public ServerAddress Address { get; internal set; }
		public string Root { get; private set; }
		public DateTime Date { get; private set; }
        public string DateTimeOffset { get; private set; }
		public int Uptime { get; private set; }
		public ServerVersion Version { get; private set; }
		public ServerLicense License { get; private set; }
		public string LicenseIp { get; private set; }
		public bool CaseSensitive { get; private set; }
		public bool UnicodeEnabled { get; private set; }
		public bool MoveEnabled { get; private set; }


		// Raw output from command in case fields are added in the future
		public TaggedObject RawData { get; private set; }

		#region fromTaggedOutput
		/// <summary>
		/// Read the fields from the tagged output of an info command
		/// </summary>
		/// <param name="objectInfo">Tagged output from the 'info' command</param>
		public void FromGetServerMetaDataCmdTaggedOutput(TaggedObject objectInfo)
		{
			RawData = objectInfo;

			if (objectInfo.ContainsKey("serverName"))
				Name = objectInfo["serverName"];

			if (objectInfo.ContainsKey("serverAddress"))
				Address = new ServerAddress(objectInfo["serverAddress"]);

			if (objectInfo.ContainsKey("serverRoot"))
				Root = objectInfo["serverRoot"];

            if (objectInfo.ContainsKey("serverDate"))
            {
                string dateTimeString = objectInfo["serverDate"];
                string[] dateTimeArray = dateTimeString.Split(' ');
                DateTime v;
                DateTime.TryParse(dateTimeArray[0] + " " + dateTimeArray[1], out v);
                Date = v;
                for (int idx = 2; idx < dateTimeArray.Count();idx++)
                {
                  DateTimeOffset += dateTimeArray[idx] + " ";
                }
                DateTimeOffset=DateTimeOffset.Trim();
            }

		    if (objectInfo.ContainsKey("serverUptime"))
			{
				int v;
				int.TryParse(objectInfo["serverUptime"], out v);
				Uptime = v;
			}


			if (objectInfo.ContainsKey("serverVersion"))
			{
				string serverVersion = objectInfo["serverVersion"];
				string[] info = serverVersion.Split('/', ' ');
				string product = info[0];
				string platform = info[1];
				string major = info[2];
				string minor = info[3];
                char[] trimChars = { '(', ')' };
                string dateString = info[4] +"-"+ info[5] +"-"+ info[6];
                dateString = dateString.Trim(trimChars);
				DateTime date = new DateTime(1,1,1);
				DateTime.TryParse(dateString, out date);
				Version = new ServerVersion(product,platform, major,minor,date);
			}




			if (objectInfo.ContainsKey("serverLicence"))
			{
				string lic = objectInfo["serverLicence"];
				string[] info = lic.Split(' ');
				int users;
				int.TryParse(info[0], out users);
				DateTime expires;
				DateTime.TryParse(info[2], out expires);
				License = new ServerLicense(users, expires);
			}

			if (objectInfo.ContainsKey("serverLicenceIp"))
				LicenseIp = objectInfo["serverLicenceIp"];

			if (objectInfo.ContainsKey("caseHandling"))
			{
				if (objectInfo["caseHandling"] == "sensitive")
					CaseSensitive = true;
			}


			if (objectInfo.ContainsKey("unicode"))
			{
				if (objectInfo["unicode"] == "enabled")
					UnicodeEnabled = true;
			}

			if (objectInfo.ContainsKey("move"))
			{
				if (objectInfo["move"] == "disabled")
					MoveEnabled = false;
			}
			else
			{
				MoveEnabled = true; ;
			}

			
		}
		#endregion

		/// <summary>
		/// Defines the UTC offset for the server.
		/// </summary>
		public class ServerTimeZone
		{
			public static int UtcOffset()
			{
				return 100;
			}
		}


	}

	/// <summary>
	/// The current state of a specific server.
	/// </summary>
	[Flags]
	public enum ServerState 
	{ 
		/// <summary>
		/// The server is offline.
		/// </summary>
		Offline = 0x000,
		/// <summary>
		/// The server is online.
		/// </summary>
		Online = 0x0001,
		/// <summary>
		/// The state of the server is unknown.
		/// </summary>
		Unknown = 0x0002
	}
	
	/// <summary>
	/// Represents a specific Perforce server. 
	/// </summary>
	public class Server
	{
		public Server(ServerAddress address)
		{
			State = ServerState.Unknown;
			Address = address;
		}
		internal void SetMetadata(ServerMetaData metadata) { Metadata = metadata; }
		internal void SetState(ServerState state) { State = state; }

		/// <summary>
		/// The host:port used to connect to a Perforce server.
		/// </summary>
		/// <remarks>
		/// Note: this can be different than the value returned by the info
		/// command if a proxy or broker is used to make the connection.
		/// </remarks>
		public ServerAddress Address { get; internal set; }

		public ServerState State { get; set; }
		public ServerMetaData Metadata { get; private set; }
	}
}
