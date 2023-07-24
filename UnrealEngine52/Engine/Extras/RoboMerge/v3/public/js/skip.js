// Copyright Epic Games, Inc. All Rights Reserved.

function goHome() {
    location.href = '/' + location.hash
}

function skipVerify() {
    addOnBeforeUnload('Leaving this page will abort the skip action')
    let queryParams = processQueryParameters(['bot', 'branch', 'cl', 'edge'], ["reason"])
    if (!queryParams) return;

    let requestedBotName = queryParams["bot"]
    let requestedBranchName = queryParams["branch"]
    let requestedBranchCl = parseInt(queryParams["cl"], 10)
    let requestedEdgeName = queryParams["edge"]

    // Fill in changelist value in HTML
    $('.changelist').each(function() {
        $(this).text(`CL ${requestedBranchCl}`)
    })

    // Verify user wants to skip
    let buttonDiv = $('<div align="center">').appendTo($('#reasonForm'))

    // Return to Robomerge homepage
    let cancelButton = $('<button type="button" class="btn btn-lg btn-info">').text(`Cancel`).appendTo(buttonDiv)
    cancelButton.click(function() {
        removeOnBeforeUnload()
        goHome()
    })

    // Perform skip
    let skipButton = $('<button type="button" class="btn btn-lg btn-primary" style="margin-left: .5em" disabled>').text(`Skip ${requestedBranchCl}`).appendTo(buttonDiv)

    // If a reason was passed in as a query parameter select it as default
    let skipReason = queryParams["reason"] // "notrelevant" | "willredo" | undefined
    if (skipReason) {
        if (skipReason === "notrelevant") {
            $('#notrelevant').prop("checked", true)
            skipButton.attr('disabled', false)
        } else if (skipReason === "willredo") {
            $('#willredo').prop("checked", true)
            skipButton.attr('disabled', false)
        }
    }

    if (isNaN(requestedBranchCl)) {
        skipFailure(`CL ${queryParams["cl"]} not a number.`)
        return
    }

    addOnBeforeUnload('Leaving this page will abort the skip action on CL ' + requestedBranchCl)

    getBranch(requestedBotName, requestedBranchName, function(data) {
        // Ensure we have data
        if (!data) {
            return
        }

        let requestedBranch = verifySkipRequest(requestedBotName, requestedBranchName, requestedEdgeName, requestedBranchCl, data.branch)
        // If the branch failed verification, please quick immediately
        if (!requestedBranch) {
            return
        }

        // Print preview
        $('#preview').append($('<h3>').text("Preview:"))
        $('#preview').append(renderSingleBranchTable(requestedBranch))

        skipButton.click(function() {
            removeOnBeforeUnload()

            // Grab values BEFORE locking form
            // This should only return one value
            const selectedReason = $("input[name='reason']:checked")[0].value
            
            cancelButton.attr('disabled', true)
            skipButton.attr('disabled', true)

            $.each($('.reasonRadio'), function(index, radioButton) {
                $(radioButton).attr('disabled', true)
            })

            skipButton.text(`Skipping ${requestedBranchCl}...`)

            let skipText = "Reason could not be determined, contact Robomerge Help"
            switch (selectedReason) {
                case "notrelevant":
                    skipText = "Work is not relevant to other branches"
                    break;
                
                case "willredo":
                    skipText = "User will redo work in the merge target"
                    break;
                
                default:
                    skipFailure("Error processing reason form, please contact Robomerge Help.")
                    return
            }

            getBranch(requestedBotName, requestedBranchName, function(data) {
                $('#preview').hide()

                // Ensure we still have data since we last verified and displayed the data
                // (The issue might have been resolved during that time.)
                requestedBranch = verifySkipRequest(requestedBotName, requestedBranchName, requestedEdgeName, requestedBranchCl, data.branch)
                if (!requestedBranch) {
                    // No longer valid.
                    skipFailure(`Skipping ${requestedBranchCl} is no longer valid.`)
                    return
                }
                
                doSkip(data, requestedEdgeName, requestedBranchCl, skipText)
            })

            // Now that we have setup the div, display to user
            $('#reasonSelectDiv').addClass('hidden')
        })

        $('#reasonForm').change(function() {
            // On form change, enable the skip button
            skipButton.attr('disabled', false)
        })

        // Now that we have setup the div, display to user
        $('#reasonSelectDiv').fadeIn("fast", "swing")
    })

}

