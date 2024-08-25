// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Text.Json;
using System.Text.Json.Serialization;
using UnrealBuildBase;


#pragma warning disable SYSLIB0014

namespace AutomationTool.DeviceReservation
{
	/// <summary>
	/// Co-operatively reserves remote devices for build automation.
	/// 
	/// The constructor blocks until the specified type and number of devices are available.
	/// The reservation is automatically renewed for the lifetime of this object, and released
	/// when the object is disposed or garbage collected.
	/// </summary>
	public sealed class DeviceReservationAutoRenew : IDisposable
	{
		private Uri ReservationBaseUri;

		private static readonly TimeSpan ReserveTime = TimeSpan.FromMinutes(10);
		private static readonly TimeSpan RenewTime = TimeSpan.FromMinutes(5);

		// Max times to attempt reservation renewal, in case device starvation, reservation service being restarted, etc
		private static readonly int RenewRetryMax = 14;
		private static readonly TimeSpan RenewRetryTime = TimeSpan.FromMinutes(1);

		private Thread RenewThread;
		private AutoResetEvent WaitEvent = new AutoResetEvent(false);

		private Reservation ActiveReservation;
		private List<Device> ReservedDevices;

		// Whether the reservation requires an installation
		public bool? InstallRequired => ActiveReservation?.InstallRequired;

		public IReadOnlyList<Device> Devices
		{
			get
			{
				// Return a copy so our list can't be modified.
				return ReservedDevices.Select(d => d.Clone()).ToList();
			}
		}

		/// <summary>
		/// Creates a device reservation for the specified type and number of devices.
		/// Blocks until the devices are available.
		/// </summary>
		/// <param name="InWorkingDirectory">Working directory which contains the devices.xml and reservations.xml files. Usually a network share.</param>
		/// <param name="InDeviceTypes">An array of device types to reserve, one for each device requested. These must match the device types listed in devices.xml.</param>
		public DeviceReservationAutoRenew(string InReservationBaseUri, int RetryMax, string PoolID, params string[] InDeviceTypes)
		{
			ReservationBaseUri = new Uri(InReservationBaseUri);

			// Make a device reservation for all the required device types.
			// This blocks until the reservation is successful.
			ActiveReservation = Reservation.Create(ReservationBaseUri, InDeviceTypes, ReserveTime, RetryMax, PoolID);

			// Resolve the device IPs
			ReservedDevices = new List<Device>();
			foreach (var DeviceName in ActiveReservation.DeviceNames)
			{
				ReservedDevices.Add(Device.Get(ReservationBaseUri, DeviceName));
			}

			RenewThread = new Thread(DoAutoRenew);
			RenewThread.Start();
		}

		private void DoAutoRenew()
		{
			int RetryCurrent = 0;
			TimeSpan RenewTimeCurrent = RenewTime;

			while (!WaitEvent.WaitOne(RenewTimeCurrent))
			{
				try
				{
					// Renew the reservation on the backend
					if (RetryCurrent <= RenewRetryMax)
					{
						ActiveReservation.Renew(ReservationBaseUri, ReserveTime);
						RetryCurrent = 0;
						RenewTimeCurrent = RenewTime;
					}
				}
				catch (Exception Ex)
				{
					// There was an exception, warn if we've exceeded retry
					if (RetryCurrent == RenewRetryMax)
					{
						Utils.Log(string.Format("Warning: Reservation renewal returned bad status: {0}", Ex.Message));
					}

					// try again
					RetryCurrent++;
					RenewTimeCurrent = RenewRetryTime;
				}
			}

			// Delete reservation on server, if the web request fails backend has logic to cleanup reservations
			try
			{
				ActiveReservation.Delete(ReservationBaseUri);
			}
			catch (Exception Ex)
			{
				Utils.Log(string.Format("Warning: Reservation delete returned bad status: {0}", Ex.Message));
			}

		}

		private void StopAutoRenew()
		{
			if (RenewThread != null)
			{
				WaitEvent.Set();
				RenewThread.Join();
				RenewThread = null;
			}
		}

		~DeviceReservationAutoRenew()
		{
			StopAutoRenew();
		}

		public void Dispose()
		{
			StopAutoRenew();
			GC.SuppressFinalize(this);
		}
	}

	public static class Utils
	{

		static public string SanitizeErrorMessage(string Message)
		{
			string[] TriggersSrc = { "Warning:", "Error:", "Exception:" };
			string[] TriggersDst = { "Warn1ng:", "Err0r:", "Except10n:" };

			for (int Index = 0; Index < TriggersSrc.Length; ++Index)
			{
				if (Message.IndexOf(TriggersSrc[Index], StringComparison.OrdinalIgnoreCase) != -1)
				{
					Message = Regex.Replace(Message, TriggersSrc[Index], TriggersDst[Index], RegexOptions.IgnoreCase);
				}
			}

			return Message;
		}

