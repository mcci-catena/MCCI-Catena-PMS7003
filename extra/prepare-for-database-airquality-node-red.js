/*

Name:   prepare-for-database-airquality-node-red.js

Function:
    Given decoded data, prepare the array objects u;sed for inserting into InfluxDB.

Copyright and License:
    See accompanying LICENSE file at https://github.com/mcci-catena/MCCI-Catena-PMS7003/

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

// two things happen here:
// 1. we do any final database normalization
// 2. we convert to the instructions for inserting into influx.
//
// Database normalization is somewhat experiment-specific. The names from
// the standard conversion routine may not be to our taste; or we might
// not want to record all the points.
//
// This is a convenient place to make any adjustments, because what we have
// is utterly unlike what Influx expects.
//
// Our input data is a simple object, where msg.payload.* has individual
// fields of data.
//
// Influx wants to seen an array of arrays of data, divided into "values"
// and "tags".
//
// At the most basic level, we prepare an array with two elements; element[0]
// has the values (which can be operated upon in forumulae) and the timestamp.
// element [1] has the tags (which are generally strings, which are more or
// less keys you can use for querying the data). So this transformation is
// where the final schema is really defined.
//

// we will return this object. Per Node-red and influx, result.payload[0]
// is the values, and resutl.payload[1] is the tags.  If we wanted to insert
// multiples with a single insert, with different tiemestamps, we'd have
// to make result.payload[] be an array of array; result.payload[0][0] being
// the first set of values, result.payload[0][1] being the first set of tags;
// result[1][0] being the second set ovf values, etc.
//
// Out of habit and efficiency, we'll use the fact that JavaScript refers
// to portions of an object by reference, not by value, so we can set up
// "pointers" below.
var result =
{
    payload:
      [
        //[0]: initialize the consistent portions of the
        //     values.
        {
        // message ID
        msgID: msg._msgid,

        // FCntUp from LoRaWAN
        counter: msg.counter,

        // we use the time of insertion as the time. We could also
        // use the network time of uplink, by setting the field "time"
        // to the nanoseconds since the epoch.
        },
        //[1]: initialze the consistent tags that are based on node ID.
        //     Caution: all of these items must be present in the message
        //     or Node-RED will raise an error.
        {
        devEUI: msg.hardware_serial,
        devID: msg.dev_id,
        displayKey: msg.app_id + "." + msg.dev_id,
        nodeType: msg.local.nodeType,
        platformType: msg.local.platformType,
        radioType: msg.local.radioType,
        applicationName: msg.local.applicationName,
        }
      ]
};

// set `values` to be a pointer to "values" part of the payload.
// saves typing and is faster at runtime.
var values = result.payload[0];
// similarly, set `tags`.
var tags = result.payload[1];

// We need to know what measurments in the input must be copied to the output.
// The easy way to do this in JavaScript is to load an array with the names
// of the input fields to be copied.
var value_keys = [
            "vBat", "vBus", "vSys", "boot", "tempC", "TVOC", "tDewC", "tHeatIndexC", "rh", "pm", "dust", "aqi", "aqi_partial"
            ];

// Similary, we may have input fields that want to become tags in the output.
// we list them here by name.
var tag_keys = [
    /* none */
    ];

// This helper function inserts a key/value pair in the output set of values
// or tags (depending on what you pass as pOutput).
//
// As a special case, if inValue is an object, we create a number of key/value
// pairs in the output table. For example, if sInKey is "dust", and inValue is
// { "0.3": 10, "0.5": 20}, then we'll insert { "dust.0.3": 10 } and { "dust.0.5: 20 }
// in the output table.  It does this by recursing, which means it can handle
// several layers.
function insert_value(pOutput, sInKey, inValue)
    {
    // deal with recusion and nested objects
    if (typeof inValue == "object" )
        {
        for (var i in inValue)
            insert_value(pOutput, sInKey + "." + i, inValue[i]);
        }
    // handle the normal case.
    else
        pOutput[sInKey] = inValue;
    }

// Back to mainline code. Having defined `insert_value()`, 
// we now use it to add the fields we want as influxdb values
// to the part of the result that establishes values.
for (var i in value_keys)
    {
    var key = value_keys[i];
    if (key in msg.payload)
        {
        insert_value(values, key, msg.payload[key]);
        }
    }

// do the same thing for tags.
for (var i in tag_keys)
    {
    var key = tag_keys[i];
    if (key in msg.payload)
        tags[key] = msg.payload[key];
    }

// now we can return the resuting object; result.payload[0] and [1] were
// modified above using `values` and `tags` as pointers.
return result;
