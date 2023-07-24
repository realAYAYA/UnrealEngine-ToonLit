// Copyright Epic Games, Inc. All Rights Reserved.

function goHome() {
    location.href = '/' + location.hash
}

function shelfVerify() {
    let queryParams = processQueryParameters(['bot', 'branch', 'cl', 'target'], ['targetStream'])
    if (!queryParams) return;

    let requestedBotName = queryParams["bot"]
    let requestedBranchName = queryParams["branch"]
    let targetBranchName = queryParams["target"]
    let optTargetStreamName = queryParams["targetStream"]
    let requestedBranchCl = parseInt(queryParams["cl"], 10)
    
    if (isNaN(requestedBranchCl)) {
        shelfFailure(`CL ${queryParams["cl"]} not a number.`)
        return
    }

    // Retrieve branch information to verify requested information
    getBranch(requestedBotName, requestedBranchName, function(data) {
        if (!data) {
            shelfFailure(`Couldn't find branch for ${requestedBranchName}`)
            return
        }

        let requestedBranch = verifyShelfRequest(requestedBotName, requestedBranchName, requestedBranchCl, targetBranchName, data.branch)
        // If the branch failed verification, please quit immediately
        if (!requestedBranch) {
            return
        }
        let requestedEdgeData = requestedBranch.edges[targetBranchName.toUpperCase()]

        // Print preview
        $('#preview').append(renderSingleBranchTable(requestedBranch, requestedEdgeData))
        let formDiv = $('#workspaceFormDiv')
        let workspaceForm = $('<form id="workspaceForm" style="display: inline-block; text-align: left">').prependTo(formDiv)

        // Check if conflict is already acknowledged or not
        const conflictAcknowledged = requestedEdgeData.blockage.acknowledger !== undefined

        // Determine if we have any relevant workspaces
        let foundWorkspaces = false
        getUserWorkspaces(function(wsdata) {
            // Ensure we have data
            if (!wsdata || !wsdata.data) {
                shelfFailure('Workspaces could not be found for user.')
                return
            }

            // Ensure we have a proper return code, or display the error message
            if (wsdata.statusCode !== 200) {
                shelfFailure("Could not retrieve workspaces: " + wsdata.message)
                return
            }

            let userWorkspaces = wsdata.data
            if (!userWorkspaces || userWorkspaces === []) {
                shelfFailure("Found no user workspaces.")
                return
            }

            // Collect and sort the user workspaces
            userWorkspaces.sort(function(a,b) {
                return a.client.localeCompare(b.client);
            });

            // Create form for relevant workspaces
            const addClientToForm = function(client, exactMatch) {
                const clientOptionLabel = $('<label>').text(client)

                // Add CSS class to differentiate 
                if (exactMatch) {
                    clientOptionLabel.addClass('workspaceExactMatch')
                } else {
                    clientOptionLabel.addClass('workspaceInexactMatch')
                }

                workspaceForm.append(
                    clientOptionLabel.prepend(
                        $(`<input type="radio" class="workspaceRadio" name="workspace" value="${client}">`)
                    )
                ).append($('<br>'))
            }

            // If the target branch is a stream, filter the available workspaces 
            if (optTargetStreamName) {
                // We have some elements in the HTML that hold the streamname as text
                $('.streamName').each(function() {
                    $(this).text(optTargetStreamName)
                })

                // Look through user's list of workspaces for any that are mapped to the target stream (or start with the same depot path)
                const depotPathMatches = optTargetStreamName.match(/(\/\/[\w-_]+\/)/)
                var depotPath
                if (depotPathMatches) {
                    depotPath = depotPathMatches[0]
                }
                
                console.log(`Selecting user workspaces for stream ${optTargetStreamName} (or in depot "${depotPath}")`)
                var exactMatches = []
                var inexactMatches = []
                $.each(userWorkspaces, function(_, workspace) {
                    if (!workspace.Stream) {
                        return
                    }

                    // Check for exact matches
                    if (workspace.Stream === optTargetStreamName) {
                        // Mark our tracker that we've encountered relevant workspaces
                        foundWorkspaces = true

                        exactMatches.push(workspace.client)
                    }
                    // Check for workspaces with similar depots
                    else if (workspace.Stream.startsWith(depotPath)) {
                        // Mark our tracker that we've encountered relevant workspaces
                        foundWorkspaces = true

                        inexactMatches.push(workspace.client)
                    }
                })

                // Append workspace options to our form
                for (var i in exactMatches) {
                    addClientToForm(exactMatches[i], true)
                }
                for (var i in inexactMatches) {
                    addClientToForm(inexactMatches[i], false)
                }
                

                // If we found no workspaces, display the tutorial on how to create a stream workspace
                if (!foundWorkspaces) {
                    $('#createStreamWorkspaceTutorial').removeClass('initiallyHidden')
                }
            } 
            // On non-stream requests, display all non-stream workspaces
            else {
                // This must be a non-Stream integration
                foundWorkspaces = true
                console.log(`Selecting non-stream user workspaces`)
                $.each(userWorkspaces, function(_, workspace) {
                    if (!workspace.Stream || workspace.Stream === "") {
                        // Append workspace option to our form
                        addClientToForm(workspace.client)
                    }
                })
            }

            // Check if we found any relevant data
            if (foundWorkspaces) {
                $('#preview').append(
                    $('<h3 class="centered-text">').text(`Creating shelf in ${targetBranchName}. Please choose one of your workspaces to create the shelf in:`)
                )
                if (optTargetStreamName && inexactMatches.length > 0) {
                    $('#preview').append(
                        $('<p class="centered-text">').html(`Workspaces that are mapped to ${optTargetStreamName} are in <strong>bold</strong>.<br />Alternative workspaces under the same depot (${depotPath}) are at the bottom of the list.`)
                    )
                }
                

                // Show form and submit/cancel buttons
                $('#preview').append(formDiv)
                formDiv.removeClass('initiallyHidden')
                let buttonDiv = $('#submitDiv')

                // Return to Robomerge homepage
                let cancelButton = $('<button type="button" class="btn btn-lg btn-info">').text(`Cancel`).appendTo(buttonDiv)
                cancelButton.click(function() {
                    goHome()
                })

                // Show the acknowledge checkbox option
                let acknowledgeCheckbox = $($("input[name='acknowledgeCheckbox']")[0])
                acknowledgeCheckbox.removeClass('initiallyHidden')
                // If the conflict is not acknowledged, default to checking this box to acknowledge with creating the shelf
                acknowledgeCheckbox.prop('checked', !conflictAcknowledged)

                let createShelfButton = $('<button type="button" id="createShelfButton" class="btn btn-lg btn-primary" disabled>').text('Create Shelf').appendTo(buttonDiv)
                workspaceForm.change(function() {
                    // On form change, enable the create shelf button
                    createShelfButton.attr('disabled', false);
                    
                    // Display warning message if the user selects an inexact match for their workspace
                    const selectedWorkspace = $("input[name='workspace']:checked")[0].value
                    const selectWorkspaceLabel = $($(`label:contains('${selectedWorkspace}')`)[0])
                    if (selectWorkspaceLabel.hasClass('workspaceInexactMatch')) {
                        $('#inexactWorkspaceWarning').removeClass('initiallyHidden')
                    } else {
                        // Exact match
                        $('#inexactWorkspaceWarning').addClass('initiallyHidden')
                    }
                })

                createShelfButton.click(function() {
                    // Grab values BEFORE locking form
                    // This should only return one value
                    const selectedWorkspace = $("input[name='workspace']:checked")[0].value
                    const acknowledgeWithShelf = acknowledgeCheckbox.prop('checked')

                    // Disable the form elements to lock in the choice
                    createShelfButton.attr('disabled', true)
                    cancelButton.attr('disabled', true)
                    acknowledgeCheckbox.attr('disabled', true)
                    $.each($('.workspaceRadio'), function(index, radioButton) {
                        $(radioButton).attr('disabled', true)
                    })

                    createShelfButton.text(`Creating shelf in ${selectedWorkspace}...`)

                     // Ensure we still have data since we last verified and displayed the data
                    // (The issue might have been resolved during that time.)
                    getBranch(requestedBotName, requestedBranchName, function(data) {
                        let requestedBranch = verifyShelfRequest(requestedBotName, requestedBranchName, requestedBranchCl, targetBranchName, data.branch)

                        // If the branch failed verification, please quit immediately
                        if (!requestedBranch) {
                            // No longer valid.
                            shelfFailure(`Shelving conflict for ${requestedBranchCl} is no longer valid.`)
                            return
                        }

                        doShelf(data, requestedBranchCl, targetBranchName, selectedWorkspace, acknowledgeWithShelf)
                    })
                })
            } else {
                $('#returnbutton').removeClass('initiallyHidden')
            }

        })
         
    })
}

