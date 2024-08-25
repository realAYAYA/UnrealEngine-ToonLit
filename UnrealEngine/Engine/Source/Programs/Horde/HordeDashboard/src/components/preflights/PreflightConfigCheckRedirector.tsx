// Copyright Epic Games, Inc. All Rights Reserved.

import React from 'react';
import { Navigate } from 'react-router-dom';
import { useQuery } from '../JobDetailCommon';
import ErrorHandler from '../ErrorHandler';

function setError(message: string) {

   console.error(message);

   message = `${message}\n\nReferring URL: ${encodeURI(window.location.href)}`

   ErrorHandler.set({

      title: `Error handling Preflight Configuration`,
      message: message

   }, true);

}

// redirect from external source, where horde stream id, etc are not known by that application
export const PreflightConfigRedirector: React.FC = () => {

   const query = useQuery();

   const shelvedCL = !query.get("shelvedchange") ? "" : query.get("shelvedchange")!;
   
   if (!shelvedCL) { 
      setError("No shelved change specified");
   } else {
      return <Navigate to={`/index?preflightconfig=${shelvedCL}`} replace={true} />
   }

   return null;

}