		static public void Log(string Message)
		{
			Console.WriteLine("<-- Suspend Log Parsing -->");
			Console.WriteLine(SanitizeErrorMessage(Message));
			Console.WriteLine("<-- Resume Log Parsing -->");

		}

		public static Uri AppendPath(this Uri BaseUri, string NewPath)
		{
			var Builder = new UriBuilder(BaseUri);
			Builder.Path += NewPath;
			return Builder.Uri;
		}

		public static T InvokeAPI<T>(Uri UriToRequest, string Method, object ObjectToSerialize)
		{
			var SerializeOptions = new JsonSerializerOptions() { PropertyNameCaseInsensitive = true };
			SerializeOptions.Converters.Add(new TimeSpanJsonConverter());

			var Request = (HttpWebRequest)WebRequest.Create(UriToRequest);
			Request.UseDefaultCredentials = true;
			Request.Method = Method;

			if (ObjectToSerialize != null)
			{
				Request.ContentType = "application/json";

				using (var RequestStream = Request.GetRequestStream())
				{
					var JsonString = JsonSerializer.Serialize(ObjectToSerialize, SerializeOptions);
					var Writer = new StreamWriter(RequestStream);
					Writer.Write(JsonString);
					Writer.Flush();
					RequestStream.Flush();
				}
			}

			using (var Response = (HttpWebResponse)Request.GetResponse())
			using (var ResponseStream = Response.GetResponseStream())
			{
				MemoryStream MemoryStream = new MemoryStream();
				ResponseStream.CopyTo(MemoryStream);
				string JsonString = Encoding.UTF8.GetString(MemoryStream.ToArray());
				return JsonSerializer.Deserialize<T>(JsonString, SerializeOptions );
			}
		}

		public static void InvokeAPI(Uri UriToRequest, string Method, object ObjectToSerialize = null)
		{
			var Request = (HttpWebRequest)WebRequest.Create(UriToRequest);
			Request.UseDefaultCredentials = true;
			Request.Method = Method;

			if (ObjectToSerialize != null)
			{
				Request.ContentType = "application/json";

				using (var RequestStream = Request.GetRequestStream())
				{
					var JsonString = JsonSerializer.Serialize(ObjectToSerialize, new JsonSerializerOptions());
					var Writer = new StreamWriter(RequestStream);
					Writer.Write(JsonString);
					Writer.Flush();
					RequestStream.Flush();
				}
			}
			else if (Method == "PUT")
			{
				Request.ContentLength = 0;
			}

			using (var Response = (HttpWebResponse)Request.GetResponse())
			using (var ResponseStream = Response.GetResponseStream())
			{
				// Nothing to do here. Error codes throw exceptions.
			}
		}
	}

	public sealed class Reservation
	{
		public string[] DeviceNames { get; set; } = new string[] { };
		public string HostName { get; set; }
		public DateTime StartDateTime { get; set; }
		public TimeSpan Duration { get; set; }
		public Guid Guid { get; set; }
		public static string ReservationDetails = "";
		public bool? InstallRequired { get; set; } = null;

		private sealed class CreateReservationData
		{
			public string[] DeviceTypes { get; set; }
			public string Hostname { get; set; }
			public TimeSpan Duration { get; set; }
			public string ReservationDetails { get; set; }
			public string PoolId { get; set; }
			public string JobId { get; set; }
			public string StepId { get; set; }
		}

		public static Reservation Create(Uri BaseUri, string[] DeviceTypes, TimeSpan Duration, int RetryMax = 5, string PoolID = "")
		{
			bool bFirst = true;
			TimeSpan RetryTime = TimeSpan.FromMinutes(1);
			int RetryCount = 0;

			while (true)
			{
				if (!bFirst)
				{
					Thread.Sleep(RetryTime);
				}

				bFirst = false;

				Console.WriteLine("Requesting device reservation...");

				Exception UnknownException;

				try
				{
					return Utils.InvokeAPI<Reservation>(BaseUri.AppendPath("api/v1/reservations"), "POST", new CreateReservationData()
					{
						DeviceTypes = DeviceTypes,
						Hostname = Unreal.MachineName,
						Duration = Duration,
						ReservationDetails = ReservationDetails,
						PoolId = PoolID,
						JobId = Environment.GetEnvironmentVariable("UE_HORDE_JOBID"),
						StepId = Environment.GetEnvironmentVariable("UE_HORDE_STEPID")
					});
				}
				catch (WebException WebEx)
				{

					if (RetryCount == RetryMax)
					{
						Console.WriteLine("Device reservation unsuccessful");
						string FinalMessage = "Device reservation unsuccessful";
						string MaxRetryWebExMessage = GetReservationErrorMessage(WebEx);
						if (!string.IsNullOrEmpty(MaxRetryWebExMessage))
						{
							FinalMessage = string.Format("{0} - Caused by: {1}", FinalMessage, MaxRetryWebExMessage);
						}
						else
						{
							FinalMessage = string.Format("{0} - Request returned status {1}", FinalMessage, WebEx.Status.ToString());
						}
						throw new AutomationException(WebEx, FinalMessage);
					}

					string RetryMessage = String.Format("retry {0} of {1} in {2} minutes", RetryCount + 1, RetryMax, RetryTime.Minutes);
					string Message = String.Format("Unknown device server error, {0}", RetryMessage);

					string WebExMessage = GetReservationErrorMessage(WebEx, RetryMessage);

					if (!string.IsNullOrEmpty(WebExMessage))
					{
						Message = WebExMessage;
					}

					Console.WriteLine(Message);
					RetryCount++;
					UnknownException = WebEx;
				}
				catch (Exception Ex)
				{
					UnknownException = Ex;
					Utils.Log(string.Format("Device reservation unsuccessful: {0}", UnknownException.Message));
				}
			}
		}

