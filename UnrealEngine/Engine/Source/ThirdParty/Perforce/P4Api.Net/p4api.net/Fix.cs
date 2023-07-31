using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Specifies a fix relationship between one or more jobs and one
	/// or more changelists. 
	/// </summary>
	public class Fix
	{
		public string JobId { get; set; }
		public int ChangeId { get; set; }
		public DateTime Date { get; set; }
		public string ClientName { get; set; }
		public string UserName { get; set; }
		public string Status { get; set; }

		StringEnum<FixAction> _action;
		public FixAction Action 
		{
			get { return _action; }
			set { _action = value; }
		}

		public Fix() { }

		public Fix(	string jobid,
					int changeid,
					DateTime date,
					string clientname,
					string username,
					string status,
					FixAction action
					)
		{
			JobId = jobid;
			ChangeId = changeid;
			Date = date;
			ClientName = clientname;
			UserName = username;
			Status = status;
			Action = action;
		}
		public void ParseFixesCmdTaggedData(TaggedObject obj, string offset, bool dst_mismatch)
		{
			if (obj.ContainsKey("Job"))
			{
				JobId = obj["Job"];
			}

			if (obj.ContainsKey("Change"))
			{
				int c = -1;
				int.TryParse(obj["Change"], out c);
				ChangeId = c;
			}

			if (obj.ContainsKey("Date"))
			{
                DateTime UTC = FormBase.ConvertUnixTime(obj["Date"]);
                DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                    DateTimeKind.Unspecified);
                Date = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
			}

			if (obj.ContainsKey("User"))
			{
				UserName = obj["User"];
			}

			if (obj.ContainsKey("Client"))
			{
				ClientName = obj["Client"];
			}

			if (obj.ContainsKey("Status"))
			{
				Status = obj["Status"];
			}

			if (obj.ContainsKey("Action"))
			{
				_action = obj["Action"];
			}
			else { Action = FixAction.Fixed; }

		}

		public static Fix FromFixesCmdTaggedOutput(TaggedObject obj, string offset, bool dst_mismatch)
		{
			Fix val = new Fix();
			val.ParseFixesCmdTaggedData(obj, offset,dst_mismatch);
			return val;
		}

		public static Fix FromFixCmdTaggedOutput(TaggedObject obj)
		{
			Fix val = new Fix();

			if (obj.ContainsKey("job"))
			{
				val.JobId = obj["job"];
			}

			if (obj.ContainsKey("change"))
			{
				int v = -1;
				int.TryParse(obj["change"], out v);
				val.ChangeId = v;
			}

			if (obj.ContainsKey("status"))
			{
				val.Status = obj["status"];
			}

			if (obj.ContainsKey("action"))
			{
				val._action = obj["action"];
			}
			return val;
		}
	}
	/// <summary>
	/// The fix action (Fixed or Unfixed).
	/// </summary>
	[Flags]
	public enum FixAction
	{
		/// <summary>
		/// Fixed
		/// </summary>
		Fixed = 0x000,
		/// <summary>
		/// Fix removed
		/// </summary>
		Unfixed = 0x001
	}
}