// This function takes in requested bot/branch/cl combo (usually from query parameters processed earlier) and 
// ensures this is a valid request
function verifyShelfRequest(requestedBotName, requestedBranchName, requestedBranchCl, targetBranchName, branchData) {
    // Verify we got a branch
    if (!branchData.def) {
        let errText = `Could not find matching branch for ${requestedBotName}:${requestedBranchName}.`
        if (branchData.message) {
            errText += `\n${branchData.message}`
        }
        shelfFailure(errText)
        return
    }

    // Verify we can get the edge for the requested branch
    if (!branchData.edges || !branchData.edges[targetBranchName.toUpperCase()]) {
        shelfFailure(`Could not find a edge in ${requestedBotName}:${requestedBranchName} for requested edge "${targetBranchName}"`)
        return
    }

    let requestedBranch = branchData
    let requestedEdgeData = branchData.edges[targetBranchName.toUpperCase()]

    // Find conflict in question matching the requested CL
    // First, verify the request branch is paused
    if (!requestedEdgeData.blockage) {
        shelfFailure(`${requestedEdgeData.display_name} not currently blocked, no need to create a shelf.`)
        $('#result').append(renderSingleBranchTable(requestedBranch, requestedEdgeData))
        return
    }
    
    // Ensure the current pause is applicable to the CL
    if (requestedEdgeData.blockage.change != requestedBranchCl) {
        displayWarningMessage(`${requestedEdgeData.display_name} currently blocked, but not at requested CL ${requestedBranchCl}. Performing no action.`, false)
        $('#result').append(renderSingleBranchTable(requestedBranch, requestedEdgeData))
        $('#returnbutton').removeClass("initiallyHidden")
        return
    }

    return requestedBranch
}

