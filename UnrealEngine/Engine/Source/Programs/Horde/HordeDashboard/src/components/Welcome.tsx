import { FocusZone, FocusZoneDirection, ScrollablePane, ScrollbarVisibility, Stack, Text } from "@fluentui/react";
import { Link } from 'react-router-dom';
import { useWindowSize } from "../base/utilities/hooks";
import { getHordeStyling } from "../styles/Styles";
import { Breadcrumbs } from "./Breadcrumbs";
import { TopNav } from "./TopNav";


const WelcomePanel: React.FC = () => {

   const { hordeClasses } = getHordeStyling();

    return (<Stack style={{ width: 1390, marginLeft: 4 }}>
        <Stack className={hordeClasses.raised}>
            <Stack tokens={{ childrenGap: 12 }}>
                <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Welcome to Horde!</Text>
                <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
                    <Stack>
                        <Text block>Horde is a horizontally scalable, distributed build platform, tightly integrated with Unreal Engine.</Text>
                    </Stack>
                </Stack>
            </Stack>
        </Stack>
    </Stack>);
};


const BuildPanel: React.FC = () => {

   const { hordeClasses } = getHordeStyling();

    return (<Stack style={{ width: 1390, marginLeft: 4 }}>
        <Stack className={hordeClasses.raised}>
            <Stack tokens={{ childrenGap: 12 }}>
                <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Build Automation</Text>
                <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
                    <Link style={{fontSize: 14}} to="/server/bootstrap" >Set up build automation... </Link>
                </Stack>
            </Stack>
        </Stack>
    </Stack>);
};


const WelcomeViewInner: React.FC = () => {

   const { detailClasses } = getHordeStyling();

    return <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: "#fffffff", margin: 0, paddingTop: 8 } }}>
        <Stack style={{ padding: 0 }} className={detailClasses.detailsRow}>
            <FocusZone direction={FocusZoneDirection.vertical} style={{ padding: 0 }}>
                <div className={detailClasses.container} style={{ width: "100%", height: 'calc(100vh - 208px)', position: 'relative' }} data-is-scrollable={true}>
                    <ScrollablePane scrollbarVisibility={ScrollbarVisibility.auto} onScroll={() => { }}>
                        <Stack tokens={{ childrenGap: 18 }} style={{ padding: 0 }}>
                            <Stack horizontalAlign="center">
                                <img alt="" src="/images/unreal_logo_black.svg" width={300} />
                            </Stack>
                            <Stack>
                                <WelcomePanel />
                            </Stack>
                            <Stack>
                                <BuildPanel />
                            </Stack>
                        </Stack>
                    </ScrollablePane>
                </div>
            </FocusZone>
        </Stack>
    </Stack>
};

export const WelcomeView: React.FC = () => {

    const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);
   
   const { hordeClasses } = getHordeStyling();

    return (
        <Stack className={hordeClasses.horde}>
            <TopNav suppressServer={true}/>
            <Breadcrumbs items={[{ text: 'Welcome' }]} suppressHome={true} />
            <Stack horizontal>
                <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 720, flexShrink: 0, backgroundColor: 'rgb(250, 249, 249)' }} />
                <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: 'rgb(250, 249, 249)', width: "100%" } }}>
                    <Stack style={{ maxWidth: 1420, paddingTop: 6, marginLeft: 4 }}>
                        <WelcomeViewInner />
                    </Stack>
                </Stack>
            </Stack>
        </Stack>);

}
