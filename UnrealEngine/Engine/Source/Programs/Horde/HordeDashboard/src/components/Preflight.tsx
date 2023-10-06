// Copyright Epic Games, Inc. All Rights Reserved.

import React, { useState } from 'react';
import { useNavigate, Navigate } from 'react-router-dom';
import backend, { useBackend } from '../backend';
import { useQuery } from './JobDetailCommon';
import ErrorHandler from './ErrorHandler';

function setError(message: string) {

   console.error(message);

   message = `${message}\n\nReferring URL: ${encodeURI(window.location.href)}`

   ErrorHandler.set({

      title: `Error handling Preflight`,
      message: message

   }, true);

}

// redirect from external source, where horde stream id, etc are not known by that application
export const PreflightRedirector: React.FC = () => {


   const [state, setState] = useState({ preflightQueried: false })

   const navigate = useNavigate();
   const query = useQuery();
   const { projectStore } = useBackend();

   const streamName = !query.get("stream") ? "" : query.get("stream")!;
   const change = !query.get("change") ? "" : query.get("change")!;

   // whether to autosubmit
   const autosubmit = !query.get("submit") ? "" : query.get("submit")!;

   if (!change) {
      setError("No preflight change specified");
      return null;
   }

   const cl = parseInt(change);

   if (isNaN(cl)) {
      setError(`Bad change in preflight ${change}`);
      return null;
   }

   if (!streamName) {
      setError("No stream in query");
      return null;
   }

   let stream = projectStore.streamByFullname(streamName);


   if (!stream) {
      setError(`Unable to resolve stream with name ${streamName}`);
      return null;
   }

   const project = stream?.project;

   if (!stream || !project) {
      setError("Bad stream or project id");
      return null;
   }

   if (!state.preflightQueried) {

      backend.getJobs({ filter: "id", count: 1, preflightChange: cl }).then(result => {

         if (result && result.length === 1) {

            let url = `/job/${result[0].id}?newbuild=true&allowtemplatechange=true&shelvedchange=${change}&p4v=true`;

            if (autosubmit === "true") {
               url += "&autosubmit=true";
            }

            navigate(url, { replace: true });
            return;
         }

         let url = `/stream/${stream!.id}?tab=summary&newbuild=true&shelvedchange=${change}&p4v=true`;

         if (autosubmit === "true") {
            url += "&autosubmit=true";
         }

         navigate(url, { replace: true });


      }).catch(reason => {
         console.error(`Error getting job for preflight: `, reason);
         navigate("/", { replace: true });
      })

      setState({ preflightQueried: true })

      return null;

   }

   return null;
}
