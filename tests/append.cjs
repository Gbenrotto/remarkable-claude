// Exercise the actual QML insertion function with scene/view transforms.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const source = fs.readFileSync(__dirname + '/../src/inserter.inc', 'utf8');
const fn = source.split('    function insertImageFile(')[1].split('\n}\n)QML"')[0];
function insert(content, center, scale = 1, enabled = true) {
    let inserted;
    const context = {
        Qt: { point: (x,y) => ({x,y}) },
        nativeImageHeight: 800,
        appendBelowContent: enabled,
        nativeImageView: {
            mapFromItem: (_,x,y) => ({x,y}),
            viewToScene: p => ({x:p.x*scale,y:p.y*scale+100})
        },
        sceneController: {
            boundingRect: content,
            insertImageFileAsSceneItem: (_,position) => {inserted=position;}
        }
    };
    vm.createContext(context);
    vm.runInContext('function insertImageFile(' + fn,context);
    const status=context.insertImageFile('test.png',700,center,'screen');
    return {status,inserted};
}
// Nearly full screen: append below the question, beyond the viewport.
let result=insert({width:1000,y:100,height:1700},2145);
assert.equal(result.status,'ok');assert(result.inserted.y-400>=1845);
// Existing off-screen content must not be overwritten.
result=insert({width:1000,y:100,height:4000},900);
assert(result.inserted.y-400>=4145);
// Zoomed/scrolled transform: gap and image half-height use scene units.
result=insert({width:1000,y:100,height:4000},900,0.5);
assert(result.inserted.y-200>=4122.5);
// Short page doesn't jump to paper bottom; existing behavior remains near question.
result=insert({width:1000,y:100,height:200},900);
assert.equal(result.inserted.y,1000);
// Unsupported transform is rejected before any mutation.
result=insert({width:1000,y:100,height:200},900,-1);
assert.equal(result.status,'invalid-page-orientation');assert.equal(result.inserted,undefined);
console.log('PASS: append placement, off-screen content, transformed coordinates, orientation guard');