		private static string GetReservationErrorMessage(WebException WebEx, string RetryMessage = "")
		{
			string Message = string.Empty;
			string RetryMessageEx = string.IsNullOrEmpty(RetryMessage) ? string.Empty : (", " + RetryMessage);

			if (WebEx.Response == null)
			{
				Message = String.Format("Devices service currently not available{0}", RetryMessageEx);
			}
			else if ((WebEx.Response as HttpWebResponse).StatusCode == HttpStatusCode.Conflict)
			{
				Message = String.Format("No devices currently available{0}", RetryMessageEx);
			}
			else
			{
				using (HttpWebResponse Response = (HttpWebResponse)WebEx.Response)
				{
					using (StreamReader Reader = new StreamReader(Response.GetResponseStream()))
					{
						Message = String.Format("WebException on reservation request: {0} : {1} : {2}", WebEx.Message, WebEx.Status, Reader.ReadToEnd());
					}
				}
			}
			return Message;
		}

		public void Renew(Uri BaseUri, TimeSpan NewDuration)
		{
			Utils.InvokeAPI(BaseUri.AppendPath("api/v1/reservations/" + Guid.ToString()), "PUT", NewDuration);
		}

		public void Delete(Uri BaseUri)
		{
			Utils.InvokeAPI(BaseUri.AppendPath("api/v1/reservations/" + Guid.ToString()), "DELETE");
			Console.WriteLine("Successfully deleted device reservation \"{0}\".", Guid);
		}

		static public void ReportDeviceError(string InBaseUri, string DeviceName, string Error)
		{
			if (String.IsNullOrEmpty(InBaseUri) || String.IsNullOrEmpty(DeviceName))
			{
				return;
			}

			try
			{
				Uri BaseUri = new Uri(InBaseUri);
				Utils.InvokeAPI(BaseUri.AppendPath("api/v1/deviceerror/" + DeviceName), "PUT");
				Utils.Log(string.Format("Reported device problem: {0} : {1}", DeviceName, Error));
			}
			catch (Exception Ex)
			{
				Utils.Log(string.Format("Failed to report device: {0} : {1}", DeviceName, Ex.Message));
			}
		}

	}

	public sealed class Device
	{
		public string Name { get; set; }
		public string Type { get; set; }
		public string IPOrHostName { get; set; }
		public string PerfSpec { get; set; }
		public string Model { get; set; }
		public TimeSpan AvailableStartTime { get; set; }
		public TimeSpan AvailableEndTime { get; set; }
		public bool Enabled { get; set; }
		public string DeviceData { get; set; }

		public Device Clone() { return (Device)MemberwiseClone(); }

		public static Device Get(Uri BaseUri, string DeviceName)
		{
			return Utils.InvokeAPI<Device>(BaseUri.AppendPath("api/v1/devices/" + DeviceName), "GET", null);
		}
	}

	public class TimeSpanJsonConverter : JsonConverter<TimeSpan>
	{
		public override bool CanConvert(Type ObjectType)
		{
			return ObjectType == typeof(TimeSpan);
		}

		public override TimeSpan Read(ref Utf8JsonReader Reader, Type TypeToConvert, JsonSerializerOptions Options)
		{
			return TimeSpan.Parse(Reader.GetString());
		}

		public override void Write(Utf8JsonWriter Writer, TimeSpan Value, JsonSerializerOptions Options)
		{
			var StrValue = Value.ToString();
			Writer.WriteStringValue(Options.PropertyNamingPolicy?.ConvertName(StrValue) ?? StrValue);
		}
	}
}
