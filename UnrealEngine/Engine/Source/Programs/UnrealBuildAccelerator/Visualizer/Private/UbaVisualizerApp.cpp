// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaVisualizer.h"
#include "UbaNetworkBackendTcp.h"
#include "UbaVersion.h"

using namespace uba;

int PrintHelp(const tchar* message)
{
	StringBuffer<64*1024> s;
	if (*message)
		s.Appendf(TC("%s\r\n\r\n"), message);

	s.Appendf(TC("\r\n"));
	s.Appendf(TC("------------------------\r\n"));
	s.Appendf(TC("   UbaVisualizer v%s\r\n"), GetVersionString());
	s.Appendf(TC("------------------------\r\n"));
	s.Appendf(TC("\r\n"));
	s.Appendf(TC("  When started UbaVisualizer will keep trying to connect to provided host address or named memory buffer.\r\n"));
	s.Appendf(TC("  Once connected it will start visualizing. Nothing else is needed :)\r\n"));
	s.Appendf(TC("\r\n"));
	s.Appendf(TC("  -host=<host>         The ip/name of the machine we want to connect to\r\n"));
	s.Appendf(TC("  -port=<port>         The port to connect to. Defaults to \"%u\"\r\n"), DefaultPort);
	s.Appendf(TC("  -named=<name>        Name of named memory to connect to\r\n"));
	s.Appendf(TC("  -file=<name>         Name of file to parse\r\n"));
	s.Appendf(TC("  -listen[=<channel>]  Listen for announcements of new sessions. Defaults to channel '%s'\r\n"), TC("Default"));
	s.Appendf(TC("  -replay              Visualize the data as if it was running right now\r\n"));
	s.Appendf(TC("  -theme=<dark/light>  Force dark/light theme\r\n"));
	s.Appendf(TC("\r\n"));
	MessageBox(NULL, s.data, TC("UbaVisualizer"), 0);
	//wprintf(s.data);
	return -1;
}

struct MessageBoxLogWriter : public LogWriter
{
	virtual void BeginScope() override
	{
	}

	virtual void EndScope() override
	{
	}

	virtual void Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix = nullptr, u32 prefixLen = 0) override
	{
		if (type > LogEntryType_Warning)
			return;

		HWND hwnd = m_visualizer ? m_visualizer->GetHwnd() : NULL;
		UINT flags = MB_ICONERROR;
		if (!hwnd)
			flags |= MB_TOPMOST;
		MessageBox(hwnd, str, TC("UbaVisualizer"), flags);
		if (type == LogEntryType_Error)
			ExitProcess(~0u);
	}
	
	Visualizer* m_visualizer = nullptr;
};

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nShowCmd)
{
	StringBuffer<> host; // 192.168.86.49
	StringBuffer<> named;
	StringBuffer<> file;
	StringBuffer<> channel;
	u32 port = DefaultPort;
	u32 replay = 0;
	bool useDark = false;
	bool isThemeSet = false;

	int argc;
	auto argv = CommandLineToArgvW(GetCommandLine(), &argc);

	for (int i=1; i!=argc; ++i)
	{
		StringBuffer<> name;
		StringBuffer<> value;

		if (i == 1 && argv[i][0] != '-')
		{
			file.Append(argv[i]);
			continue;
		}

		if (const tchar* equals = wcschr(argv[i],'='))
		{
			name.Append(argv[i], equals - argv[i]);
			value.Append(equals+1);
		}
		else
		{
			name.Append(argv[i]);
		}
		
		if (name.Equals(TC("-host")))
		{
			if (value.IsEmpty())
				return PrintHelp(TC("-host needs a value"));
			host.Append(value);
		}
		else if (name.Equals(TC("-named")))
		{
			if (value.IsEmpty())
				return PrintHelp(TC("-named needs a value"));
			named.Append(value);
		}
		else if (name.Equals(TC("-file")))
		{
			if (value.IsEmpty())
				return PrintHelp(TC("-file needs a value"));
			file.Append(value);
		}
		else if (name.Equals(TC("-port")))
		{
			if (!value.Parse(port))
				return PrintHelp(TC("Invalid value for -port"));
		}
		else if (name.Equals(TC("-listen")))
		{
			if (!value.IsEmpty())
				channel.Append(value.data);
			else
				channel.Append(TC("Default"));
		}
		else if (name.Equals(TC("-replay")))
		{
			replay = 1;
			if (!value.IsEmpty())
				value.Parse(replay);
		}
		else if (name.Equals(TC("-theme")))
		{
			isThemeSet = true;
			if (value.Equals(TC("dark")))
				useDark = true;
			else if (!value.Equals(TC("light")))
				return PrintHelp(TC("Invalid value for -theme. Must be 'light' or 'dark'"));
		}
		else
		{
			StringBuffer<> msg;
			msg.Appendf(TC("Unknown argument '%s'"), name.data);
			return PrintHelp(msg.data);
		}
	}

	LocalFree(argv);

	if (host.IsEmpty() && named.IsEmpty() && file.IsEmpty() && !channel.count)
		channel.Append(TC("Default")); // return PrintHelp(TC("No host/named/file provided. Add -host=<host> or -file=<file> or -named=<name>"));

	MessageBoxLogWriter logWriter;
	LoggerWithWriter logger(logWriter);

	NetworkBackendTcp networkBackend(logWriter);
	Visualizer visualizer(logger);
	logWriter.m_visualizer = &visualizer;

	if (isThemeSet)
		visualizer.SetTheme(useDark);


	if (channel.count)
	{
		if (!visualizer.ShowUsingListener(channel.data))
			logger.Error(TC("Failed listening to named pipe"));
	}
	else if (!named.IsEmpty())
	{
		if (!visualizer.ShowUsingNamedTrace(named.data))
			logger.Error(TC("Failed reading from mapped memory %s"), named.data);
	}
	else if (!host.IsEmpty())
	{
		if (!visualizer.ShowUsingSocket(networkBackend, host.data, u16(port)))
			logger.Error(TC("Failed to connect to %s:%u"), host.data, port);
	}
	else
	{
		if (!visualizer.ShowUsingFile(file.data, replay))
			logger.Error(TC("Failed to read trace file '%s'"), file.data);
	}

	while (true)
	{
		if (!visualizer.HasWindow())
			break;
		uba::Sleep(500);
	}
	return 0;
}
