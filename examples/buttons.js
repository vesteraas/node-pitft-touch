var touchscreen = require("../index.js");
var fb = require("pitft")("/dev/fb1", true); // run 'npm install pitft' in the example directory

var gui = {};

for (var y=0; y<4; y++) {
    for (var x=0; x<4; x++) {
        gui["button_" + (y * 4 + x)] = {
            "type": "button",
            "pos": [x * 80 + 5, y * 60 + 5],
            "size": [70, 50],
            "background_color": [0, 0, 1],
            "pressed_color": [1, 0, 1],
            "text": {
                "caption": "" + (y * 4 + x),
                "font": ["fantasy", 24, true],
                "color": [1, 1, 0]
            },
            "pressed": false
        }
    }
}

var drawElement = function(element) {
    if (gui[element].pressed) {
        fb.color(gui[element].pressed_color[0], gui[element].pressed_color[1], gui[element].pressed_color[2]);
    } else {
        fb.color(gui[element].background_color[0], gui[element].background_color[1], gui[element].background_color[2]);
    }
    fb.rect(gui[element].pos[0], gui[element].pos[1], gui[element].size[0], gui[element].size[1], true);
    fb.color(gui[element].text.color[0], gui[element].text.color[1], gui[element].text.color[2]);
    fb.font(gui[element].text.font[0], gui[element].text.font[1], gui[element].text.font[2]);
    fb.text(gui[element].pos[0] + gui[element].size[0] / 2, gui[element].pos[1] + gui[element].size[1]/2, gui[element].text.caption, true);
}

var getElementUnderCursor = function(x, y) {
    for (var element in gui) {
        if (gui.hasOwnProperty(element)) {
            if (x >= gui[element].pos[0] && x < (gui[element].pos[0] + gui[element].size[0])) {
                if (y >= gui[element].pos[1] && y < (gui[element].pos[1] + gui[element].size[1])) {
                    return element;
                }
            }
        }
    }

    return null;
}

fb.clear();
for (var element in gui) {
    if (gui.hasOwnProperty(element)) {
        drawElement(element);
    }
}
fb.blit();

var elementUnderCursor, pressedElement;

touchscreen("/dev/input/touchscreen", function(err, data) {
    if (err) {
        throw err;
    }

    var screenX = ((270 - 50) / (693 - 3307)) * data.x + 320;
    var screenY = ((190 - 50) / (3220 - 996)) * data.y;

    if (pressedElement && data.touch == 0) {
        gui[pressedElement].pressed = false;
        drawElement(pressedElement);
    }

    elementUnderCursor = getElementUnderCursor(screenX, screenY);

    if (elementUnderCursor) {
        if (data.touch == 1) {
            pressedElement = elementUnderCursor;
            gui[pressedElement].pressed = true;
            drawElement(pressedElement);
            console.log(gui[pressedElement].text.caption);
        }
    }

    fb.blit();
});
