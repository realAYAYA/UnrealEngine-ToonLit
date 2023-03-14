// Copyright Epic Games, Inc. All Rights Reserved.

namespace Gauntlet
{
	public class TestException : System.Exception
	{
		public TestException(string Msg)
				: base(Msg)
		{
		}
		public TestException(string Format, params object[] Args)
				: base(string.Format(Format, Args))
		{
		}
	}

	public class DeviceException : TestException
	{
		public DeviceException(string Msg)
				: base(Msg)
		{
		}
		public DeviceException(string Format, params object[] Args)
				: base(Format, Args)
		{
		}
	}
}
