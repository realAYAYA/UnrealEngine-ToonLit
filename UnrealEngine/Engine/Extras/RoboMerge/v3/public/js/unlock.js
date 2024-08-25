// Copyright Epic Games, Inc. All Rights Reserved.

function goHome() {
    location.href = '/' + location.hash
}

function unlockVerify() {
    let queryParams = processQueryParameters(['bot', 'branch', 'cl', 'target'])
    if (!queryParams) return;

    let requestedBotName = queryParams["bot"]
    let requestedBranchName = queryParams["branch"]
    let targetBranchName = queryParams["target"]
    let requestedBranchCl = parseInt(queryParams["cl"], 10)
    
    if (isNaN(requestedBranchCl)) {
        unlockFailure(`CL ${queryParams["cl"]} not a number.`)
        return
    }

    // Filled in if/when user clicks the verify button
    let unlockOperation = $.Deferred()
    
    // Get requested branch infomation and setup the page data
    getBranch(requestedBotName, requestedBranchName, function(data) {
        try {
            // Ensure we have data
            if (!data) {
                unlockFailure(`Couldn't retrieve data for ${requestedBotName}:${requestedBranchName}`)
                return
            }
            
            let requestedNode = data.branch

            // Verify we got a node
            if (!requestedNode) {
                let errText = `Could not find matching branch for ${requestedBotName}:${requestedBranchName}.`
                if (data.message) {
                    errText += `\n${data.message}`
                }
                unlockFailure(errText)
                return
            }

            let requestedEdge = requestedNode.edges[targetBranchName.toUpperCase()]

            // Ensure this node actually has this requested edge
            if (!requestedEdge) {
                unlockFailure(`${requestedBotName}:${requestedNodeName} has no edge for "${targetBranchName}".`)
                $('#result').append(renderSingleBranchTable(requestedNode))
                return
            }
        
            
            // Verify the request branch is paused
            if (!requestedEdge.blockage) {
                unlockFailure(`${requestedEdge.display_name} not currently blocked, no need to unlock.`)
                $('#result').append(renderSingleBranchTable(requestedNode))
                return
            }


            // Ensure the current pause is applicable to the CL
            if (requestedEdge.blockage.change !== requestedBranchCl) {
                displayWarningMessage(`${requestedEdge.display_name} currently blocked, but not at requested CL ${requestedBranchCl}. Performing no action.`, false)
                $('#singleBranchDisplay').append(renderSingleBranchTable(requestedNode)).fadeIn("fast", "swing")
                return
            }

            // Passed all safety checks. Display branch info and unlock warning, and begin verifying the request
            $('#branchPreviewBeforeVerify').append(renderSingleBranchTable(requestedNode, requestedEdge))

            $('.sourceName').each(function() {
                $(this).text(requestedBranchName)
            })
            $('.targetName').each(function() {
                $(this).text(targetBranchName)
            })
            $('.changelist').each(function() {
                $(this).text(requestedBranchCl)
            })

            // Data all verified, time to perform the node operation.
            let unlockQueryData = {
                cl: requestedEdge.blockage.change,
                target: targetBranchName
            }

            let verifyOperation = nodeAPIOp(requestedNode.bot, requestedNode.def.name, "/verifyunlock?" + toQuery(unlockQueryData))

            // If verification succeeded:
            verifyOperation.done(function(success) {
                // Unlock returns a JSON payload on success
                const unlockJson = JSON.parse(success)
                console.log(`Unlock Verification message: ${unlockJson.message}`)

                // Visualize the json
                visualizeUnlockVerification(requestedBranchCl, unlockJson)

                if (unlockJson.validRequest) {
                    $('#afterVerificationResultText').html('Unlock Verification Complete')
                    $('#afterVerificationResultText').append(
                        $('<h4 style="text-align: center">').text("Use the button below to proceed with the unlock operation")
                    )

                } else {
                    $('#afterVerificationResultText').html('<span><i class="fas fa-exclamation-triangle"></i></span> Unlock Verification Returned Issues.')
                    $('#afterVerificationResultText').css('color', 'red');
                }

                transitionPostVerification()

                // If we're a valid request, add form buttons to formally request an unlock
                if (unlockJson.validRequest) {
                    const formButtonDiv = $('#formButtons')

                    // Return to Robomerge homepage
                    let cancelButton = $('<button type="button" class="btn btn-lg btn-info" style="margin:.3em;">').text(`Cancel`).appendTo(formButtonDiv)
                    cancelButton.click(function() {
                        goHome()
                    })

                    // Perform unlock
                    let unlockButton = $('<button type="button" class="btn btn-lg btn-primary" style="margin:.3em;">').text(`Perform Unlock`).appendTo(formButtonDiv)
                    unlockButton.click(function() {
                        unlockButton.attr('disabled', true)
                        unlockButton.text(`Requesting unlock with CL ${requestedBranchCl}...`)
                        transitionUnlockChanges(requestedBranchCl)

                        nodeAPIOp(requestedNode.bot, requestedNode.def.name, "/unlockchanges?" + toQuery(unlockQueryData))
                            .done(function (success) {
                                unlockOperation.resolve(success)
                            })
                            .fail(function(jqXHR, textStatus, errMsg) {
                                unlockOperation.reject(jqXHR, textStatus, errMsg)
                            })
                    })
                } else {
                    $('#returnbutton').removeClass("initiallyHidden")
                }
            })

            // If verification failed:
            verifyOperation.fail(function(jqXHR, _textStatus, errMsg) {
                transitionPostVerification()

                $('#returnbutton').removeClass("initiallyHidden")
                const errText = `${jqXHR && jqXHR.status ? jqXHR.status + ": " : ""}${jqXHR && jqXHR.responseText ? jqXHR.responseText : errMsg}`
                unlockFailure(`Encountered issues verifying files from CL ${requestedEdge.blockage.change} to unlock changes in ${targetBranchName}: "${errText}" If you think this is an error, please contact Robomerge support.`)
                
                // Call getBranchList a second time to display most up-to-date data
                getBranch(requestedNode.bot, requestedNode.def.name, function(data) {
                    const upToDateEdgeData = data.edges && data.edges[targetBranchName.toUpperCase()] ? data.edges[targetBranchName.toUpperCase()] : null
                    $('#afterVerification').append(renderSingleBranchTable(data.branch, upToDateEdgeData))
                })
            })
        }
        catch (err) {
            unlockFailure(`Error encountered displaying unlock verification results: ${err}`)
        }
    })

    // Logic controlling unlock operation
    $.when(unlockOperation).done(function(success) {
        try {
            transitionDisplayUnlockResults()
            unlockSuccess(success)
        }
        catch (err) {
            unlockFailure(`Error encountered displaying unlock files results: ${err}`)
        }
        
    }).fail(function(jqXHR, textStatus, errMsg) {
        try {
            transitionDisplayUnlockResults()
            unlockFailure(`${textStatus}: ${errMsg}`)
        }
        catch (err) {
            unlockFailure(`Error encountered displaying unlock files failure: ${err}`)
        }
    })
}

