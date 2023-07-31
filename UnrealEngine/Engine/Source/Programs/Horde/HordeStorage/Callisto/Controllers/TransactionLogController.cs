// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel.DataAnnotations;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Callisto.Implementation;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using Serilog;

namespace Callisto.Controllers
{
    [ApiController]
    [FormatFilter]
    [Route("api/v1/t")]
    public class TransactionLogController : ControllerBase
    {
        private readonly ITransactionLogs _transactionLogs;
        private readonly IDiagnosticContext _diagnosticContext;
        private readonly IAuthorizationService _authorizationService;

        private readonly IOptionsMonitor<JupiterSettings> _settings;
        private readonly ILogger _logger = Log.ForContext<TransactionLogController>();

        public TransactionLogController(ITransactionLogs transactionLogs, IOptionsMonitor<JupiterSettings> options, IDiagnosticContext diagnosticContext, IAuthorizationService authorizationService)
        {
            _transactionLogs = transactionLogs;
            _diagnosticContext = diagnosticContext;
            _authorizationService = authorizationService;

            _settings = options;
        }

        [HttpGet("")]
        [ProducesDefaultResponseType]
        [ProducesResponseType(type: typeof(ProblemDetails), 400)]
        [Authorize("TLog.read")]
        public IActionResult Get()
        {
            NamespaceId[] namespaces = _transactionLogs.GetNamespaces();

            // filter namespaces down to only the namespaces the user has access to
            namespaces = namespaces.Where(ns =>
            {
                Task<AuthorizationResult> authorizationResult = _authorizationService.AuthorizeAsync(User, new NamespaceAccessRequest()
                {
                    Namespace = ns,
                    Actions = new [] { AclAction.ReadTransactionLog }
                }, NamespaceAccessRequirement.Name);
                return authorizationResult.Result.Succeeded;
            }).ToArray();

            return Ok(new {
                Logs = namespaces
            });
        }

        [HttpGet("{ns}/{start}.{format?}")]
        [ProducesDefaultResponseType]
        [ProducesResponseType(type: typeof(ProblemDetails), 400)]
        [Authorize("TLog.read")]
        public async Task<IActionResult> Get(
            [Required] NamespaceId ns,
            [Required] long start,
            [FromQuery] string? notSeenAtSite = null,
            [FromQuery] Guid? expectedGeneration = null,
            [FromQuery] int count = 100,
            [FromQuery] int maxOffsetsAttempted = 100
        )
        {
            AuthorizationResult authorizationResult = await _authorizationService.AuthorizeAsync(User, new NamespaceAccessRequest()
            {
                Namespace = ns,
                Actions = new[] { AclAction.ReadTransactionLog }
            }, NamespaceAccessRequirement.Name);

            if (!authorizationResult.Succeeded)
            {
                return Forbid();
            }

            TransactionLogDescription description = _transactionLogs.Get(ns).Describe();
            // if a given generation is expected verify we are using that generation for the namespace
            if (expectedGeneration != null)
            {
                if (description.Generation != expectedGeneration)
                {
                    return BadRequest(new ProblemDetails
                    {
                        Title =
                            $"Generation mismatch, current generation is {description.Generation} expected {expectedGeneration}",
                        Type = "http://jupiter.epicgames.com/callisto/newGeneration"
                    });
                }
            }

            // try to recover by finding the next valid object in the log
            for (int i = 0; i < maxOffsetsAttempted; i++)
            {
                try
                {
                    long currentOffset = start + i;
                    TransactionEvents events = await _transactionLogs.Get(ns).Get(currentOffset, count, notSeenAtSite);
                    // if the current offset was invalid we attempt to move it to find the next valid set of values but if we had some valid events before it we return those first
                    if (events.TransactionLogMismatchFoundAt.HasValue && events.TransactionLogMismatchFoundAt.Value == currentOffset)
                    {
                        continue;
                    }

                    return Ok(new CallistoGetResponse(events.Events, description.Generation, events.CurrentOffset));
                }
                catch (FileNotFoundException e)
                {
                    return BadRequest(new ProblemDetails {Title = $"Unable to find log file {e.FileName}", Type = "http://jupiter.epicgames.com/callisto/unknownNamespace"});
                }
                catch (EventHashMismatchException)
                {
                    // if the event has a bad hash we skip it and search for the next event in the list
                    continue;
                }
            }

            return BadRequest(new ProblemDetails {Title = $"Unable to find a valid offset in {ns} tried {start} to {start+maxOffsetsAttempted}", Type = "http://jupiter.epicgames.com/callisto/transactionLogOffsetMismatch" });
        }

