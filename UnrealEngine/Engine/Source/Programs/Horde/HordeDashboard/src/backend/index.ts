// Copyright Epic Games, Inc. All Rights Reserved.

import React from 'react';
import { Backend } from './Backend';
import { projectStore } from './ProjectStore';
import { issueStore } from './IssueStore';

export default new Backend();

const backendContext = React.createContext({
  projectStore: projectStore,
  issueStore: issueStore  
});

export const useBackend = () => React.useContext(backendContext);
