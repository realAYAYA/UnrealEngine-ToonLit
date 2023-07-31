// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using Microsoft.AspNetCore.Mvc.ActionConstraints;
using Microsoft.Extensions.Primitives;

namespace Jupiter
{
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class RequiredContentTypeAttribute : Attribute, IActionConstraint
    {
        private readonly string _mediaTypeName;

        public RequiredContentTypeAttribute(string mediaTypeName)
        {
            _mediaTypeName = mediaTypeName;
        }

        public string MediaTypeName => _mediaTypeName;

        public int Order => 0;

        public bool Accept(ActionConstraintContext context)
        {
            StringValues contentTypeHeader = context.RouteContext.HttpContext.Request.Headers["Content-Type"];

            bool valid = contentTypeHeader.ToList().Contains(_mediaTypeName, StringComparer.InvariantCultureIgnoreCase);
            return valid;
        }
    }
}
