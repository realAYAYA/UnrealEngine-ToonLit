// Copyright Epic Games, Inc. All Rights Reserved.
function acknowledge() {
    let queryParams = processQueryParameters(['bot', 'branch', 'cl'], ['edge'])
    if (!queryParams) return;

    let requestedBotName = queryParams["bot"]
    let requestedBranchName = queryParams["branch"]
    let requestedBranchCl = parseInt(queryParams["cl"], 10)
    let requestedEdgeName = queryParams["edge"]
    let requestedEdgeData = null

    if (isNaN(requestedBranchCl)) {
        acknowledgeFailure(`CL ${queryParams["cl"]} not a number.`)
        return
    }

    // Filled in if/when acknowledge operation is finished
    let ackOperation = $.Deferred()

    getBranch(requestedBotName, requestedBranchName, function(data) {
        // Ensure we have data
        if (!data) {
            return
        }

        // Verify we got a branch
        if (!data.branch) {
            let errText = `Could not find matching branch for ${requestedBotName}:${requestedBranchName}.`
            if (data.message) {
                errText += `\n${data.message}`
            }
            acknowledgeFailure(errText)
            return
        }
        let requestedBranchData = data.branch

        if (requestedEdgeName) {
            if (!requestedBranchData.edges) {
                acknowledgeFailure(`Could not find edges for bot ${requestedBotName.display_name}`)
                return
            }
        
            requestedEdgeData = requestedBranchData.edges[requestedEdgeName.toUpperCase()]
        
            // Ensure this node actually has this requested edge
            if (!requestedEdgeData) {
                acknowledgeFailure(`${requestedBotName}:${requestedNodeName} has no edge for "${requestedEdgeName}".`)
                $('#result').append(renderSingleBranchTable(requestedBranchData))
                return
            }
        
            // Second, verify the request branch is paused
            if (!requestedEdgeData.blockage) {
                acknowledgeFailure(`${requestedEdgeData.display_name} not currently blocked, no need to acknowledge.`)
                $('#result').append(renderSingleBranchTable(requestedBranchData, requestedEdgeData))
                return
            }
        }

        let targetData = requestedEdgeData || requestedBranchData

         // Find conflict in question matching the requested CL
        // First, verify the request branch is paused
        if (!targetData.is_blocked) {
            acknowledgeFailure(`${targetData.display_name} not currently blocked, no need to acknowledge.`)
            $('#result').append(renderSingleBranchTable(requestedBranchData, requestedEdgeData))
            return
        }
        
        // Ensure the current pause is applicable to the CL
        if (targetData.blockage.change !== requestedBranchCl) {
            displayWarningMessage(`${targetData.display_name} currently blocked, but not at requested CL ${requestedBranchCl}. Performing no action.`, false)
            $('#result').append(renderSingleBranchTable(requestedBranchData, requestedEdgeData))
            return
        }

        // Data all verified, time to perform the node operation.
        let ackQueryData = {
			cl: targetData.blockage.change
        }
        
        let targetOp = nodeAPIOp
        let targetArgs = [requestedBranchData.bot, requestedBranchData.def.name]
        if (requestedEdgeData) {
            targetOp = edgeAPIOp
            targetArgs = [requestedBranchData.bot, requestedBranchData.def.name, requestedEdgeData.target]
        }

        targetOp(...targetArgs, "/acknowledge?" + toQuery(ackQueryData), 
            function(_success) {
                ackOperation.resolve(data.user.displayName, requestedBranchCl, requestedBotName, requestedBranchName, targetData)
            },
            function(errMsg) {
                ackOperation.reject(data.user.displayName, requestedBranchCl, targetData.display_name, errMsg)
            }
        )
    })

    // Process Acknowledge Result
    ackOperation.done(function(username, changelist, bot, branch, targetData) {
        acknowledgeSuccess(`${username} successfully acknowledged CL ${changelist} for ${targetData.display_name}`)

        // After a successful acknowledgement, offer to create a shelf for the user for all of the current conflicts
        let buttonCreated = false

        const button = createCreateShelfButton(bot, branch, changelist, targetData.blockage)
        if (button) {
            $('#createShelfButtonsDiv').append(button)
            buttonCreated = true
        }

        if (buttonCreated) {
            $('#createShelfDiv').removeClass("initiallyHidden")
        }
    }).fail(function(username, changelist, targetName, errText) {
        acknowledgeFailure(`${username} encountered error "${errText}" acknowledging CL ${changelist} for ${targetName}. Please contact Robomerge support.`)
    }).always(function() {
        // Call getBranchList a second time to display most up-to-date data
        getBranch(requestedBotName, requestedBranchName, function(data) {
            const upToDateEdgeData = requestedEdgeName && data.edges && data.edges[targetBranchName.toUpperCase()] ? data.edges[targetBranchName.toUpperCase()] : null
            $('#result').append(renderSingleBranchTable(data.branch, upToDateEdgeData))
        })
    })

}

function createCreateShelfButton(botname, branchname, changelist, blockage) {
    // Don't allow shelf creation for syntax error or exclusive checkout blockages
    if (!blockage.targetBranchName) {
        return null
    }

    // Create Shelf
    let shelfRequestUrl = `/op/createshelf?bot=${botname}&branch=${branchname}&cl=${changelist}&target=${blockage.targetBranchName}`
    if (blockage.targetStream) {
        shelfRequestUrl += '&targetStream=' + encodeURIComponent(blockage.targetStream);
    }

    return $(`<button class="btn btn-lg btn-block btn-basic" style="margin-bottom: .5em;" onclick="window.location.href='${shelfRequestUrl}'">`).html(`Create Shelf for ${changelist} -> ${blockage.targetBranchName}`)
}

function acknowledgeFailure(message) {
    $(`<div class="alert alert-danger show" role="alert">`).html(`<strong>ERROR:</strong> ${message}`).appendTo($('#result'))
}

function acknowledgeSuccess(message) {
    $(`<div class="alert alert-success show" role="alert">`).html(`<strong>SUCCESS!</strong> ${message}`).appendTo($('#result'))
}