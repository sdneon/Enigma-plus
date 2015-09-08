//JSLint static code analysis options
/*jslint browser:true, unparam:true, sloppy:true, plusplus:true, indent:4, white:true */
/*global Pebble, console*/

var VERSION = 26,   //i.e. v2.6; for sending to config page
    //Defaults:
    DEF_VIBES = 0X00A14,
    DEF_ROW2 = false,
    DEF_ROW4 = false,
    DEF_TIMEZONE2 = 65,
    watchConfig = {
        KEY_VIBES: DEF_VIBES,
        row2: DEF_ROW2,
        row4: DEF_ROW4,
        timezone2: DEF_TIMEZONE2
    },
    //masks for vibes:
    MASKV_BTDC = 0x20000,
    MASKV_HOURLY = 0x10000,
    MASKV_FROM = 0xFF00,
    MASKV_TO = 0x00FF;

//Load saved config from localStorage
function loadConfig()
{
    var config = localStorage.getItem(1),
        vibes = parseInt(localStorage.getItem(0), 10);
    if (config)
    {
        if (typeof config === 'string')
        {
            config = JSON.parse(config);
        }
        if (config
            && (config.KEY_VIBES !== undefined)
            && (config.row2 !== undefined)
            && (config.row4 !== undefined)
            && (config.timezone2 !== undefined))
        {
            watchConfig = config;
        }
    }
    if (isNaN(vibes))
    {
        vibes = DEF_VIBES;
    }
    watchConfig.KEY_VIBES = vibes;
}

//Save config to localStorage
function saveConfig()
{
    localStorage.setItem(0, watchConfig.KEY_VIBES);
    localStorage.setItem(1, JSON.stringify(watchConfig));
}

function sendOptions(options)
{
    Pebble.sendAppMessage({
            KEY_VIBES: options.KEY_VIBES,
            row2: options.row2? 1:0,
            row4: options.row4? 1:0,
            timezone2: options.timezone2
        },
        function(e) {
            console.log('Successfully delivered message');
        },
        function(e) {
            console.log('Unable to deliver message');
        }
    );
}

Pebble.addEventListener('webviewclosed',
    function(e) {
        //console.log('Configuration window returned: ' + e.response);
        if (!e.response)
        {
            return;
        }
        var options = JSON.parse(e.response),
            noOptions = true,
            value;
        if (options.vibes !== undefined)
        {
            value = parseInt(options.vibes, 10);
            watchConfig.KEY_VIBES = value;
            noOptions = false;
        }
        if (options.row2 !== undefined)
        {
            watchConfig.row2 = !!options.row2;
            noOptions = false;
        }
        if (options.row4 !== undefined)
        {
            watchConfig.row4 = !!options.row4;
            noOptions = false;
        }
        if (options.timezone2 !== undefined)
        {
            watchConfig.timezone2 = parseInt(options.timezone2, 10);
            noOptions = false;
        }
        if (noOptions)
        {
            return;
        }

        saveConfig();
        sendOptions(watchConfig);
    }
);


Pebble.addEventListener('showConfiguration',
    function(e) {
        try {
            var url = 'http://yunharla.altervista.org/pebble/config-enigma.html?ver=' + VERSION + '&vibes=0x';
            //Send/show current config in config page:
            url += watchConfig.KEY_VIBES.toString(16);
            url += '&row2=' + watchConfig.row2
                + '&row4=' + watchConfig.row4
                + '&timezone2=' + watchConfig.timezone2;
            //console.log(url);

            // Show config page
            Pebble.openURL(url);
        }
        catch (ex)
        {
            console.log('ERR: showConfiguration exception');
        }
    }
);

Pebble.addEventListener('ready',
    function(e) {
        console.log('ready');
        loadConfig();
        sendOptions(watchConfig);
    });
