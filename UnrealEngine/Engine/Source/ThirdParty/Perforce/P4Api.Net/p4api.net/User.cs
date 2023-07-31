/*******************************************************************************

Copyright (c) 2011, Perforce Software, Inc.  All rights reserved.

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
 * Name		: User.cs
 *
 * Author	: dbb
 *
 * Description	: Class used to abstract a form specification in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Defines the user type.
	/// </summary>
	[Flags]
	public enum UserType
	{
		/// <summary>
		/// Standard: a normal user (licensed)
		/// </summary>
		Standard = 0x000,
		/// <summary>
		/// Service: a service user (not licensed)
		/// </summary>
		Service = 0x001,
		/// <summary>
		/// Operator: an operator user (not licensed)
		/// </summary>
		Operator = 0x002
	}

	/// <summary>
	/// A user on a Perforce Server, represented by a P4 User spec.
	/// </summary>
	/// <remarks>
	/// <br/><b>p4 help user</b>
	/// <br/> 
	/// <br/>     user -- Create or edit a user specification
	/// <br/> 
	/// <br/>     p4 user [-f] [name]
	/// <br/>     p4 user -d [-f] name
	/// <br/>     p4 user -o [name]
	/// <br/>     p4 user -i [-f]
	/// <br/> 
	/// <br/> 	Create a new user specification or edit an existing user specification.
	/// <br/> 	The specification form is put into a temporary file and the editor
	/// <br/> 	(configured by the environment variable $P4EDITOR) is invoked.
	/// <br/> 
	/// <br/> 	Normally, a user specification is created automatically the first
	/// <br/> 	time that the user issues any command that updates the depot. The
	/// <br/> 	'p4 user' command is typically used to edit the user's subscription
	/// <br/> 	list for change review.
	/// <br/> 
	/// <br/> 	The user specification form contains the following fields:
	/// <br/> 
	/// <br/> 	User:        The user name (read-only).
	/// <br/> 
	/// <br/> 	Email:       The user's email address (Default: user@client).
	/// <br/> 
	/// <br/> 	Update:      The date the specification was last modified (read-only).
	/// <br/> 
	/// <br/> 	Access:      The date that the user last issued a client command.
	/// <br/> 
	/// <br/> 	FullName:    The user's real name.
	/// <br/> 
	/// <br/> 	JobView:     Selects jobs that are displayed when the user creates
	/// <br/> 		     a changelist. These jobs can be closed automatically
	/// <br/> 		     when the user submits the changelist. For a description
	/// <br/> 		     of jobview syntax, see 'p4 help jobview'
	/// <br/> 
	/// <br/> 	Reviews:     The subscription list for change review.  There is no
	/// <br/> 	             limit on the number of lines that this field can contain.
	/// <br/> 		     You can include the following wildcards:
	/// <br/> 
	/// <br/> 			 ...            matches any characters including /
	/// <br/> 			 *              matches any character except /
	/// <br/> 
	/// <br/> 	Password:    The user's password.  See 'p4 help passwd'.
	/// <br/> 
	/// <br/> 	Type:        Must be 'service', operator, or 'standard'. Default is
	/// <br/> 		     'standard'. Once set, the user type cannot be changed.
	/// <br/> 
	/// <br/> 	AuthMethod:  Must be 'perforce' or 'ldap'. Default is 'perforce'
	/// <br/> 		     Unless overridden by the 'auth.default.method'
	/// <br/> 		     configurable, see 'p4 help configurables'. AuthMethod
	/// <br/> 		     can only be changed when the -f flag has been provided.
	/// <br/> 
	/// <br/> 	The -d flag deletes the specified user (unless the user has files
	/// <br/> 	open).
	/// <br/> 
	/// <br/> 	The -o flag writes the user specification to the standard output.
	/// <br/> 	The user's editor is not invoked.
	/// <br/> 
	/// <br/> 	The -i flag reads a user specification from the standard input.
	/// <br/> 	The user's editor is not invoked.
	/// <br/> 
	/// <br/> 	The -f flag forces the creation, update or deletion of the specified
	/// <br/> 	user, and enables you to change the Last Modified date. By default,
	/// <br/> 	users can only delete or modify their own user specifications.  The
	/// <br/> 	-f flag requires 'super' access, which is granted by 'p4 protect'.
	/// <br/> 
	/// <br/> 
	/// </remarks>
	public class User
	{
        /// <summary>
        /// Default Constructor
        /// </summary>
		public User()
		{
		}

		/// <summary>
		/// Parameterized constructor
		/// </summary>
		/// <param name="id">Perforce user name</param>
		/// <param name="fullname">User Full Name</param>
		/// <param name="password">password</param>
		/// <param name="emailaddress">email address</param>
		/// <param name="updated">last user update</param>
		/// <param name="accessed">last user access</param>
		/// <param name="jobview">Job View String</param>
		/// <param name="reviews">Review Specification</param>
		/// <param name="type">User type</param>
		/// <param name="spec">User form specificatoin</param>
		[ObsoleteAttribute("Use User(string id, string fullname, string password, string emailaddress, DateTime updated, DateTime accessed, string jobview, string authmethod, List<string> reviews, UserType type, FormSpec spec)", false)]
		public User(string id,
						string fullname,
						string password,
						string emailaddress,
						DateTime updated,
						DateTime accessed,
						string jobview,
						List<string> reviews,
						UserType type,
						FormSpec spec
						)
		{
			Id = id;
			FullName = fullname;
			Password = password;
			EmailAddress = emailaddress;
			Updated = updated;
			Accessed = accessed;
			JobView = jobview;
			Reviews = reviews;
			Type = type;
			Spec = spec;
		}

		/// <summary>
		/// Parameterized constructor
		/// </summary>
		/// <param name="id">Perforce user name</param>
		/// <param name="fullname">User Full Name</param>
		/// <param name="password">password</param>
		/// <param name="emailaddress">email address</param>
		/// <param name="updated">last user update</param>
		/// <param name="accessed">last user access</param>
		/// <param name="jobview">Job View String</param>
		/// <param name="authmethod">Authentication Method</param>
		/// <param name="reviews">Review Specification</param>
		/// <param name="type">User type</param>
		/// <param name="spec">User form specificatoin</param>
		public User(string id,
						string fullname,
						string password,
						string emailaddress,
						DateTime updated,
						DateTime accessed,
						string jobview,
						string authmethod,
						List<string> reviews,
						UserType type,
						FormSpec spec
						)
		{
			Id = id;
			FullName = fullname;
			Password = password;
			EmailAddress = emailaddress;
			Updated = updated;
			Accessed = accessed;
			JobView = jobview;
			AuthMethod = authmethod;
			Reviews = reviews;
			Type = type;
			Spec = spec;
		}

		private FormBase _baseForm;

		#region properties
		/// <summary>
		/// The user's user name.
		/// </summary>
		public string Id { get; set; }
		/// <summary>
		/// The user's real name.
		/// </summary>
		public string FullName { get; set; }
		/// <summary>
		/// If set, user must have matching $P4PASSWD on client.
		/// </summary>
		public string Password { get; set; }
		/// <summary>
		/// The user's email address; for email review.
		/// </summary>
		public string EmailAddress { get; set; }
		/// <summary>
		/// The date this specification was last modified.
		/// </summary>
		public DateTime Updated { get; set; }
		/// <summary>
		/// The date this user was last active.  Read only.
		/// </summary>
		public DateTime Accessed { get; set; }
		/// <summary>
		/// Selects jobs for inclusion during changelist creation.
		/// </summary>
		public string JobView { get; set; }
		/// <summary>
		/// Authentication method used for this user
		/// </summary>
		public string AuthMethod { get; set; }
		/// <summary>
		/// Listing of depot files to be reviewed by user.
		/// </summary>
		public IList<string> Reviews { get; set; }
		private StringEnum<UserType> _type;
		/// <summary>
		/// Either 'service', 'operator', or 'standard'.
		/// Default: 'standard'. Read only.
		/// </summary>
		public UserType Type 
		{ 
			get {return _type;}
			set {_type= value;}
		}
		/// <summary>
        /// The specification for the user form
        /// </summary>
		public FormSpec Spec { get; set; }

        #endregion

        /// <summary>
        /// Read the fields from the tagged output of a user command
        /// </summary>
        /// <param name="objectInfo">Tagged output from the 'user' command</param>
        /// <param name="offset">Date processing</param>
        /// <param name="dst_mismatch">DST for date</param>
        public void FromUserCmdTaggedOutput(TaggedObject objectInfo, string offset, bool dst_mismatch)
		{
			_baseForm = new FormBase();

			_baseForm.SetValues(objectInfo);

			if (objectInfo.ContainsKey("User"))
				Id = objectInfo["User"];

			if (objectInfo.ContainsKey("Email"))
				EmailAddress = objectInfo["Email"];

			if (objectInfo.ContainsKey("Update"))
			{
				DateTime d;

				if (DateTime.TryParse(objectInfo["Update"] as string, out d))
				{
					Updated = d;
				}

				else
				{
					long unixTime = 0;
					if (Int64.TryParse(objectInfo["Update"], out unixTime))
					{
                        DateTime UTC = FormBase.ConvertUnixTime(unixTime);
                        DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                            DateTimeKind.Unspecified);
                        Updated = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
					}
				}
			}

			if (objectInfo.ContainsKey("Access"))
			{
				DateTime d;

				if (DateTime.TryParse(objectInfo["Access"] as string, out d))
				{
					Updated = d;
				}

				else
				{
					long unixTime = 0;
					if (Int64.TryParse(objectInfo["Access"], out unixTime))
                    {
                        DateTime UTC = FormBase.ConvertUnixTime(unixTime);
                        DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                            DateTimeKind.Unspecified);
                        Accessed = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
                    }
				}
			}

			if (objectInfo.ContainsKey("FullName"))
				FullName = objectInfo["FullName"];

			if (objectInfo.ContainsKey("JobView"))
				JobView = objectInfo["JobView"];

            String key = "Reviews0";
            int idx = 0;
            Reviews = new StringList();

            while (objectInfo.ContainsKey((key =
                String.Format("Reviews{0}", idx))))
                { idx++; Reviews.Add(objectInfo[key]); }

            if (objectInfo.ContainsKey("Password"))
				Password = objectInfo["Password"];

			if (objectInfo.ContainsKey("Type"))
				_type = objectInfo["Type"];

			if (objectInfo.ContainsKey("AuthMethod"))
				AuthMethod = objectInfo["AuthMethod"];
		}

		/// <summary>
		/// Parse the fields from a user specification 
		/// </summary>
		/// <param name="spec">Text of the user specification in server format</param>
		/// <returns></returns>
		public bool Parse(String spec)
		{
			_baseForm = new FormBase();

			_baseForm.Parse(spec); // parse the values into the underlying dictionary

			if (_baseForm.ContainsKey("User"))
			{
				Id = _baseForm["User"] as string;
			}

			if (_baseForm.ContainsKey("Email"))
			{
				EmailAddress = _baseForm["Email"] as string;
			}

			if (_baseForm.ContainsKey("Update"))
			{
				DateTime v = DateTime.MinValue;
				DateTime.TryParse(_baseForm["Update"] as string, out v);
				Updated = v;
			}

			if (_baseForm.ContainsKey("Access"))
			{
				DateTime v = DateTime.MinValue;
				DateTime.TryParse(_baseForm["Access"] as string, out v);
				Accessed = v;
			}

			if (_baseForm.ContainsKey("FullName"))
				FullName = _baseForm["FullName"] as string;

			if (_baseForm.ContainsKey("JobView"))
				JobView = _baseForm["JobView"] as string;

            if (_baseForm.ContainsKey("Reviews"))
            {
                if (_baseForm["Reviews"] is IList<string>)
                {
                    Reviews = _baseForm["Reviews"] as IList<string>;
                }
                if (_baseForm["Reviews"] is SimpleList<string>)
                {
                    SimpleList<string> sls = (SimpleList<string>)_baseForm["Reviews"];
                    Reviews = (List<string>) sls;
                }
            }
			if (_baseForm.ContainsKey("Password"))
				Password = _baseForm["Password"] as string;

			if (_baseForm.ContainsKey("Type"))
				_type = _baseForm["Type"] as string;
			else
				Type = UserType.Standard;

			if (_baseForm.ContainsKey("AuthMethod"))
				AuthMethod = _baseForm["AuthMethod"] as string;

			return true;
		}

		/// <summary>
		/// Format of a user specification used to save a user to the server
		/// </summary>
		private static String UserSpecFormat =
													"User:\t{0}\r\n" +
													"\r\n" +
													"Email:\t{1}\r\n" +
													"\r\n" +
													"Update:\t{2}\r\n" +
													"\r\n" +
													"Access:\t{3}\r\n" +
													"\r\n" +
													"FullName:\t{4}\r\n" +
													"\r\n" +
													"JobView:\t{5}\r\n" +
													"\r\n" +
													"Reviews:\r\n{6}" +
													"\r\n" +
													"Password:\t{7}\r\n" +
													"\r\n" +
													"AuthMethod:\t{8}\r\n";

		/// <summary>
		/// Convert to specification in server format
		/// </summary>
		/// <returns></returns>
		override public String ToString()
		{
			String reviewsView = String.Empty;
			if (Reviews != null)
			{
				for (int idx = 0; idx < Reviews.Count; idx++)
				{
					reviewsView += String.Format("\t{0}\r\n", Reviews[idx]);
				}
			}
			String value = String.Format(UserSpecFormat, Id, EmailAddress,
				FormBase.FormatDateTime(Updated), FormBase.FormatDateTime(Accessed),
				FullName, JobView, reviewsView, Password, AuthMethod);
			return value;
		}
	}
	     
}
