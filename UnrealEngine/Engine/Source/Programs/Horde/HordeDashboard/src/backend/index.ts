// Copyright Epic Games, Inc. All Rights Reserved.

import React from 'react';
import { Backend } from './Backend';
import { projectStore } from './ProjectStore';
import { issueStore } from './IssueStore';

const backend = new Backend();
export default backend;

const backendContext = React.createContext({
  projectStore: projectStore,
  issueStore: issueStore  
});

export const useBackend = () => React.useContext(backendContext);
