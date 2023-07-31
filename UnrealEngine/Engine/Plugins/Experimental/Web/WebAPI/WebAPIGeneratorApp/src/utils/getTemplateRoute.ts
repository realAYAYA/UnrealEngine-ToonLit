import path from 'path';

const getTemplateRoute = () => {
    let root = __dirname;
    const srcIdx = root.indexOf('src');    
    return path.resolve(`${root.substring(0, srcIdx)}`, 'templates/');
}

module.exports = getTemplateRoute();