function doShelf(fullData, branchCl, target, workspace, acknowledgeWithShelf) {
    // Data all verified, time to perform the shelf operation.
    let queryData = {
        cl: branchCl,
        target,
        workspace
    }

    let branchData = fullData.branch

    let botname = branchData.bot
    let branchname = branchData.def.name

    // Perform create shelf operation
    let createShelf = nodeAPIOp(botname, branchname, '/create_shelf?' + toQuery(queryData))
    createShelf.done(function(_success) {
        $('#preview').hide()
        let successMsg = `${fullData.user.displayName} successfully queued CL ${branchCl} for reconsideration. The shelf will appear in workspace "${workspace}"`
        
        // If we're set to acknowledge after the shelf is created, do so now
        if (acknowledgeWithShelf) {
            performAcknowledge(botname, branchname, target, branchCl).done(function(_success) {
                shelfSuccess(`${successMsg}.  Additionally, acknowledged blockage in ${botname}:${branchname}.`)
            }).fail(function(_jqXHR, _textStatus, _errMsg) {
                shelfWarning(`${successMsg}, but could not acknowledge blockage in ${botname}:${branchname}. Please contact Robomerge help if you need assistance`)
            })
        } 
        // Otherwise display a success and be done with it
        else {
            shelfSuccess(`${successMsg}.`)
        }

    }).fail(function(jqXHR, _textStatus, errMsg) {
        $('#preview').hide()
        const errText = `${jqXHR && jqXHR.status ? jqXHR.status + ": " : ""}${jqXHR && jqXHR.responseText ? jqXHR.responseText : errMsg}`
        let shelfFailureAlert = `An error was encountered reconsidering CL ${branchCl}. No shelf was created. Error message: "${errText}"`

        if (acknowledgeWithShelf) {
            shelfFailureAlert += "  Acknowledge operation skipped due to error."
        }

        shelfFailure(shelfFailureAlert)
    })
}

function performAcknowledge(requestedBotName, nodeBranch, targetBranch, requestedBranchCl) {
    const ackQueryData = {
        cl: requestedBranchCl
    }
    // This page is only relevant to edge acknowledgements
    return edgeAPIOp(requestedBotName, nodeBranch, targetBranch, '/acknowledge?' + toQuery(ackQueryData))
}

function shelfFailure(message) {
    $(`<div class="alert alert-danger show" role="alert">`).html(`<strong>ERROR:</strong> ${message}`).appendTo($('#result'))
    $('#returnbutton').removeClass("initiallyHidden")
}

function shelfWarning(message) {
    $(`<div class="alert alert-warning show" role="alert">`).html(`<strong>WARNING:</strong> ${message}`).appendTo($('#result'))
    $('#returnbutton').removeClass("initiallyHidden")
}

function shelfSuccess(message) {
    $(`<div class="alert alert-success show" role="alert">`).html(message).appendTo($('#result'))
    $('#nextSteps').removeClass("initiallyHidden")
    $('#returnbutton').removeClass("initiallyHidden")
}