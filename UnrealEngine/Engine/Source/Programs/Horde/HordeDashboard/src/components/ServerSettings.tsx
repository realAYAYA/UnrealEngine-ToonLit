import { Checkbox, DefaultButton, FocusZone, FocusZoneDirection, IconButton, Label, mergeStyleSets, MessageBar, MessageBarType, Modal, Pivot, PivotItem, PrimaryButton, ScrollablePane, ScrollbarVisibility, Spinner, SpinnerSize, Stack, Text, TextField } from "@fluentui/react";
import { useState } from "react";
import backend from "../backend";
import { GetServerSettingsResponse, ServerUpdateResponse } from "../backend/Api";
import dashboard from "../backend/Dashboard";
import { useWindowSize } from "../base/utilities/hooks";
import { Breadcrumbs } from "./Breadcrumbs";
import { TopNav } from "./TopNav";
import { getHordeStyling } from "../styles/Styles";

const settingStyles = mergeStyleSets({

    settings: {
        selectors: {
            ".ms-TextField-fieldGroup": {
                width: 580
            },
        }
    }
})

enum SettingType {
    Boolean,
    String,
    Number
}

type Setting = {
    type: SettingType;
    property: string;
    label: string;
    desc?: string;
    readOnly?: boolean;

    // uses a password field to hide value
    hideValue?: boolean;
}

type Section = {

    title?: string;
    settings: Setting[];

}

type ServerPanel = {

    title: string;
    sections: Section[];

}

const UpdateErrorModal: React.FC<{ errors: string[], onClose: () => void }> = ({ errors, onClose }) => {

   const { hordeClasses } = getHordeStyling();

    const close = () => {
        onClose();
    };

    const messages = errors.map((e, idx) => {
        return <MessageBar key={`validation_error_${idx}`} messageBarType={MessageBarType.error} isMultiline={true}>
            <Stack>
                <Stack grow tokens={{ childrenGap: 12 }}>
                    <Stack tokens={{ childrenGap: 12 }} horizontal>
                        <Text nowrap style={{ fontFamily: "Horde Open Sans SemiBold", width: 80 }}>Error:</Text>
                        <Text>{e}</Text>
                    </Stack>
                </Stack>

            </Stack>
        </MessageBar>
    })

    return <Modal isOpen={true} className={hordeClasses.modal} styles={{ main: { padding: 8, width: 900 } }} onDismiss={() => { close() }}>
        <Stack horizontal styles={{ root: { padding: 8 } }}>
            <Stack style={{ paddingLeft: 8, paddingTop: 4 }} grow>
                <Text variant="mediumPlus">Server Update Errors</Text>
            </Stack>
            <Stack grow horizontalAlign="end">
                <IconButton
                    iconProps={{ iconName: 'Cancel' }}
                    ariaLabel="Close popup modal"
                    onClick={() => { close(); }}
                />
            </Stack>
        </Stack>

        <Stack style={{ paddingLeft: 20, paddingTop: 8, paddingBottom: 8 }}>
            <Text style={{ fontSize: 15 }}>Please correct the following errors and try again.</Text>
        </Stack>

        <Stack tokens={{ childrenGap: 8 }} styles={{ root: { paddingLeft: 20, paddingTop: 18, paddingBottom: 24, width: 860 } }}>
            {messages}
        </Stack>


        <Stack horizontal styles={{ root: { padding: 8, paddingTop: 8 } }}>
            <Stack grow />
            <Stack horizontal tokens={{ childrenGap: 16 }} styles={{ root: { paddingTop: 12, paddingLeft: 8, paddingBottom: 8 } }}>
                <PrimaryButton text="Ok" disabled={false} onClick={() => { close(); }} />
            </Stack>
        </Stack>
    </Modal>;

};

