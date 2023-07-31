// Copyright Epic Games, Inc. All Rights Reserved.

import React from 'react';
import { mergeStyleSets, getTheme } from '@fluentui/react/lib/Styling';
import { Stack, Text } from '@fluentui/react';
import { Link } from 'react-router-dom';
import { hordeClasses } from '../styles/Styles';
import { TestData } from '../backend/Api';
import hordePlugins, { PluginMount } from  '../Plugins';
import { ComponentMount } from './TestReportView';

const theme = getTheme();

const styles = mergeStyleSets({
    item: {
        padding: 8,
        borderBottom: '1px solid '+theme.palette.neutralLighter,
        selectors: {
            ':hover': {background: theme.palette.neutralLight}
        }
    }
});

type ComponentItem = {
    hasComponent: boolean;
    linkComponent?: React.FC<any>;
}

type TestDataItem = ComponentItem & {
    name: string;
    item: TestData;
}

const testReportComponentTypes : Map<string, ComponentItem> = new Map();

export const TestReportPanel: React.FC<{ testdata: TestData[] }> = ({ testdata }) => {

    const testdataItems : TestDataItem[] = [];
    testdata.forEach((test) => {
        let components : ComponentItem = {hasComponent: false};
        const splitKey = test.key.split('::', 2);
        const type = splitKey[0];
        const name = splitKey.length > 1 ? splitKey[1] : type;
        if (testReportComponentTypes.has(type)) {
            components = testReportComponentTypes.get(type)!;
        }
        else
        {
            let pluginComponents = hordePlugins.getComponents(PluginMount.TestReportPanel, type);
            components.hasComponent = pluginComponents.length > 0
            testReportComponentTypes.set(type, components);
            if (components.hasComponent) {
                pluginComponents = hordePlugins.getComponents(PluginMount.TestReportLink, type);
                if (pluginComponents.length > 0) {
                    components.linkComponent = pluginComponents[0].component;
                }
            }
        }
        components.hasComponent && testdataItems.push({...components, item: test, name: name})
    });

    return (<Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
        <Stack className={hordeClasses.raised}>
            <Stack tokens={{ childrenGap: 12 }}>
                <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Test Report</Text>
                <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
                    {testdataItems.map((test) => <Stack key={test.item.id} className={styles.item} horizontal wrap tokens={{ childrenGap: 30 }}>
                        <Link className={"view-log-link"} to={`/testreport/${test.item.id}`}>{test.name}</Link>
                        <ComponentMount component={test.linkComponent} id={test.item.id} fullName={test.item.key}/>
                    </Stack>)}
                </Stack>
            </Stack>
        </Stack>
    </Stack>);
};

export const BuildHealthTestReportPanel: React.FC<{ streamId: string }> = ({ streamId }) => {
    const items: ComponentItem[] = hordePlugins.getComponents(PluginMount.BuildHealthSummary).map((item) => {
        return {hasComponent: true, linkComponent: item.component};
    });

    return (
        <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }} horizontal wrap tokens={{ childrenGap: 30 }}>
            {items.map((item, index) =>
                    <Stack key={`BuildHealthPlugin${index}`}><ComponentMount component={item.linkComponent} streamId={streamId}/></Stack>
                )
            }
        </Stack>
    );
};

