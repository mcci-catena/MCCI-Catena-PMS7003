/*

Name:   CalculatePmAqi()

Description:
    Calculate the NWS AQI given PM 1, PM 2.5 and PM 10 concentrations

Definition:
    function CalculatePmAqi(pm2_5, pm10) -> { AQI_2_5=x, AQI_10=x, AQI=x }

Description:
    pm2_5 is the PM2.5 concentration, pm10 is the PM10 concentration, in ug/m3.
    If either is null, the corresponding conversion is skipped. AQI is the greater
    of the resulting two AQIs.

Returns:
    An object with three fields.

*/

function CalculatePmAqi(pm2_5, pm10)
    {
    var result = {};
    result.AQI_2_5 = null;
    result.AQI_10 = null;
    result.AQI = null;

    function interpolate(v, t)
        {
        if (v === null)
            return null;

        var i;
        for (i = t.length - 2; i > 0; --i)
            if (t[i][0] <= v)
                break;

        var entry = t[i];
        var baseX = entry[0];
        var baseY = entry[1];
        var dx = (t[i+1][0] - baseX);
        var f = (v - baseX);
        var dy = (t[i+1][1] - baseY);
        return Math.floor(baseY + f * dy / dx + 0.5);
        }

    var t2_5 = [[0, 0], [15.5, 51], [40.5, 101], [65.5, 151], [150.5, 201], [250.5, 301], [350.5, 401]];
    var t10 =  [[0, 0], [55, 51], [155, 101], [255, 151], [355, 201], [425, 301], [505, 401]];

    result.AQI_2_5 = interpolate(pm2_5, t2_5);
    result.AQI_10 = interpolate(pm10, t10);
    if (result.AQI_2_5 === null)
        result.AQI = result.AQI_10;
    else if (result.AQI_10 === null)
        result.AQI = result.AQI_2_5;
    else if (result.AQI_2_5 > result.AQI_10)
        result.AQI = result.AQI_2_5;
    else
        result.AQI = result.AQI_10;

    return result;
    }
