// Copyright Epic Games, Inc. All Rights Reserved.

import { ContextualMenu, IContextualMenuItem } from '@fluentui/react';
import React, { useState } from 'react';
import { GetJobResponse } from '../backend/Api';
import dashboard from '../backend/Dashboard';
import { JobDetails } from '../backend/JobDetails';
import { projectStore } from '../backend/ProjectStore';
// import { AbortJobModal } from './AbortJobModal';

enum ParameterState {
    Hidden,
    Parameters,
    Clone
}

export const JobOperationsContextMenu: React.FC<{ job: GetJobResponse, children?: any }> = ({ job, children }) => {

    const [state, setState] = useState<{ pos?: { x: number, y: number } }>({});
    const [details, setDetails] = useState<JobDetails | undefined>(undefined);


    // const [abortShown, setAbortShown] = useState(false);
    //const [editShown, setEditShown] = useState(false);
    const [, setParametersState] = useState(ParameterState.Hidden);


    const copyToClipboard = (value: string | undefined) => {

        if (!value) {
            return;
        }

        const el = document.createElement('textarea');
        el.value = value;
        document.body.appendChild(el);
        el.select();
        document.execCommand('copy');
        document.body.removeChild(el);
    }

    const pinned = dashboard.jobPinned(job.id);

    let change = job.change!.toString();
    if (job.preflightChange) {
        change = job.preflightChange.toString();
    }

    let menuItems: IContextualMenuItem[] = [
        {
            key: 'open_in_new_window',
            text: 'Open in New Tab',
            onClick: () => window.open(`/job/${job.id}`),
        },
        {
            key: 'pin_job',
            text: pinned ? 'Unpin from Home' : "Pin to Home",
            onClick: () => { pinned ? dashboard.unpinJob(job.id) : dashboard.pinJob(job.id) },
        },
        {
            key: 'copy_to_clipboard',
            text: 'Copy Job to Clipboard',
            onClick: () => copyToClipboard(window.location.protocol + "//" + window.location.hostname + `/job/${job.id}`)
        },
        {
            key: 'copy_cl_to_clipboard',
            text: 'Copy CL to Clipboard',
            onClick: () => copyToClipboard(change),
        },
        {
            key: 'open_in_swarm', text: 'Open CL in Swarm', onClick: () => {
                window.open(`${dashboard.swarmUrl}/changes/${change}`);
            }
        },
        {        
            key: 'open_in_swarm_range', text: 'Open CL Range in Swarm', onClick: () => {

                const stream = projectStore.streamById(job.streamId)!;
                const project = projectStore.byId(stream!.projectId)!;
                const name = project.name === "Engine" ? "UE4" : project.name;
                const rangeUrl = `${dashboard.swarmUrl}/files/${name}/${stream.name}?range=@${change}#commits`;
    
                window.open(rangeUrl)
            }
        }

    ];

    // const abortDisabled = details?.jobdata?.state === JobState.Complete;
    // const editDisabled = details?.jobdata?.state === JobState.Complete;

    if (details) {

        menuItems.push({
            key: 'view_paremeters',
            text: 'View Parameters',
            onClick: () => setParametersState(ParameterState.Parameters),
        });

       /*
        if (!editDisabled) {
            menuItems.push({
                key: 'edit_job',
                text: 'Edit',
                onClick: () => setEditShown(true),
            });
        } */

       /*
        if (!abortDisabled) {
            menuItems.push({
                key: 'abort_job',
                text: 'Abort Job',
                onClick: () => setAbortShown(true),
            });
        } */

        menuItems.push({
            key: 'run_again',
            text: 'Run Again',
            onClick: () => setParametersState(ParameterState.Clone),
        });

    }

    if (state.pos && !details) {
        menuItems = menuItems.slice(0, 6);
        menuItems.push({
            key: 'loading_job',
            text: 'Loading...',
        });
   }
   
   // removed due to JobDetails
   /*
           { !!details && <NewBuild streamId={details.stream!.id} jobDetails={details} readOnly={parametersState === ParameterState.Parameters}
            show={parametersState === ParameterState.Parameters || parametersState === ParameterState.Clone}
            onClose={() => { setParametersState(ParameterState.Hidden); }} />}

             { !abortDisabled && !!details && <AbortJobModal jobDetails={details} show={abortShown} onClose={() => { setAbortShown(false); }} />}
   */
   
   // Also, removed due to job details refactor
   /*
      { !editDisabled && !!details && <EditJobModal jobDetails={details} show={editShown} onClose={() => { setEditShown(false); }} />}
   */

    return (<div onContextMenu={(ev) => {
        ev.preventDefault();

        // chrome nulls out the event properties so capture them
        const pos = { x: ev.clientX, y: ev.clientY };

        if (!details) {
            const details = new JobDetails();

            details.set(job.id, undefined, undefined, undefined, undefined, () => {
                details.stopPolling();

                setDetails(details);

            });
        }

        setState({ pos: pos });

        return false;
    }}>
               
        <ContextualMenu
            styles={{ list: { selectors: { '.ms-ContextualMenu-itemText': { fontSize: "10px" } } } }}
            items={menuItems}
            hidden={(!state.pos)}
            target={state.pos!}
            onItemClick={(ev) => { setState({}); ev?.preventDefault(); }}
            onDismiss={(ev) => { setState({}); ev?.preventDefault(); }} />
        {children}
    </div>)
};