function transitionPostVerification() {
    // Remove before verify animation, fade in new visualization
    $('#beforeVerification').fadeOut("fast", "swing", function() { $('#afterVerification').fadeIn("fast") })
    $('#loadingDiv').fadeOut("fast", "swing")
}

function transitionUnlockChanges(changelist) {
    $('#loadingText').text(`Performing unlock with CL ${changelist}...`)
    $('#afterVerification').fadeOut("fast", "swing", function() { if ($('#returnbutton').hasClass("initiallyHidden")) { $('#loadingDiv').fadeIn("fast") }})
}

function transitionDisplayUnlockResults() {
    $('#loadingDiv').fadeOut("fast", "swing", function() { $('#afterUnlock').fadeIn("fast") })
    $('#returnbutton').removeClass("initiallyHidden")
}

// Create visualization of the file, with links to swarm
function visualizeAuthor(author, files) {
    const authorDiv = $(`<div id="${author}" class="unlock-visual">`)
    if (robomergeUser && author.toLowerCase() === robomergeUser.userName.toLowerCase()) {
        authorDiv.append($('<h3>').html("Locked by <strong>you</strong>"))
    } else {
        authorDiv.append($('<h3>').html(`Locked by ${author}`))
    }
    
    // Begin compiling the list of relevant files in this changelist
    let fileList = $('<ul>')
    for (const file of files) {
        fileList.append($('<li>').html(file))
    }
    fileList.appendTo(authorDiv)

    return authorDiv
}

function visualizeUnlockVerification(requestedBranchCl, unlockJson) {
    // Debug
    const prettyJson = JSON.stringify(unlockJson, null, 2)
    console.log(`Unlock Verification Debug JSON:\n${prettyJson}`)
    let linkToSwarm = makeClLink(requestedBranchCl, `CL ${requestedBranchCl}`)

    let authorDict = {}
    for (const file of unlockJson.files) {

        if (!authorDict[file.user]) {
            authorDict[file.user] = {
                files: [file.depotPath]
            }
        }
        else {
            authorDict[file.user].files.push(file.depotPath)
        }
    }

    Object.keys(authorDict).forEach(function(key) {
        $('#resultVisualization').append(visualizeAuthor(key, authorDict[key].files))
    })
}

function unlockFailure(message) {
    displayErrorMessage(message, false)
    $('#loadingDiv').fadeOut("fast", "swing")
    $('#returnbutton').removeClass("initiallyHidden")
}

function unlockSuccess(message) {
    displaySuccessfulMessage(message, false)
    $('#loadingDiv').fadeOut("fast", "swing")
    $('#returnbutton').removeClass("initiallyHidden")
}