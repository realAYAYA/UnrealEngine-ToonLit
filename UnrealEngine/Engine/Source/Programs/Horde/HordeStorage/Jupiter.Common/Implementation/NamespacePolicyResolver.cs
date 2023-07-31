// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
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

        public NamespacePolicyResolver(IOptionsMonitor<NamespaceSettings> namespaceSettings)
        {
            _namespaceSettings = namespaceSettings;
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

            throw new UnknownNamespaceException($"Unable to find a valid policy for namespace {ns}");
        }
    }
}
