// Copyright Epic Games, Inc. All Rights Reserved.

function goHome() {
    location.href = '/' + location.hash
}

function stompVerify() {
    let queryParams = processQueryParameters(['bot', 'branch', 'cl', 'target'])
    if (!queryParams) return;

    let requestedBotName = queryParams["bot"]
    let requestedBranchName = queryParams["branch"]
    let targetBranchName = queryParams["target"]
    let requestedBranchCl = parseInt(queryParams["cl"], 10)
    
    if (isNaN(requestedBranchCl)) {
        stompFailure(`CL ${queryParams["cl"]} not a number.`)
        return
    }

    // Filled in if/when user clicks the stomp button
    let stompOperation = $.Deferred()
    
    // Get requested branch infomation and setup the page data
    let visualizationOperation = getBranch(requestedBotName, requestedBranchName, function(data) {
        try {
            // Ensure we have data
            if (!data) {
                stompFailure(`Couldn't retrieve data for ${requestedBotName}:${requestedBranchName}`)
                return
            }
            
            let requestedNode = data.branch

            // Verify we got a node
            if (!requestedNode) {
                let errText = `Could not find matching branch for ${requestedBotName}:${requestedBranchName}.`
                if (data.message) {
                    errText += `\n${data.message}`
                }
                stompFailure(errText)
                return
            }

            let requestedEdge = requestedNode.edges[targetBranchName.toUpperCase()]

            // Ensure this node actually has this requested edge
            if (!requestedEdge) {
                stompFailure(`${requestedBotName}:${requestedNodeName} has no edge for "${targetBranchName}".`)
                $('#result').append(renderSingleBranchTable(requestedNode))
                return
            }
        
            
            // Verify the request branch is paused
            if (!requestedEdge.blockage) {
                stompFailure(`${requestedEdge.display_name} not currently blocked, no need to skip.`)
                $('#result').append(renderSingleBranchTable(requestedNode))
                return
            }


            // Ensure the current pause is applicable to the CL
            if (requestedEdge.blockage.change !== requestedBranchCl) {
                displayWarningMessage(`${requestedEdge.display_name} currently blocked, but not at requested CL ${requestedBranchCl}. Performing no action.`, false)
                $('#singleBranchDisplay').append(renderSingleBranchTable(requestedNode)).fadeIn("fast", "swing")
                return
            }

            // Passed all safety checks. Display branch info and stomp warning, and begin verifying the request
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
            let stompQueryData = {
                cl: requestedEdge.blockage.change,
                target: targetBranchName
            }

            let verifyOperation = nodeAPIOp(requestedNode.bot, requestedNode.def.name, "/verifystomp?" + toQuery(stompQueryData))

            // If verification succeeded:
            verifyOperation.done(function(success) {
                // Stomp returns a JSON payload on success
                const stompJson = JSON.parse(success)
                console.log(`Stomp Verification message: ${stompJson.message}`)

                // Visualize the json
                visualizeStompVerification(requestedBranchCl, stompJson)

                if (stompJson.validRequest) {
                    $('#afterVerificationResultText').html('<span><i class="fas fa-check-circle"></i></span> Stomp Verification Success!')
                    $('#afterVerificationResultText').css('color', 'green');
                } else {
                    $('#afterVerificationResultText').html('<span><i class="fas fa-exclamation-triangle"></i></span> Stomp Verification Returned Issues.')
                    $('#afterVerificationResultText').css('color', 'red');
                }

                transitionPostVerification()

                // If we're a valid request, add form buttons to formally request a stomp
                if (stompJson.validRequest) {
                    const formButtonDiv = $('#formButtons')

                    formButtonDiv.append(
                        $('<h3 style="text-align: center">').text("Use the button below to proceed with the stomp operation:")
                    )
                    // Return to Robomerge homepage
                    let cancelButton = $('<button type="button" class="btn btn-lg btn-info" style="margin:.3em;">').text(`Cancel`).appendTo(formButtonDiv)
                    cancelButton.click(function() {
                        goHome()
                    })

                    // Perform stomp
                    let stompButton = $('<button type="button" class="btn btn-lg btn-primary" style="margin:.3em;">').text(`Perform Stomp`).appendTo(formButtonDiv)
                    stompButton.click(function() {
                        stompButton.attr('disabled', true)
                        stompButton.text(`Requesting stomp with CL ${requestedBranchCl}...`)
                        transitionStompChanges(requestedBranchCl)

                        nodeAPIOp(requestedNode.bot, requestedNode.def.name, "/stompchanges?" + toQuery(stompQueryData))
                            .done(function (success) {
                                stompOperation.resolve(success)
                            })
                            .fail(function(jqXHR, textStatus, errMsg) {
                                stompOperation.reject(jqXHR, textStatus, errMsg)
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
                stompFailure(`Encountered issues verifying CL ${requestedEdge.blockage.change} as a candidate to stomp changes in ${targetBranchName}: "${errText}" If you think this is an error, please contact Robomerge support.`)
                
                // Call getBranchList a second time to display most up-to-date data
                getBranch(requestedNode.bot, requestedNode.def.name, function(data) {
                    const upToDateEdgeData = data.edges && data.edges[targetBranchName.toUpperCase()] ? data.edges[targetBranchName.toUpperCase()] : null
                    $('#afterVerification').append(renderSingleBranchTable(data.branch, upToDateEdgeData))
                })
            })
        }
        catch (err) {
            stompFailure(`Error encountered displaying stomp verification results: ${err}`)
        }
    })

    // Logic controlling stomp operation
    $.when(stompOperation).done(function(success) {
        try {
            transitionDisplayStompResults()
            stompSuccess(success)
        }
        catch (err) {
            stompFailure(`Error encountered displaying stomp changes results: ${err}`)
        }
        
    }).fail(function(jqXHR, textStatus, errMsg) {
        try {
            transitionDisplayStompResults()
            stompFailure(`${textStatus}: ${errMsg}`)
        }
        catch (err) {
            stompFailure(`Error encountered displaying stomp changes failure: ${err}`)
        }
    })
}

function transitionPostVerification() {
    // Remove before verify animation, fade in new visualization
    $('#beforeVerification').fadeOut("fast", "swing", function() { $('#afterVerification').fadeIn("fast") })
    $('#loadingDiv').fadeOut("fast", "swing")
}

function transitionStompChanges(changelist) {
    $('#loadingText').text(`Performing stomp with CL ${changelist}...`)
    $('#afterVerification').fadeOut("fast", "swing", function() { $('#loadingDiv').fadeIn("fast") })
}

function transitionDisplayStompResults() {
    $('#loadingDiv').fadeOut("fast", "swing", function() { $('#afterStomp').fadeIn("fast") })
    $('#returnbutton').removeClass("initiallyHidden")
}

// Create visualization of the changelist, with links to swarm, a display of the description and a list of relevant files to this request
function visualizeChangelist(changelist, changelistData) {
    const changelistDiv = $(`<div id="${changelist}" class="stomp-visual">`)
    if (changelistData.author.toLowerCase() === robomergeUser.userName.toLowerCase()) {
        changelistDiv.append($('<h3>').html(`${makeClLink(changelist)} by <strong>you</strong>`))
    } else {
        changelistDiv.append($('<h3>').html(`${makeClLink(changelist)} by ${changelistData.author}`))
    }
    changelistDiv.append($('<hr style="margin-top:0em; border-width: 2px;">'))

    changelistDiv.append($('<pre>').text(changelistData.description))
    
    // Begin compiling the list of relevant files in this changelist
    let fileList = $('<ul>')
    let unresolvedFiles = false
    for (const fileObject of changelistData.filesStomped.sort()) {
        let itemHtml = fileObject.filename

        // If the file isn't resolved, it will be stomped (or in the case of text files, block the stomp)
        if (!fileObject.resolved) {
            itemHtml = `<strong>${fileObject.filename}</strong>`
            unresolvedFiles = true
        }
        fileList.append($('<li>').html(itemHtml))
    }

    // Append list
    if (unresolvedFiles) {
        changelistDiv.append($('<h5>').html('Relevant files in changelist (unresolved files in <strong>bold</strong>):'))
    } else {
        changelistDiv.append($('<h5>').html('Relevant files in changelist:'))
    }
    fileList.appendTo(changelistDiv)

    return changelistDiv
}

function visualizeStompVerification(requestedBranchCl, stompJson) {
    // Debug
    const prettyJson = JSON.stringify(stompJson, null, 2)
    console.log(`Stomp Verification Debug JSON:\n${prettyJson}`)
    let linkToSwarm = makeClLink(requestedBranchCl, `CL ${requestedBranchCl}`)

    // For visualization, sort data by stomped revision
    // (JSON data is sorted by file)
    let clDict = {}
    let encounteredStompedRevisionsCalculationIssues = false
    for (const file of stompJson.files) {
        const linkToSwarm = makeSwarmFileLink(file.targetFileName)

        // Process resolved files
        if (file.resolved) {
            // Add text files to the warning list
            if (file.filetype.startsWith('text')) {
                $('#mixedMergeWarningList').append(
                    $('<li>').html(linkToSwarm)
                )
            }
        } 
        // Add unresolved text files to the error list
        else if (file.filetype.startsWith('text')) {
            $('#unresolvedTextFilesList').append(
                $('<li>').html(linkToSwarm)
            )
        }

        if (file.branchOrDeleteResolveRequired) {
            $('#branchOrDeleteResolveRequiredList').append(
                $('<li>').html(linkToSwarm)
            )
        }

        if (file.stompedRevisionsSkipped) {
            $('#stompedRevisionsSkippedDiv').removeClass('initiallyHidden')
            $('#stompedRevisionsSkippedList').append(
                $('<li>').html(linkToSwarm)
            )
            // If this file skipped stomped revisions, don't bother attempting to add to clDict
            continue
        }

        if (file.stompedRevisionsCalculationIssues) {
            encounteredStompedRevisionsCalculationIssues = true
            $('#stompedRevisionsIssuesList').append(
                $('<li>').html(linkToSwarm)
            )
            // If this file couldn't calculate stomped revisions, don't bother attempting to add to clDict
            continue
        } 
        
        const fileObj = {
            filename: file.targetFileName,
            resolved: file.resolved
        }

        // Process unresolved files
        for (const stompedRev of file.stompedRevisions) {
            // If we don't track this changelist, add a new entry to the dictionary
            if (!clDict[stompedRev.changelist]) {
                clDict[stompedRev.changelist] = {
                    author: stompedRev.author,
                    description: stompedRev.description,
                    filesStomped: [ fileObj ]
                }
            }
            // Otherwise, add another file stomped to the existing entry
            else {
                clDict[stompedRev.changelist].filesStomped.push(fileObj)
            }
        }
    }

    if (stompJson.branchOrDeleteResolveRequired) {
        $('#verificationBranchOrDeleteResolveRequired').removeClass('initiallyHidden')
        $('#resultVisualization').append(
            $('<h5 class="centered-text">').html(`Robomerge cannot proceed with the stomp request using ${linkToSwarm}.`)
        )
        return
    }

    if (encounteredStompedRevisionsCalculationIssues) {
        $('#stompedRevisionsIssuesDiv').removeClass('initiallyHidden')
    }

    // Display error if non-binary files remain after the merge
    if (!stompJson.remainingAllBinary) {
        $('#verificationNotAllBinary').removeClass('initiallyHidden')
        $('#resultVisualization').append(
            $('<h5 class="centered-text">').html(`Robomerge cannot proceed with the stomp request using ${linkToSwarm}. The following changelists contain unstompable changes due to the presence of text files:`)
        )
    } 

    if (stompJson.validRequest) {
        if (stompJson.nonBinaryFilesResolved) {
            // Enable warning if we're doing a mixed merge of binary and non-binary files
            $('#mixedMergeWarning').removeClass('initiallyHidden')
        }

        // Display valid request
        $('#resultVisualization').append(
            $('<h5 class="centered-text">').html(
                `The following changelists will be affected after stomping changes with files from ${linkToSwarm}:`
            )
        )
    }

    // Now display each changelist
    const sortedKeys = Object.keys(clDict).sort()
    sortedKeys.forEach(function(key) {
        $('#resultVisualization').append(visualizeChangelist(key, clDict[key]))
    })
}

function stompFailure(message) {
    displayErrorMessage(message, false)
    $('#loadingDiv').fadeOut("fast", "swing")
    $('#returnbutton').removeClass("initiallyHidden")
}

function stompSuccess(message) {
    displaySuccessfulMessage(message, false)
    $('#loadingDiv').fadeOut("fast", "swing")
    $('#returnbutton').removeClass("initiallyHidden")
}