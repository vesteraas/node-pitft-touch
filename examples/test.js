var touchscreen = require("../index");

var touchCount = 0;

touchscreen("/dev/input/touchscreen", function(err, data) {
    if (err) {
        throw err;
    }

    // Stop after 10 touches
    if (touchCount++ == 10) {
        data.stop = true;
    }

    console.log(data);
});
