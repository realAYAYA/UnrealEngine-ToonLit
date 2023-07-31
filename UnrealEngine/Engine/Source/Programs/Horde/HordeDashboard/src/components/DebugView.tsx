// Copyright Epic Games, Inc. All Rights Reserved.

import React from 'react';
import { useHistory, useParams } from 'react-router-dom';
import backend from '../backend';

export const DebugView: React.FC = () => {

    const history = useHistory();
    const params = useParams<{ leaseId?: string }>();

    if (params.leaseId) {

        const getLease = async () => {
            try {
                const lease = await backend.getLease(params.leaseId!);

                if (lease) {

                    history.replace(`/log/${lease.logId}?leaseId=${lease.id}`);

                } else {
                    console.error("Unable to get lease ", params.leaseId);
                }

            } catch (error) {
                console.error(error);
            }
        }

        getLease();
    }

    return null;

}