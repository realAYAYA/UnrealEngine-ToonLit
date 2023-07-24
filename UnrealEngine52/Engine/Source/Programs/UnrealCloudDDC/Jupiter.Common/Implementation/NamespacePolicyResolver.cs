// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;

namespace Jupiter.Common
{
    public interface INamespacePolicyResolver
    {
        public NamespacePolicy GetPoliciesForNs(NamespaceId ns);

        static NamespaceId JupiterInternalNamespace => new NamespaceId("jupiter-internal");
    }
    public class NamespacePolicyResolver : INamespacePolicyResolver
    {
        private readonly IOptionsMonitor<NamespaceSettings> _namespaceSettings;
        private readonly NamespacePolicy _internalNamespacePolicy;

        public NamespacePolicyResolver(IOptionsMonitor<NamespaceSettings> namespaceSettings, IOptionsMonitor<AuthSettings> authSettings)
        {
            _namespaceSettings = namespaceSettings;
            _internalNamespacePolicy = new NamespacePolicy()
            {
                StoragePool = "persistent"
            };

            // if auth is disabled add a default namespace policy that allows access to everything unless something else exists to override it
            if (!authSettings.CurrentValue.Enabled)
            {
                _namespaceSettings.CurrentValue.Policies.TryAdd("*", new NamespacePolicy() { Acls = new List<AclEntry> { new() { Claims = new List<string> {"*"} } } });
            }
        }

        public NamespacePolicy GetPoliciesForNs(NamespaceId ns)
        {
            if (_namespaceSettings.CurrentValue.Policies.TryGetValue(ns.ToString(),
                    out NamespacePolicy? settings))
            {
                return settings;
            }
            
            // attempt to find the default mapping
            if (_namespaceSettings.CurrentValue.Policies.TryGetValue("*",
                    out NamespacePolicy? defaultSettings))
            {
                return defaultSettings;
            }

            // if no override has been specified for the internal jupiter namespace then we default it
            if (ns == INamespacePolicyResolver.JupiterInternalNamespace)
            {
                return _internalNamespacePolicy;
            }

            throw new UnknownNamespaceException($"Unable to find a valid policy for namespace {ns}");
        }
    }

    public class UnknownNamespaceException : Exception
    {
        public UnknownNamespaceException(string message) : base(message)
        {
        }
    }
}