        [HttpGet("{ns}/sites")]
        [ProducesDefaultResponseType]
        [ProducesResponseType(type: typeof(ProblemDetails), 400)]
        [Authorize("TLog.read")]
        public async Task<IActionResult> Sites(
            [Required] NamespaceId ns
        )
        {
            AuthorizationResult authorizationResult = await _authorizationService.AuthorizeAsync(User, new NamespaceAccessRequest()
            {
                Namespace = ns,
                Actions = new[] { AclAction.WriteTransactionLog }
            }, NamespaceAccessRequirement.Name);

            if (!authorizationResult.Succeeded)
            {
                return Forbid();
            }

            TransactionLogDescription description = _transactionLogs.Get(ns).Describe();
            try
            {
                return Ok(new {Sites = description.KnownSites});
            }
            catch (FileNotFoundException e)
            {
                return BadRequest(new ProblemDetails { Title = $"Unable to find log file {e.FileName}" });
            }
        }

        [HttpPost("{ns}")]
        [Authorize("TLog.write")]
        public async Task<IActionResult> PostNewTransaction(
            [Required] NamespaceId ns,
            [Required] [FromBody] TransactionEvent request
        )
        {
            AuthorizationResult authorizationResult = await _authorizationService.AuthorizeAsync(User, new NamespaceAccessRequest()
            {
                Namespace = ns,
                Actions = new[] { AclAction.WriteTransactionLog }
            }, NamespaceAccessRequirement.Name);

            if (!authorizationResult.Succeeded)
            {
                return Forbid();
            }

            if (string.IsNullOrEmpty(_settings.CurrentValue.CurrentSite))
            {
                throw new Exception(
                    "Current site setting is missing, this has to be part of the deployment and has to be globally unique");
            }

            // add the current site to the list of sites that have seen this event
            request.Locations.Add(_settings.CurrentValue.CurrentSite);
            long id;
            switch (request)
            {
                case AddTransactionEvent addEvent:
                {
                    id = await _transactionLogs.Get(ns).NewTransaction(addEvent);
                    break;
                }
                case RemoveTransactionEvent removeEvent:
                {
                    id = await _transactionLogs.Get(ns).NewTransaction(removeEvent);
                    break;
                }
                default:
                    return BadRequest(new ProblemDetails {Title = $"Unknown request type: \"{request}\""});
            }
            _logger.Information("New {Event} added as {TransactionId} in {Namespace}", request, id, ns);
            _diagnosticContext.Set("transaction-id", id);
            return Ok(new NewTransactionResponse {Offset = id});

        }

        // this exists for admin use only
        [HttpDelete("{ns}")]
        [Authorize("TLog.delete")]
        public async Task<IActionResult> DropTransactionLog(
            [Required] NamespaceId ns
        )
        {
            AuthorizationResult authorizationResult = await _authorizationService.AuthorizeAsync(User, new NamespaceAccessRequest()
            {
                Namespace = ns,
                Actions = new[] { AclAction.WriteTransactionLog }
            }, NamespaceAccessRequirement.Name);

            if (!authorizationResult.Succeeded)
            {
                return Forbid();
            }

            await _transactionLogs.Get(ns).Drop();

            return Ok();
        }

        // TODO: Compounding of the log, do we offer a endpoint for this or is it internal only?
    }

    public class CallistoGetResponse
    {
        public CallistoGetResponse(TransactionEvent[] events, Guid generation, long currentOffset)
        {
            Events = events;
            Generation = generation;
            CurrentOffset = currentOffset;
        }

        public long CurrentOffset { get; set; }

        public Guid Generation { get; set; }
        public TransactionEvent[] Events { get; set; }
    }
    public class NewTransactionResponse
    {
        public long Offset { get; set; }
    }
}
