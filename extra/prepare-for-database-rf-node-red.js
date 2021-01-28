/*

Name:   prepare-for-database-rf-node-red.js

Function:
    Prepare RF database entries for InfluxDB in Node-RED.

Copyright and License:
    See accompanying LICENSE file at https://github.com/mcci-catena/MCCI-Catena-4430/

Author:
    Terry Moore, MCCI Corporation   November 2019

*/

// pattern to match for extracing spreading factor and bandwidth.
var data_rate_re = /SF(\d+)BW(\d+)/;

// use the pattern, extract the values.
var dataRateArray = data_rate_re.exec(msg.metadata.data_rate);

// we're going to record the best gateway; we need to find it.
function findBestGateway(g) {
    var rssi = -1000;
    var bestRssi;
    var snr = -1000;
    var bestSnr;

    for (var i in g) {
        if (g[i].rssi > rssi) {
            rssi = g[i].rssi;
            bestRssi = i;
        }
        if (g[i].snr > snr) {
            snr = g[i].snr;
            bestSnr = i;
        }
    }

    if (bestRssi == bestSnr)
        return g[bestRssi];

    if (rssi < -80)
        return g[bestSnr];
    else
        return g[bestRssi];
}

var g = findBestGateway(msg.metadata.gateways);

// Give InfluxDB an array with two entries, fields and tags.
// we can do arithmetic on tags, we can query on tags.
var result =
{
    payload:
        [{
            frequency: msg.metadata.frequency,
            channel: g.channel,
            datarate: msg.metadata.data_rate,
            codingrate: msg.metadata.coding_rate,
            spreadingFactor: Number(dataRateArray[1]),
            bandwidth: Number(dataRateArray[2]),
            rssi: g.rssi,
            snr: g.snr,
            msgID: msg._msgid,
            counter: msg.counter,
        },
        {
            devEUI: msg.hardware_serial,
            devID: msg.dev_id,
	    displayKey: msg.app_id + "." + msg.dev_id,
            gatewayEUI: g.gtw_id,
            nodeType: msg.local.nodeType,
            platformType: msg.local.platformType,
            radioType: msg.local.radioType,
            applicationName: msg.local.applicationName,
            // we make these tags so we can plot rssi by 
            // channel, etc.
            frequency: msg.metadata.frequency,
            channel: g.channel,
            datarate: msg.metadata.data_rate,
            spreadingFactor: Number(dataRateArray[1]),
            bandwidth: Number(dataRateArray[2]),
            codingrate: msg.metadata.coding_rate,
        }]
};

// if there's a time in the input, copy it to the output.
// unluckily, InfluxDB won't accept the time with all the info
// so we have to tear it apart and convert it.
if ('time' in msg.metadata) {
    var time_frac_re = /^(.+)([.].+)Z$/;

    // use the pattern, extract the values.
    var timeArray = time_frac_re.exec(msg.metadata.time);

    if (timeArray !== null) {
        // set base time, in nanos.
        // [0] is the whole match
        // [1] is the first capture
        // [2] is the second capture
        var baseTime = (new Date(timeArray[1] + "Z" ).getTime()) * 1000000;

        var nanos = (Number(timeArray[2]) * 1e9);

        // combine the times and set the timestamp.
        result.payload[0].time = baseTime + nanos;
    }
}

return result;
