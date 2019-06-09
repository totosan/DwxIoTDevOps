function init() {
    var app = new Vue({
        el: '#esp',
        data: {
            functionAppName: '',
            reportedTwin: {
                "device":{
                    "heap_free":0,
                    "sketchspace_free": 0,
                    "sketch_free": 0
                },
                "fwVersion":'',
                "update_state":''
            },
            deviceId:'Buro',
            lastUpdated: '',
            stateUpdateEndpoint: 'about:blank',
            loading: false,
            restarting: false,
            restarted: false,
            functionAppNameSet: false
        },
        computed: {
        },
        methods: {
            deviceRestart: deviceRestart,
            getTwin: getTwin,
            getState: getState
        }
    });
}

function getTwin() {
    var scope = this;
    var timespan = new Date().getTime();
    var url = `https://${this.functionAppName}.azurewebsites.net/api/device-state?action=get&t=${timespan}`;
    this.$http.jsonp(url).then(function(data){
        return data.json();
    }).then(function (data){
        scope.loading = false;
        scope.reportedTwin = data;
        scope.lastUpdated = new Date(scope.reportedTwin.$metadata.$lastUpdated).toLocaleString('de-DE', {hour12: false}).replace(',', '');
    });
}

function getState() {
    if (!this.functionAppName)
    {
        alert('No function name found.');
        return;
    }
    this.functionAppNameSet = true;
    this.loading = true;
    setInterval(this.getTwin, 1000);
}

function deviceRestart(){
    var url = `https://${this.functionAppName}.azurewebsites.net/api/device-state?action=restart`;
    this.$http.jsonp(url);
}


init();