export const ServerSettingsView: React.FC = () => {

    const [settings, setSettings] = useState<GetServerSettingsResponse | undefined>(undefined);
    const [state, setState] = useState<{ selectedPanelKey: string, modified: Map<string, string | boolean | number>, submitting?: boolean, errors?: string[] }>({ selectedPanelKey: `key_server`, modified: new Map() })
   const windowSize = useWindowSize();
   
   const { hordeClasses, detailClasses, modeColors } = getHordeStyling();

    const editDisabled = true;

    if (!settings) {
        backend.getServerSettings().then((settings) => {
            if (settings.numServerUpdates) {
                dashboard.setServerSettingsChanged(true);
            }
            setSettings(settings);
        })
        return null;
    }

    const SettingInput: React.FC<{ setting: Setting }> = ({ setting }) => {

        let value: any = state.modified.get(setting.property);

        if (value === undefined) {
            value = (settings as any)[setting.property];
        }

        if (setting.type === SettingType.String) {
            return <TextField label={setting.label} description={setting.desc} defaultValue={value} disabled={setting.readOnly || editDisabled} type={setting.hideValue ? "password" : undefined} canRevealPassword={setting.hideValue} onChange={(ev, newValue) => { state.modified.set(setting.property, newValue ? newValue : "") }} />
        }

        if (setting.type === SettingType.Number) {
            return <TextField label={setting.label} description={setting.desc} defaultValue={value} type="number" disabled={setting.readOnly || editDisabled} onChange={(ev, newValue) => { state.modified.set(setting.property, newValue ? parseFloat(newValue) : 0) }} />
        }

        if (setting.type === SettingType.Boolean) {
            return <Stack style={{ paddingTop: 4 }}>
                <Checkbox label={setting.label} disabled={setting.readOnly || editDisabled} defaultChecked={value} onChange={(ev, checked) => { state.modified.set(setting.property, checked ? true : false) }} />
            </Stack>
        }

        return <Stack>Unknown setting type for: {setting.property}</Stack>
    }


    const SettingsSection: React.FC<{ section: Section }> = ({ section }) => {

        const settings = section.settings.map(setting => {
            return <SettingInput key={`server_setting_${setting.property}`} setting={setting} />
        })

        if (section.title) {
            return <Stack style={{ paddingTop: 4 }}>
                <Stack style={{ paddingBottom: 4 }}>
                    <Label style={{ fontSize: 14 }}>{section.title}</Label>
                </Stack>
                <Stack style={{ paddingLeft: 18 }} tokens={{ childrenGap: 12 }}>{settings}</Stack>
            </Stack>
        }

        return <Stack tokens={{ childrenGap: 12 }}>{settings}</Stack>

    }

    const SettingsPanel: React.FC<{ panel: ServerPanel }> = ({ panel }) => {

        let count = 0;

        const sections = panel.sections.map(section => {
            return <SettingsSection key={`server_section_${panel.title}_${section.title ?? count++}`} section={section} />
        });


        return (<Stack style={{ marginLeft: 4 }}>
            <Stack className={hordeClasses.raised}>
                <Stack tokens={{ childrenGap: 12 }}>
                    <Stack>
                        <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{panel.title}</Text>
                    </Stack>

                    <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
                        <Stack tokens={{ childrenGap: 8 }} className={settingStyles.settings}>
                            {sections}
                        </Stack>
                    </Stack>
                </Stack>
            </Stack>
        </Stack>);
    };

    const panelOrder = ["Server", "Database", "Storage", "Perforce", "Authentication", "Logging", "Notifications"];

    const currentConfig = serverConfigPanels.find(p => `key_${p.title.toLowerCase()}` === state.selectedPanelKey)!;

    const pivotItems = panelOrder.map(title => {
        return <PivotItem headerText={title} itemKey={`key_${title.toLowerCase()}`} key={`key_${title.toLowerCase()}`} />;
    })

    // console.log(JSON.stringify(Array.from(state.modified.entries())))

    const ServerSettingsViewInner: React.FC = () => {

        return <Stack className={hordeClasses.modal} tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: "#fffffff", margin: 0, paddingTop: 8 } }}>
            {!!state.errors?.length && <UpdateErrorModal errors={state.errors!} onClose={() => setState({ ...state, errors: undefined })} />}
            <Stack style={{ padding: 0 }}>
                <Stack horizontal>
                    <Stack>
                        <Pivot className={hordeClasses.pivot}
                            selectedKey={state.selectedPanelKey}
                            linkSize="normal"
                            linkFormat="links"
                            onLinkClick={(item) => {
                                setState({ ...state, selectedPanelKey: item!.props.itemKey! })
                            }}>
                            {pivotItems}
                        </Pivot>
                    </Stack>
                    <Stack grow />
                    <Stack horizontal tokens={{ childrenGap: 12 }}>
                        <DefaultButton text={editDisabled ? "Refresh" : "Revert"} onClick={() => { setState({ ...state, modified: new Map() }) }} />
                        <PrimaryButton disabled={editDisabled} text="Save" onClick={async () => {

                            if (!state.modified.size) {
                                return;
                            }

                            setState({ ...state, submitting: true });

                            console.log(JSON.stringify(Array.from(state.modified.entries())));

                            const update: Record<string, string | boolean | number> = {};
                            state.modified.forEach((value, key) => {
                                update[key] = value;
                            })

                            let updateResponse: ServerUpdateResponse | undefined;

                            try {
                                updateResponse = await backend.updateServerSettings({ settings: update });
                            } catch (error) {
                                console.error(error);
                            }

                            if (updateResponse && !updateResponse.errors?.length) {
                                try {
                                    const newSettings = await backend.getServerSettings();
                                    if (newSettings.numServerUpdates) {
                                        dashboard.setServerSettingsChanged(true);
                                    }
                                    setSettings(newSettings);
                                } catch (error) {
                                    console.error(error);
                                }
                            }

                            if (updateResponse?.errors?.length) {
                                setState({ ...state, submitting: false, errors: updateResponse.errors });
                            } else {
                                setState({ ...state, modified: new Map(), submitting: false });
                            }



                        }} />
                    </Stack>
                </Stack>
                <FocusZone direction={FocusZoneDirection.vertical} style={{ padding: 0 }}>
                    <div className={detailClasses.container} style={{ width: "100%", height: 'calc(100vh - 208px)', position: 'relative' }} data-is-scrollable={true}>
                        <ScrollablePane scrollbarVisibility={ScrollbarVisibility.auto} onScroll={() => { }}>
                            <Stack style={{ padding: 0 }}>
                                <SettingsPanel panel={currentConfig} />
                            </Stack>
                        </ScrollablePane>
                    </div>
                </FocusZone>
            </Stack>
        </Stack>
    };


    const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

    return (
        <Stack className={hordeClasses.horde}>
            <TopNav />
            <Breadcrumbs items={[{ text: 'Server Settings' }]} suppressHome={true} />
            {state.submitting && <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 700, hasBeenOpened: false, top: "80px", position: "absolute" } }} className={hordeClasses.modal}>
                <Stack tokens={{ childrenGap: 24 }} styles={{ root: { padding: 8 } }}>
                    <Stack grow verticalAlign="center">
                        <Text variant="mediumPlus" styles={{ root: { fontWeight: "unset", fontFamily: "Horde Open Sans SemiBold" } }}>Saving Server Settings</Text>
                    </Stack>
                    <Stack horizontalAlign="center">
                        <Text variant="mediumPlus">Please wait while the settings are saved.</Text>
                    </Stack>
                    <Stack verticalAlign="center" style={{ paddingBottom: 32 }}>
                        <Spinner size={SpinnerSize.large} />
                    </Stack>
                </Stack>
            </Modal>}

            <Stack horizontal>
                <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 720, flexShrink: 0, backgroundColor: 'rgb(250, 249, 249)' }} />
                <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: 'rgb(250, 249, 249)', width: "100%" } }}>
                    <Stack style={{ maxWidth: 1420, paddingTop: 6, marginLeft: 4 }}>
                        <ServerSettingsViewInner />
                    </Stack>
                </Stack>
            </Stack>
        </Stack>);

}