// This function takes in requested bot/branch/cl combo (usually from query parameters processed earlier) and 
// ensures this is a valid request
function verifySkipRequest(requestedBotName, requestedNodeName, requestedEdgeName, requestedBranchCl, requestedBranchData) {
    // Verify we got a branch
    if (!requestedBranchData.def) {
        let errText = `Could not find matching branch for ${requestedBotName}:${requestedNodeName}.`
        if (requestedBranchData.message) {
            errText += `\n${requestedBranchData.message}`
        }
        skipFailure(errText)
        return
    }

    if (!requestedBranchData.edges) {
        skipFailure(`Could not find edges for bot ${requestedBotName.def.name}`)
        return
    }

    let requestedEdgeData = requestedBranchData.edges[requestedEdgeName.toUpperCase()]

    // Ensure this node actually has this requested edge
    if (!requestedEdgeData) {
        skipFailure(`${requestedBotName}:${requestedNodeName} has no edge for "${requestedEdgeName}".`)
        $('#result').append(renderSingleBranchTable(requestedBranchData))
        return
    }

    // Second, verify the request branch is blocked
    if (!requestedEdgeData.blockage) {
        skipFailure(`${requestedEdgeData.display_name} not currently blocked, no need to skip.`)
        $('#result').append(renderSingleBranchTable(requestedBranchData, requestedEdgeData))
        return
    }
    
    // Ensure the current blockage is applicable to the CL
    if (requestedEdgeData.blockage.change !== requestedBranchCl) {
        removeOnBeforeUnload()
        displayWarningMessage(`${requestedEdgeData.display_name} currently blocked, but not at requested CL ${requestedBranchCl}. Performing no action.`, false)
        $('#result').append(renderSingleBranchTable(requestedBranchData, requestedEdgeData))
        $('#returnbutton').removeClass("initiallyHidden")
        return
    }

    return requestedBranchData
}

function doSkip(fullData, edgeName, branchCl, skipText) {
    // Data all verified, time to perform the node operation.
    let queryData = {
        // If the forced CL is the same as the conflict CL, the conflict will be skipped
        cl: branchCl,
        reason: skipText,
        unblock: 'true'
    }

    let branchData = fullData.branch

    let botname = branchData.bot
    let nodename = branchData.def.name

    // Increment CL
    let skipOp = edgeAPIOp(botname, nodename, edgeName, '/set_last_cl?' + toQuery(queryData))
    
    skipOp.done(function(_success) {
        skipSuccess(`${fullData.user.displayName} successfully skipped CL ${branchCl} (${skipText}), resuming Robomerge for ${botname}:${nodename}.`)
    }).fail(function(jqXHR, _textStatus, errMsg) {
        const errText = `${jqXHR && jqXHR.status ? jqXHR.status + ": " : ""}${jqXHR && jqXHR.responseText ? jqXHR.responseText : errMsg}`
        skipFailure(`${fullData.user.displayName} encountered an error skipping CL ${branchCl} for ${botname}:${nodename} (Reason was: ${errText})`)        
    }).always(function() {
        getBranch(botname, nodename, function(data) {
            const upToDateEdgeData = data.edges && data.edges[targetBranchName.toUpperCase()] ? data.edges[targetBranchName.toUpperCase()] : null
            $('#result').append(renderSingleBranchTable(data.branch, upToDateEdgeData))
        })
    })
}

function skipFailure(message) {
    removeOnBeforeUnload()
    $(`<div class="alert alert-danger show" role="alert">`).html(`<strong>ERROR:</strong> ${message}`).appendTo($('#result'))
    $('#reasonSelectDiv').addClass('hidden')
    $('#returnbutton').removeClass("initiallyHidden")
}

function skipSuccess(message) {
    removeOnBeforeUnload()
    $(`<div class="alert alert-success show" role="alert">`).html(message).appendTo($('#result'))
    $('#reasonSelectDiv').addClass('hidden')
    $('#returnbutton').removeClass("initiallyHidden")
}