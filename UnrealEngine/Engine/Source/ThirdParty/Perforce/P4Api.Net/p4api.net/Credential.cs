using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Specifies user credentials for a specific connection. 
	/// </summary>
	public class Credential
	{
		public String Ticket { get; private set; }
		/// <summary>
		/// Host Name used to store the ticket in the ticket file.
		/// </summary>
		/// <remarks>
		/// The ticket is not always stored in the ticket file. If requested, the Login
		/// command will try to determine the name used in the ticket file to store the
		/// ticket. This is null if the ticket was not stored in the ticket file.
		/// </remarks>
		public String TicketHost { get; set; }
		internal String UserName { get; private set; }
		public DateTime Expires { get; private set; }

		internal Credential(string user, string password)
		{
			UserName = user;
			Ticket = password;
			Expires = DateTime.MaxValue;
		}

		internal Credential(string user, string password, DateTime expires)
		{
			UserName = user;
			Ticket = password;
			Expires = expires;
		}

		public override string ToString()
		{
			return string.Format("User: {0}, Expires: {1} {2}", UserName, Expires.ToShortDateString(), Expires.ToShortTimeString());
		}
	}
}
