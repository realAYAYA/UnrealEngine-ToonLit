// Copyright Epic Games, Inc. All Rights Reserved.

import React, { useState } from 'react';
import { Text, Stack, Spinner, SpinnerSize } from '@fluentui/react';
import backend from '../backend';


export const AdminToken: React.FC = () => {

	const [token, setToken] = useState("");
	const [tokenError, setTokenError] = useState("");	

	
	if (!token && !tokenError) {
		backend.getAdminToken().then(token => setToken(token)).catch(reason => setTokenError(reason));
		return <Stack horizontal verticalAlign="center" tokens={{childrenGap:24}} styles={{root:{padding:24}}}>
			<Text variant="mediumPlus">Getting Token...</Text>
			<Spinner size={SpinnerSize.large}/>;
		</Stack>;
		
	}

	return (
		<Stack horizontalAlign="center" styles={{ root: { width: "100%", padding: 12 } }}>
			<Stack styles={{ root: { width: 1440 } }} tokens={{ childrenGap: 18 }}>
				<div >{token ? "Horde Access Token:" : "Error Getting Token:"}</div>
				<div style={{ wordBreak: "break-word" }}> {token ?? tokenError} </div>
			</Stack>
		</Stack>
	);
};