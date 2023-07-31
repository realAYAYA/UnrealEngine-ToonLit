import React from 'react';
import { ReactComponent as Logo } from '../assets/ue_logo.svg';


export class Login extends React.Component {

  render() {
    return (
      <div className="fullscreen login-screen">
        <div className='icon-wrapper'>
          <div className="app-icon">
            <Logo className="logo" />
          </div>
          <div>Remote Control Web App</div>
        </div>
        <div className='form'>
          Password
          <input type='password'/>
          <div className='hint'>Configure the password in: <span className="breadcrumb">Project Settings / Authentication / Password</span></div>
          <label className='login-status'>Incorrect Password</label>
        </div>
     </div>
    );
  }
};