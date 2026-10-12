#include "web_portal.h"

#include <WebServer.h>
#include <WiFi.h>

#include "alert_manager.h"
#include "body_temp.h"
#include "config.h"
#include "env_monitor.h"
#include "pipe_link.h"
#include "vitals_sensor.h"

// ============================================================
//  Local web portal (safe mode)
//
//  Three pages and one JSON endpoint, served straight from the
//  node. The portal is what remains when the cloud link cannot be
//  established, so it depends on nothing that is not already on the
//  board: each page carries its own CSS and its own polling script,
//  which means it works on a phone that joined the same network
//  with no internet behind it.
//
//  Every endpoint is a read-only GET apart from /reset, which is
//  the one action an operator has from a phone.
// ============================================================

namespace
{
WebServer g_server(HTTP_PORT);

// A reading the DHT11 did not answer with is reported as null
// instead of as a number, so the pages render it as "--" and no
// browser has to parse the JSON spelling of a NaN.
String numberOrNull(float value, unsigned int decimals)
{
    if (isnan(value))
    {
        return "null";
    }

    return String(value, decimals);
}

void handleRoot()
{
    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, 1'><style>";
    html += "body{font-family:sans-serif; background:#f4f7f6; text-align:center; padding:50px 20px;}";
    html += ".card{max-width:450px; margin:auto; background:white; padding:30px; border-radius:20px; box-shadow:0 10px 25px rgba(0,0,0,0.1);}";
    html += ".btn{display:block; padding:15px; margin:10px 0; background:#3498db; color:white; text-decoration:none; border-radius:10px; font-weight:bold; transition:0.3s;}";
    html += ".btn:hover{opacity:0.8;}</style></head><body><div class='card'><h1>LifeGuard</h1><p>Local Access Portal</p>";
    html += "<a href='/admin' class='btn' style='background:#2c3e50;'>Admin Dashboard</a>";
    html += "<a href='/public' class='btn'>Public Monitor</a></div></body></html>";
    g_server.send(200, "text/html", html);
}

void handleAdmin()
{
    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, 1'><style>";
    html += "body{font-family:sans-serif; background:#2c3e50; color:white; padding:20px; text-align:center;}";
    html += ".card{background:#34495e; padding:20px; border-radius:15px; margin:10px auto; max-width:500px;}";
    html += ".btn{background:#e67e22; color:white; padding:20px; border:none; border-radius:10px; width:100%; font-size:1.2em; cursor:pointer;}";
    html += "span{font-weight:bold; color:#f1c40f;} .alert{color:#ff4d4d; animate:blink 1s infinite;}</style>";
    html += "<script>setInterval(()=>{fetch('/api').then(r=>r.json()).then(d=>{";
    html += "document.getElementById('sit').innerText=d.sit; document.getElementById('env').innerText='Gas: '+d.gas+'% | Temp: '+d.temp+'C | Hum: '+d.hum+'%';";
    html += "if(d.sit!='Normal') document.getElementById('sit').className='alert'; else document.getElementById('sit').className='';";
    html += "})}, 2000);</script></head><body><h1>Admin Dashboard</h1>";
    html += "<div class='card'><h2>Pipeline Control</h2><p>Status: <span id='sit'>...</span></p>";
    html += "<form action='/reset' method='POST'><button class='btn'>RESOLVE SITUATION</button></form></div>";
    html += "<div class='card'><h2>Station Environment</h2><p id='env'>Loading sensors...</p></div>";
    html += "<a href='/' style='color:#bdc3c7;'>&larr; Back to Portal</a></body></html>";
    g_server.send(200, "text/html", html);
}

void handlePublic()
{
    const float bodyTempC = bodyTempCapturedC();

    const String recommendation = (bodyTempC > 38.0) ? "Fever Detected: Hydrate."
        : (vitalsSensorLastSpo2() < 94)               ? "Low Oxygen: Seek air."
                                                      : "Vitals Normal.";

    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, 1'><style>";
    html += "body{font-family:sans-serif; background:#ecf0f1; padding:20px; text-align:center;}";
    html += ".box{background:white; padding:20px; border-radius:15px; margin:10px auto; max-width:500px; border-top:5px solid #27ae60;}";
    html += ".val{font-size:2em; color:#2c3e50; font-weight:bold;} .rec{background:#d5f5e3; padding:15px; border-radius:5px;}</style>";
    html += "<script>setInterval(()=>{fetch('/api').then(r=>r.json()).then(d=>{";
    html += "document.getElementById('p_env').innerText='Gas: '+d.gas+'% | Temp: '+d.temp+'C | Hum: '+d.hum+'%';";
    html += "})}, 2000);</script></head><body><h1>Public Monitor</h1>";
    html += "<div class='box'><h3>Latest Vitals</h3><p>HR: <span class='val'>" + String(vitalsSensorLastBpm()) + "</span> | SpO2: <span class='val'>" + String(vitalsSensorLastSpo2()) + "</span>%</p>";
    html += "<p>Body Temp: <span class='val'>" + String(bodyTempC, static_cast<unsigned int>(BODY_TEMP_DECIMALS)) + "</span>C</p></div>";
    html += "<div class='box'><h3>Environment</h3><p id='p_env'>Loading...</p></div>";
    html += "<div class='box'><h3>Recommendation</h3><p class='rec'>" + recommendation + "</p></div>";
    html += "<a href='/' style='color:#7f8c8d;'>&larr; Back to Portal</a></body></html>";
    g_server.send(200, "text/html", html);
}

void handleApi()
{
    String json = "{";
    json += "\"sit\":\"" + String(pipeLinkSituation()) + "\",";
    json += "\"gas\":" + String(envMonitorGasPercent()) + ",";
    json += "\"temp\":" + numberOrNull(envMonitorTemperatureC(), CLOUD_TEMP_DECIMALS) + ",";
    json += "\"hum\":" + numberOrNull(envMonitorHumidityPct(), CLOUD_TEMP_DECIMALS);
    json += "}";

    g_server.send(200, "application/json", json);
}
} // namespace

void webPortalBegin()
{
    g_server.on("/", handleRoot);
    g_server.on("/admin", handleAdmin);
    g_server.on("/public", handlePublic);
    g_server.on("/api", handleApi);
    g_server.on("/reset", HTTP_POST, []() {
        alertManagerClear();
        g_server.sendHeader("Location", "/admin", true);
        g_server.send(302);
    });

    g_server.begin();

    Serial.printf("[portal] http://%s\n", WiFi.localIP().toString().c_str());
}

void webPortalService()
{
    g_server.handleClient();
}
