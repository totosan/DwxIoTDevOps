load('api_azure.js');
load('api_config.js');
load('api_events.js');
load('api_gcp.js');
load('api_gpio.js');
load('api_mqtt.js');
load('api_shadow.js');
load('api_timer.js');
load('api_sys.js');
load('api_dht.js');

// GPIO pin which has a DHT sensor data wire connected
let pin = 5;

// Initialize DHT library
let dht = DHT.create(pin, DHT.DHT11);

let led = Cfg.get('board.led1.pin');              // Built-in LED GPIO number
let onhi = Cfg.get('board.led1.active_high');     // LED on when high?
let state = { on: false, btnCount: 0, uptime: 0 };  // Device state
let online = false;                               // Connected to the cloud?

let setLED = function (on) {
  let level = onhi ? on : !on;
  GPIO.write(led, level);
  print('LED on ->', on);
};

GPIO.set_mode(led, GPIO.MODE_OUTPUT);
setLED(state.on);

let reportState = function () {
  Shadow.update(0, state);
};

 let sendData = function () {
  let message = JSON.stringify(state);
  let sendMQTT = true;
  if (Azure.isConnected()) {
    print('== Sending Azure D2C message:', message);
    Azure.sendD2CMsg('', message);
    sendMQTT = false;
  } else if (sendMQTT) {
    print('== Not connected!');
  }
};

// Update state every second, and report to cloud if online
Timer.set(10000, Timer.REPEAT, function () {
    state.uptime = Sys.uptime();
    state.ram_free = Sys.free_ram();
    print('online:', online, JSON.stringify(state));
    if (online) reportState();

    let t = dht.getTemp();
    let h = dht.getHumidity();

    if (isNaN(h) || isNaN(t)) {
      print('Failed to read data from sensor');
      return;
    }

    print('Temperature:', t, '*C','Humidity:', h, '%');
  }, 
  null);

// Set up Shadow handler to synchronise device state with the shadow state
Shadow.addHandler(function (event, obj) {
  if (event === 'UPDATE_DELTA') {
    print('GOT DELTA:', JSON.stringify(obj));
    for (let key in obj) {  // Iterate over all keys in delta
      if (key === 'on') {   // We know about the 'on' key. Handle it!
        state.on = obj.on;  // Synchronise the state
        setLED(state.on);   // according to the delta
      } else if (key === 'reboot') {
        state.reboot = obj.reboot;      // Reboot button clicked: that
        Timer.set(750, 0, function () {  // incremented 'reboot' counter
          Sys.reboot();                 // Sync and schedule a reboot
        }, null);
      }
    }
    reportState();  // Report our new state, hopefully clearing delta
  }
});


Event.on(Event.CLOUD_CONNECTED, function () {
  online = true;
  Shadow.update(0, { ram_total: Sys.total_ram() });
}, null);

Event.on(Event.CLOUD_DISCONNECTED, function () {
  online = false;
}, null);