const serverConfigPanels: ServerPanel[] = [
    {
        title: "Database",
        sections: [{
            settings: [
                {
                    type: SettingType.String,
                    property: "databaseConnectionString",
                    label: "Database Connection String",
                    desc: "MongoDB connection string",
                    hideValue: true
                },
                {
                    type: SettingType.String,
                    property: "databaseName",
                    label: "Database Name",
                    desc: "MongoDB database name",
                    readOnly: true
                },
                {
                    type: SettingType.String,
                    property: "databasePublicCert",
                    label: "Database Public Certificate",
                    desc: "Optional certificate to trust in order to access the database (eg. AWS cert for TLS)",
                    readOnly: true
                },
                {
                    type: SettingType.Boolean,
                    property: "databaseReadOnlyMode",
                    label: "Read Only Mode",
                    desc: "Access the database in read-only mode (avoids creating indices or updating content).  Useful for debugging a local instance of HordeServer against a production database"
                }

            ]
        }]
    },
    {
        title: "Server",
        sections: [
            {                
                settings: [{
                    type: SettingType.String,
                    property: "globalConfigPath",
                    label: "Global Configuration",
                    desc: "Path to the global configuration",
                    readOnly: true
                },
                {
                    type: SettingType.String,
                    property: "userServerSettingsPath",
                    label: "Server Settings",
                    desc: "Path to server settings",
                    readOnly: true
                }]
            }, {
                title: "General",
                settings: [
                    {
                        type: SettingType.Boolean,
                        property: "disableSchedules",
                        label: "Disable Schedules",
                        desc: "Whether to run scheduled jobs."
                    },
                    {
                        type: SettingType.Boolean,
                        property: "corsEnabled",
                        label: "Enable CORS",
                        desc: "Whether to enable Cors, generally for development purposes"
                    },
                    {
                        type: SettingType.String,
                        property: "corsOrigin",
                        label: "CORS Origin",
                        desc: "Allowed Cors origin",
                    }
                ]
            },
        ]
    },
    {
        title: "Perforce",
        sections: [{
            title: "Service",
            settings: [
                {
                    type: SettingType.String,
                    property: "p4BridgeServiceUsername",
                    label: "Service Account Username",
                    desc: "The username of the service account",
                },
                {
                    type: SettingType.String,
                    property: "p4BridgeServicePassword",
                    label: "Service Account Password",
                    desc: "The password of the service account",
                    hideValue: true
                },
                {
                    type: SettingType.Boolean,
                    property: "p4BridgeCanImpersonate",
                    label: "User Impersonation",
                    desc: "Whether the service account supports user impersonation"
                },

            ]
        }]
    },
    {
        title: "Authentication",
        sections: [{
            title: "General",
            settings: [
                {
                    type: SettingType.Boolean,
                    property: "disableAuth",
                    label: "Disable Auth",
                    desc: "Disable authentication for debugging purposes"
                },
                {
                    type: SettingType.String,
                    property: "adminClaimType",
                    label: "Admin Claim Type",
                    desc: "The claim type for administrators",
                    readOnly: true
                },
                {
                    type: SettingType.String,
                    property: "adminClaimValue",
                    label: "Admin Claim Value",
                    desc: "Value of the claim type for administrators",
                    readOnly: true
                },
                {
                    type: SettingType.String,
                    property: "serverPrivateCert",
                    label: "Server Private Certificate",
                    desc: "Optional PFX certificate to use for encryting agent SSL traffic. This can be a self-signed certificate, as long as it's trusted by agents"
                },

            ]
        },
        {
            title: "JWT",
            settings: [
                {
                    type: SettingType.String,
                    property: "jwtIssuer",
                    label: "Issuer",
                    desc: "Name of the issuer in bearer tokens from the server",
                },
                {
                    type: SettingType.String,
                    property: "jwtSecret",
                    label: "Secret",
                    desc: "Secret key used to sign JWTs. This setting is typically only used for development. In prod, a unique secret key will be generated and stored in the DB for each unique server instance",
                    hideValue: true
                },
                {
                    type: SettingType.Number,
                    property: "jwtExpiryTimeHours",
                    label: "Token Expiration (Hours)",
                    desc: "Length of time before JWT tokens expire, in hourse"
                },

            ]
        },
        {
            title: "OIDC",
            settings: [
                {
                    type: SettingType.String,
                    property: "oidcAuthority",
                    label: "OIDC Issuer",
                    desc: "Issuer for tokens from the auth provider",
                },
                {
                    type: SettingType.String,
                    property: "oidcClientId",
                    label: "Client Id",
                    desc: "Client id for the OIDC authority",
                    hideValue: true
                },
                {
                    type: SettingType.String,
                    property: "oidcSigninRedirect",
                    label: "Login Redirect Url",
                    desc: "Optional redirect url provided to OIDC login"
                },

            ]
        },
        ]
    },
    {
        title: "Logging",
        sections: [{
            title: "General",
            settings: [
                {
                    type: SettingType.String,
                    property: "logServiceWriteCacheType",
                    label: "Log Service Write Cache",
                    desc: "Type of write cache to use in log service, currently Supported: \"InMemory\" or \"Redis\"",
                },
                {
                    type: SettingType.String,
                    property: "redisConnectionConfig",
                    label: "Redis Connection String",
                    desc: "Config for connecting to Redis server(s)",
                },
                {
                    type: SettingType.String,
                    property: "externalStorageProviderType",
                    label: "External Storage Provider",
                    desc: "Provider Type, currently Supported: \"S3\" or \"FileSystem\""
                },
                {
                    type: SettingType.Boolean,
                    property: "logJsonToStdOut",
                    label: "Log JSON to StdOut",
                    desc: "Whether to log json to stdout"
                }

            ]
        }]
    },
    {
        title: "Notifications",
        sections: [{
            title: "Email",
            settings: [
                {
                    type: SettingType.String,
                    property: "smtpServer",
                    label: "SMTP Server",
                    desc: "URI to the SmtpServer to use for sending email notifications",
                },
                {
                    type: SettingType.String,
                    property: "emailSenderAddress",
                    label: "Email Address",
                    desc: "The email address to send email notifications from",
                },
                {
                    type: SettingType.String,
                    property: "emailSenderName",
                    label: "Sender Name",
                    desc: "The name for the sender when sending email notifications"
                }
            ]
        },
        {
            title: "Slack",
            settings: [
                {
                    type: SettingType.String,
                    property: "slackToken",
                    label: "Slack Token",
                    desc: "Token for interacting with Slack",
                    hideValue: true
                },
                {
                    type: SettingType.String,
                    property: "slackSocketToken",
                    label: "Socket Token",
                    desc: "Token for opening a socket to slack ",
                    hideValue: true
                },
                {
                    type: SettingType.String,
                    property: "updateStreamsNotificationChannel",
                    label: "Stream Notification Channel",
                    desc: "Channel to send stream notification update failures to"
                }
            ]
        }]
    },
    {
        title: "Storage",
        sections: [{
            title: "Local",
            settings: [
                {
                    type: SettingType.String,
                    property: "localLogsDir",
                    label: "Log Storage",
                    desc: "Local log/artifact storage directory, if using type filesystem"
                },
                {
                    type: SettingType.String,
                    property: "localBlobsDir",
                    label: "Block Storage",
                    desc: "Local blob storage directory, if using type filesystem",
                },
                {
                    type: SettingType.String,
                    property: "localArtifactsDir",
                    label: "Artifact Storage",
                    desc: "Local artifact storage directory, if using type filesystem",
                }
            ]
        },
        {
            title: "S3",
            settings: [
                {
                    type: SettingType.String,
                    property: "s3BucketRegion",
                    label: "Bucket Region",
                    desc: "S3 bucket region for logfile storage",
                },
                {
                    type: SettingType.String,
                    property: "s3CredentialType",
                    label: "Credential Type",
                    desc: "Arn to assume for s3.  \"Basic\", \"AssumeRole\", \"AssumeRoleWebIdentity\" only"
                },
                {
                    type: SettingType.String,
                    property: "s3LogBucketName",
                    label: "Log Bucket",
                    desc: "S3 log bucket name"

                },
                {
                    type: SettingType.String,
                    property: "s3ArtifactBucketName",
                    label: "Artifact Bucket",
                    desc: "S3 artifact bucket name"

                },
                {
                    type: SettingType.String,
                    property: "s3ArtifactBucketName",
                    label: "Artifact Bucket",
                    desc: "S3 artifact bucket name"

                },
                {
                    type: SettingType.String,
                    property: "s3AssumeArn",
                    label: "Arn",
                    desc: "Arn to assume for s3"

                }
            ]
        }]
    },
];
