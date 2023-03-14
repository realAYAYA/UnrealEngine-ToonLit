// Copyright Epic Games, Inc. All Rights Reserved.

import React, { useState } from 'react';
import { useHistory } from 'react-router-dom';
import backend, { useBackend } from '../backend';
import { useQuery } from './JobDetailCommon';

// redirect from external source, where horde stream id, etc are not known by that application
export const PreflightRedirector: React.FC = () => {


    const [state, setState] = useState({ preflightQueried: false })

    const history = useHistory();
    const query = useQuery();
    const { projectStore } = useBackend();

    const streamName = !query.get("stream") ? "" : query.get("stream")!;
    const change = !query.get("change") ? "" : query.get("change")!;

    // whether to autosubmit
    const autosubmit = !query.get("submit") ? "" : query.get("submit")!;

    if (!change) {
        console.error("No preflight change specified");
        history.replace("/");
        return null;
    }

    const cl = parseInt(change);

    if (isNaN(cl)) {
        console.error(`Bad change in preflight ${change}`);
        history.replace("/");
        return null;
    }

    if (!streamName) {
        console.error("No stream in query");
        history.replace("/");
        return null;
    }

    let stream = projectStore.streamByFullname(streamName);


    if (!stream) {
        console.error(`Unable to resolve stream with name ${streamName}`);
        history.replace("/");
        return null;
    }

    const project = stream?.project;

    if (!stream || !project) {
        console.error("Bad stream or project id in StreamView");
        history.replace("/");
        return null;
    }

    if (!state.preflightQueried) {

        backend.getJobs({ filter: "id", count: 1, preflightChange: cl }).then(result => {

            if (result && result.length === 1) {

                let url = `/job/${result[0].id}?newbuild=true&allowtemplatechange=true&shelvedchange=${change}&p4v=true`;

                if (autosubmit === "true") {
                    url += "&autosubmit=true";
                }

                history.replace(url);
                return;
            }

            let url = `/stream/${stream!.id}?tab=summary&newbuild=true&shelvedchange=${change}&p4v=true`;

            if (autosubmit === "true") {
                url += "&autosubmit=true";
            }

            history.replace(url);


        }).catch(reason => {
            console.error(`Error getting job for preflight: `, reason);
            history.replace("/");
        })

        setState({ preflightQueried: true })

        return null;

    }

    return null;
}
