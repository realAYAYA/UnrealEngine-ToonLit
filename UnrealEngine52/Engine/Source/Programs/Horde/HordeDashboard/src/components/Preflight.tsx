// Copyright Epic Games, Inc. All Rights Reserved.

import React, { useState } from 'react';
import { useNavigate, Navigate } from 'react-router-dom';
import backend, { useBackend } from '../backend';
import { useQuery } from './JobDetailCommon';

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
      console.error("No preflight change specified");
      return <Navigate to="/" replace={true} />;
   }

   const cl = parseInt(change);

   if (isNaN(cl)) {
      console.error(`Bad change in preflight ${change}`);
      return <Navigate to="/" replace={true} />;
   }

   if (!streamName) {
      console.error("No stream in query");
      return <Navigate to="/" replace={true} />;
   }

   let stream = projectStore.streamByFullname(streamName);


   if (!stream) {
      console.error(`Unable to resolve stream with name ${streamName}`);
      return <Navigate to="/" replace={true} />;      
   }

   const project = stream?.project;

   if (!stream || !project) {
      console.error("Bad stream or project id in StreamView");
      return <Navigate to="/" replace={true} />;      
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
