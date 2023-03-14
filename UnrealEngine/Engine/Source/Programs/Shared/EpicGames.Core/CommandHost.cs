// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace EpicGames.Core
{
	/// <summary>
	/// Interface for commands
	/// </summary>
	public interface ICommand
	{
		/// <summary>
		/// Configure this object with the given command line arguments
		/// </summary>
		/// <param name="arguments">Command line arguments</param>
		/// <param name="logger">Logger for output</param>
		void Configure(CommandLineArguments arguments, ILogger logger);

		/// <summary>
		/// Gets all command line parameters to show in help for this command
		/// </summary>
		/// <param name="arguments">The command line arguments</param>
		/// <returns>List of name/description pairs</returns>
		List<KeyValuePair<string, string>> GetParameters(CommandLineArguments arguments);

		/// <summary>
		/// Execute this command
		/// </summary>
		/// <param name="logger">The logger to use for this command</param>
		/// <returns>Exit code</returns>
		Task<int> ExecuteAsync(ILogger logger);
	}

	/// <summary>
	/// Interface describing a command that can be exectued
	/// </summary>
	public interface ICommandFactory
	{
		/// <summary>
		/// Names for this command
		/// </summary>
		public string[] Names { get; }

		/// <summary>
		/// Short description for the mode. Will be displayed in the help text.
		/// </summary>
		public string Description { get; }

		/// <summary>
		/// Create a command instance
		/// </summary>
		public ICommand CreateInstance(IServiceProvider serviceProvider);
	}

	/// <summary>
	/// Attribute used to specify names of program modes, and help text
	/// </summary>
	public class CommandAttribute : Attribute
	{
		/// <summary>
		/// Names for this command
		/// </summary>
		public string[] Names { get; }

		/// <summary>
		/// Short description for the mode. Will be displayed in the help text.
		/// </summary>
		public string Description { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name of the mode</param>
		/// <param name="description">Short description for display in the help text</param>
		public CommandAttribute(string name, string description)
		{
			Names = new string[] { name };
			Description = description;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="category">Category for this command</param>
		/// <param name="name">Name of the mode</param>
		/// <param name="description">Short description for display in the help text</param>
		public CommandAttribute(string category, string name, string description)
		{
			Names = new string[] { category, name };
			Description = description;
		}
	}

	/// <summary>
	/// Base class for all commands that can be executed by HordeAgent
	/// </summary>
	public abstract class Command : ICommand
	{
		/// <inheritdoc/>
		public virtual void Configure(CommandLineArguments arguments, ILogger logger)
		{
			arguments.ApplyTo(this, logger);
		}

		/// <inheritdoc/>
		public virtual List<KeyValuePair<string, string>> GetParameters(CommandLineArguments arguments)
		{
			return CommandLineArguments.GetParameters(GetType());
		}

		/// <inheritdoc/>
		public abstract Task<int> ExecuteAsync(ILogger logger);
	}

	/// <summary>
	/// Default implementation of a command factory
	/// </summary>
	class CommandFactory : ICommandFactory
	{
		public string[] Names { get; }
		public string Description { get; }

		public Type Type { get; }

		public CommandFactory(string[] names, string description, Type type)
		{
			Names = names;
			Description = description;
			Type = type;
		}

		public ICommand CreateInstance(IServiceProvider serviceProvider) => (ICommand)serviceProvider.GetRequiredService(Type);
		public override string ToString() => String.Join(" ", Names);
	}

	/// <summary>
	/// Entry point for dispatching commands
	/// </summary>
	public static class CommandHost
	{
		/// <summary>
		/// Adds services for executing the 
		/// </summary>
		/// <param name="services"></param>
		/// <param name="assembly"></param>
		public static void AddCommandsFromAssembly(this IServiceCollection services, Assembly assembly)
		{
			List<(CommandAttribute, Type)> commands = new List<(CommandAttribute, Type)>();
			foreach (Type type in assembly.GetTypes())
			{
				if (typeof(ICommand).IsAssignableFrom(type) && !type.IsAbstract)
				{
					CommandAttribute? attribute = type.GetCustomAttribute<CommandAttribute>();
					if (attribute != null)
					{
						services.AddTransient(type);
						services.AddTransient(typeof(ICommandFactory), sp => new CommandFactory(attribute.Names, attribute.Description, type));
					}
				}
			}
		}

		/// <summary>
		/// Entry point for executing registered command types in a particular assembly
		/// </summary>
		/// <param name="args">Command line arguments</param>
		/// <param name="serviceProvider">The service provider for the application</param>
		/// <param name="defaultCommandType">The default command type</param>
		/// <returns>Return code from the command</returns>
		public static async Task<int> RunAsync(CommandLineArguments args, IServiceProvider serviceProvider, Type? defaultCommandType)
		{
			// Find all the command types
			List<ICommandFactory> commandFactories = serviceProvider.GetServices<ICommandFactory>().ToList();

			// Check if there's a matching command
			ICommand? command = null;
			ICommandFactory? commandFactory = null;

			// Parse the positional arguments for the command name
			string[] positionalArgs = args.GetPositionalArguments();
			if (positionalArgs.Length == 0)
			{
				if (defaultCommandType == null || args.HasOption("-Help"))
				{
					Console.WriteLine("Usage:");
					Console.WriteLine("    [Command] [-Option1] [-Option2]...");
					Console.WriteLine("");
					Console.WriteLine("Commands:");

					PrintCommands(commandFactories);

					Console.WriteLine("");
					Console.WriteLine("Specify \"<CommandName> -Help\" for command-specific help");
					return 0;
				}
				else 
				{
					command = (ICommand)serviceProvider.GetService(defaultCommandType);
				}
			}
			else
			{
				foreach (ICommandFactory factory in commandFactories)
				{
					if (factory.Names.SequenceEqual(positionalArgs, StringComparer.OrdinalIgnoreCase))
					{
						command = factory.CreateInstance(serviceProvider);
						commandFactory = factory;
						break;
					}
				}
				if (command == null)
				{
					ConsoleUtils.WriteError($"Invalid command '{String.Join(" ", positionalArgs)}'");
					Console.WriteLine("");
					Console.WriteLine("Available commands:");

					PrintCommands(commandFactories);
					return 1;
				}
			}

			// If the help flag is specified, print the help info and exit immediately
			if (args.HasOption("-Help"))
			{
				if (commandFactory == null)
				{
					HelpUtils.PrintHelp(null, null, command.GetParameters(args));
				}
				else
				{
					HelpUtils.PrintHelp(String.Join(" ", commandFactory.Names), commandFactory.Description, command.GetParameters(args));
				}
				return 1;
			}

			// Configure the command
			ILogger logger = serviceProvider.GetRequiredService<ILoggerProvider>().CreateLogger("CommandHost");
			try
			{
				command.Configure(args, logger);
				args.CheckAllArgumentsUsed(logger);
			}
			catch (CommandLineArgumentException ex)
			{
				ConsoleUtils.WriteError(ex.Message);
				Console.WriteLine("");
				Console.WriteLine("Valid parameters:");

				HelpUtils.PrintTable(command.GetParameters(args), 4, 24);
				return 1;
			}

			// Execute all the commands
			try
			{
				return await command.ExecuteAsync(logger);
			}
			catch (FatalErrorException ex)
			{
				logger.LogCritical(ex, "Fatal error.");
				return ex.ExitCode;
			}
			catch (Exception ex)
			{
				logger.LogCritical(ex, "Fatal error.");
				return 1;
			}
		}

		/// <summary>
		/// Print a formatted list of all the available commands
		/// </summary>
		/// <param name="attributes">List of command attributes</param>
		static void PrintCommands(IEnumerable<ICommandFactory> attributes)
		{
			List<KeyValuePair<string, string>> commands = new List<KeyValuePair<string, string>>();
			foreach (ICommandFactory attribute in attributes)
			{
				commands.Add(new KeyValuePair<string, string>(String.Join(" ", attribute.Names), attribute.Description));
			}
			HelpUtils.PrintTable(commands.OrderBy(x => x.Key).ToList(), 4, 20);
		}
	}
}
