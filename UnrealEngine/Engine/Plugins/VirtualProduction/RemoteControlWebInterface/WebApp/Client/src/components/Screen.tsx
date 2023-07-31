import React from 'react';
import { connect } from 'react-redux';
import { ReduxState } from 'src/reducers';
import { IScreen, IPayload, ScreenType } from 'src/shared';
import * as Screens from './Screens';


type PropsFromState = Partial<{
  payload: IPayload;
}>;

type Props = {
  screen: IScreen;
  visible?: boolean;
};

const mapStateToProps = (state: ReduxState): PropsFromState => ({
  payload: state.api.payload
});

@(connect(mapStateToProps) as any)
export class Screen extends React.Component<Props & PropsFromState> {

  render() {
    const { screen, payload, visible } = this.props;
    if (!screen)
      return null;

    const props = {
      payload,
      visible,
      data: screen.data,
    };

    switch (screen.type) {
      case ScreenType.Snapshot:
        return <Screens.Snapshot {...props} />;

      case ScreenType.Sequencer:
        return <Screens.Sequencer {...props} />;

      case ScreenType.Playlist:
        return <Screens.Playlist {...props} />;
      
      case ScreenType.ColorCorrection:
        return <Screens.ColorCorrection {...props} />;
      
      case ScreenType.LightCards:
        return <Screens.LightCards {...props} />;
    }

    return null;
  }
}