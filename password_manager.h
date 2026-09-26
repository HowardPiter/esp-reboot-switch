#pragma once

#include "esphome.h"
#include <ESPAsyncWebServer.h>
#include "esphome/core/preferences.h"
#include "esphome/core/application.h"
#include <ESP8266WiFi.h>
#include <user_interface.h>

#define REASON_POWER_ON 1
#define REASON_RST_PIN 6
#define REASON_WDT_RST 4
#define RST_SERIES_WINDOW_MS 8000

struct RTCData {
    int reset_counter;
    uint32_t magic;
};

struct PasswordData {
    char password[32];
    uint32_t magic;
};
#define PASSWORD_MAGIC 0xBEEF

struct WifiResetData {
    bool ap_only_mode;
    uint32_t magic;
};
#define WIFI_RESET_MAGIC 0xCAFE

struct WebEnabledData {
    bool web_enabled;
    uint32_t magic;
};
#define WEB_ENABLED_MAGIC 0xDEAD

static AsyncWebServer *custom_web_server = nullptr;
static String current_password = "admin";
static RTCData rtc_data;
static ESPPreferenceObject password_pref;
static ESPPreferenceObject wifi_reset_pref;
static ESPPreferenceObject web_enabled_pref;
static bool ap_only_mode = false;
static bool web_enabled = true;

static esphome::text_sensor::TextSensor *g_password_status_sensor = nullptr;

static void bind_password_status_sensor(esphome::text_sensor::TextSensor *sensor) {
    g_password_status_sensor = sensor;
}

static void publish_password_status(const char* message) {
    if (g_password_status_sensor) {
        g_password_status_sensor->publish_state(message);
    }
}

static const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP Reboot Switch</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f0f2f5; padding: 20px; }
        .container { max-width: 600px; margin: 0 auto; }
        .card { background: white; border-radius: 12px; padding: 20px; margin-bottom: 16px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
        h1 { color: #1a1a1a; font-size: 24px; margin-bottom: 8px; }
        h2 { color: #666; font-size: 14px; font-weight: normal; margin-bottom: 20px; }
        .status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 16px; }
        .status-item { background: #f8f9fa; padding: 12px; border-radius: 8px; }
        .status-label { font-size: 12px; color: #666; margin-bottom: 4px; }
        .status-value { font-size: 18px; font-weight: bold; color: #1a1a1a; }
        .btn { width: 100%; padding: 14px; border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; margin-bottom: 12px; transition: all 0.2s; }
        .btn-primary { background: #1976d2; color: white; }
        .btn-primary:hover { background: #1565c0; }
        .btn-danger { background: #d32f2f; color: white; }
        .btn-danger:hover { background: #c62828; }
        .btn-success { background: #388e3c; color: white; }
        .btn-success:hover { background: #2e7d32; }
        .form-group { margin-bottom: 16px; }
        label { display: block; font-size: 14px; color: #666; margin-bottom: 6px; }
        input, select { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 8px; font-size: 14px; }
        input:focus, select:focus { outline: none; border-color: #1976d2; }
        .alert { padding: 12px; border-radius: 8px; margin-bottom: 16px; font-size: 14px; }
        .alert-info { background: #e3f2fd; color: #1565c0; }
        .alert-warning { background: #fff3cd; color: #856404; }
        .section-title { font-size: 18px; font-weight: bold; margin-bottom: 16px; color: #1a1a1a; }
    </style>
</head>
<body autocomplete="off">
    <div class="container">
        <div class="card">
            <h1>ESP Reboot Switch</h1>
            <h2>Устройство перезагрузки</h2>
            <div class="status-grid">
                <div class="status-item"><div class="status-label">IP адрес</div><div class="status-value" id="ip">-</div></div>
                <div class="status-item"><div class="status-label">WiFi сигнал</div><div class="status-value" id="rssi">-</div></div>
                <div class="status-item"><div class="status-label">Время работы</div><div class="status-value" id="uptime">-</div></div>
                <div class="status-item"><div class="status-label">Питание</div><div class="status-value" id="power">-</div></div>
            </div>
        </div>
        <div class="card">
            <div class="section-title">Управление</div>
            <button class="btn btn-danger" id="powerBtn" onclick="togglePower()">ВЫКЛ</button>
            <button class="btn btn-primary" onclick="rebootServer()">Перезагрузить</button>
        </div>
        <div class="card">
            <div class="section-title">Настройки</div>
            <div class="form-group">
                <label>Время отключения (секунд):</label>
                <input type="number" id="rebootDelay" min="5" max="120" value="10" autocomplete="off">
            </div>
            <button class="btn btn-success" onclick="setRebootDelay()">Сохранить время</button>
            <div class="form-group" style="margin-top: 16px;">
                <label>Поведение при включении питания:</label>
                <select id="powerBehavior" autocomplete="off">
                    <option value="always_on">Всегда включено</option>
                    <option value="always_off">Всегда выключено</option>
                    <option value="restore">Восстановить последнее состояние</option>
                </select>
            </div>
            <button class="btn btn-success" onclick="setPowerBehavior()">Сохранить поведение</button>
        </div>
        <div class="card">
            <div class="section-title">Смена пароля</div>
            <div class="alert alert-warning">
                <strong>Внимание:</strong> Пароль меняется без проверки старого. Если вы забыли пароль — нажмите кнопку RST на устройстве 5 раз подряд для сброса к "admin".
            </div>
            <div class="form-group">
                <label>Новый пароль (мин. 4 символа):</label>
                <input type="password" id="newPassword" autocomplete="new-password">
            </div>
            <div class="form-group">
                <label>Подтвердите новый пароль:</label>
                <input type="password" id="confirmPassword" autocomplete="new-password">
            </div>
            <button class="btn btn-primary" onclick="changePassword()">Сменить пароль</button>
        </div>
        <div class="card">
            <div class="section-title">Сброс настроек</div>
            <div class="alert alert-info">
                <strong>Сброс пароля:</strong> 5 нажатий RST подряд (интервал ~1 сек) — сброс пароля к "admin".<br>
                <strong>Полный сброс:</strong> 7 нажатий RST подряд — сброс пароля + Wi-Fi + включение веб-интерфейса.
            </div>
        </div>
    </div>
    <script>
        function updateStatus() {
            fetch('/api/status').then(r => r.json()).then(data => {
                document.getElementById('ip').textContent = data.ip;
                document.getElementById('rssi').textContent = data.rssi + ' dBm';
                document.getElementById('uptime').textContent = Math.floor(data.uptime / 60) + ' мин';
                const powerState = data.power ? 'ВКЛ' : 'ВЫКЛ';
                document.getElementById('power').textContent = powerState;
                const powerBtn = document.getElementById('powerBtn');
                powerBtn.textContent = data.power ? 'ВЫКЛ' : 'ВКЛ';
                powerBtn.className = data.power ? 'btn btn-danger' : 'btn btn-success';
                document.getElementById('rebootDelay').value = data.reboot_delay;
                document.getElementById('powerBehavior').value = data.power_behavior;
            }).catch(() => {});
        }
        function togglePower() { fetch('/api/toggle_power').then(() => { setTimeout(updateStatus, 500); }); }
        function rebootServer() {
            if(confirm('Перезагрузить? Если питание включено — оно будет отключено на заданное время.')) {
                fetch('/api/reboot').then(r => r.json()).then(data => { alert(data.message); setTimeout(updateStatus, 1000); });
            }
        }
        function setRebootDelay() {
            const delay = document.getElementById('rebootDelay').value;
            fetch('/api/set_reboot_delay?delay=' + delay).then(r => r.json()).then(data => { alert(data.message); setTimeout(updateStatus, 500); });
        }
        function setPowerBehavior() {
            const behavior = document.getElementById('powerBehavior').value;
            fetch('/api/set_power_behavior?behavior=' + behavior).then(r => r.json()).then(data => { alert(data.message); setTimeout(updateStatus, 500); });
        }
        function changePassword() {
            const newPass = document.getElementById('newPassword').value;
            const confirmPass = document.getElementById('confirmPassword').value;
            if(newPass !== confirmPass) { alert('Пароли не совпадают!'); return; }
            if(newPass.length < 4) { alert('Пароль должен быть минимум 4 символа!'); return; }
            fetch('/api/change_password?new=' + encodeURIComponent(newPass))
                .then(r => r.json()).then(data => {
                    alert(data.message);
                    if(data.success) {
                        document.getElementById('newPassword').value = '';
                        document.getElementById('confirmPassword').value = '';
                    }
                });
        }
        updateStatus();
        setInterval(updateStatus, 5000);
    </script>
</body>
</html>
)rawliteral";

static void loadPassword() {
    PasswordData data;
    if (password_pref.load(&data) && data.magic == PASSWORD_MAGIC) {
        current_password = String(data.password);
        ESP_LOGI("password_manager", "Загружен сохраненный пароль (длина: %d)", current_password.length());
    } else {
        current_password = "admin";
        ESP_LOGI("password_manager", "Пароль не найден, используем: admin");
    }
}

static void savePassword(String password) {
    PasswordData data;
    data.magic = PASSWORD_MAGIC;
    memset(data.password, 0, sizeof(data.password));
    strncpy(data.password, password.c_str(), sizeof(data.password) - 1);
    password_pref.save(&data);
    ESP_LOGI("password_manager", "Пароль сохранен (длина: %d)", password.length());
}

static void clearPassword() {
    PasswordData data;
    data.magic = 0;
    memset(data.password, 0, sizeof(data.password));
    password_pref.save(&data);
    ESP_LOGI("password_manager", "Пароль очищен");
}

static void loadWifiResetFlag() {
    WifiResetData data;
    if (wifi_reset_pref.load(&data) && data.magic == WIFI_RESET_MAGIC) {
        ap_only_mode = data.ap_only_mode;
    } else { ap_only_mode = false; }
    ESP_LOGI("password_manager", "Режим только точки доступа: %s", ap_only_mode ? "ВКЛ" : "выкл");
}

static void setWifiResetFlag(bool value) {
    WifiResetData data;
    data.magic = WIFI_RESET_MAGIC;
    data.ap_only_mode = value;
    wifi_reset_pref.save(&data);
    ap_only_mode = value;
}

void clearWifiApOnlyFlag() {
    if (ap_only_mode) {
        ESP_LOGI("password_manager", "Wi-Fi подключен, снимаем флаг 'только AP'");
        setWifiResetFlag(false);
    }
}

static void loadWebEnabledFlag() {
    WebEnabledData data;
    if (web_enabled_pref.load(&data) && data.magic == WEB_ENABLED_MAGIC) {
        web_enabled = data.web_enabled;
    } else { web_enabled = true; }
    ESP_LOGI("password_manager", "Веб-интерфейс: %s", web_enabled ? "ВКЛ" : "ВЫКЛ");
}

static void setWebEnabledFlag(bool value) {
    WebEnabledData data;
    data.magic = WEB_ENABLED_MAGIC;
    data.web_enabled = value;
    web_enabled_pref.save(&data);
    web_enabled = value;
    ESP_LOGI("password_manager", "Веб-интерфейс переключен: %s", value ? "ВКЛ" : "ВЫКЛ");
}

static void scheduleCounterWindowReset() {
    App.scheduler.set_timeout(nullptr, "rst_window_reset", RST_SERIES_WINDOW_MS, []() {
        if (rtc_data.reset_counter > 0 && rtc_data.reset_counter < 5) {
            ESP_LOGI("password_manager", "Окно серии RST истекло, обнуляем счетчик (было: %d)", rtc_data.reset_counter);
            rtc_data.reset_counter = 0;
            ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_data, sizeof(rtc_data));
        }
    });
}

static void performPasswordReset() {
    savePassword("admin");
    current_password = "admin";
    rtc_data.reset_counter = 0;
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_data, sizeof(rtc_data));
    ESP_LOGW("password_manager", "Пароль сброшен к admin");
}

static void performFullReset() {
    ESP_LOGW("password_manager", ">>> НАЧАТ ПОЛНЫЙ СБРОС (7 нажатий) <<<");
    clearPassword();
    current_password = "admin";
    setWifiResetFlag(true);
    setWebEnabledFlag(true);

    WiFi.disconnect(true); 
    wifi_set_opmode(SOFTAP_MODE); 
    
    ESP_LOGW("password_manager", "Wi-Fi credentials стерты, включен режим AP");
    rtc_data.reset_counter = 0;
    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_data, sizeof(rtc_data));
    delay(1000);
    ESP.restart();
}

static void startWebServer() {
    custom_web_server = new AsyncWebServer(8080);

    custom_web_server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!web_enabled) { request->send(503, "text/html", "<html><body><h1>503 Service Unavailable</h1></body></html>"); return; }
        if(!request->authenticate("admin", current_password.c_str())) return request->requestAuthentication();
        request->send(200, "text/html", INDEX_HTML);
    });

    custom_web_server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!web_enabled) { request->send(503, "application/json", "{\"error\":\"Service Unavailable\"}"); return; }
        if(!request->authenticate("admin", current_password.c_str())) return request->requestAuthentication();
        String json = "{";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"uptime\":" + String(millis() / 1000) + ",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"power\":" + String(id(server_power_relay).state) + ",";
        json += "\"reboot_delay\":" + String(id(reboot_delay_seconds).state) + ",";
        json += "\"power_behavior\":\"always_on\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    custom_web_server->on("/api/toggle_power", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!web_enabled) { request->send(503, "application/json", "{\"error\":\"Service Unavailable\"}"); return; }
        if(!request->authenticate("admin", current_password.c_str())) return request->requestAuthentication();
        id(server_power_relay).toggle();
        request->send(200, "application/json", "{\"success\":true}");
    });

    custom_web_server->on("/api/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!web_enabled) { request->send(503, "application/json", "{\"error\":\"Service Unavailable\"}"); return; }
        if(!request->authenticate("admin", current_password.c_str())) return request->requestAuthentication();
        if (!id(server_power_relay).state) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Уже выключено\"}");
            return;
        }
        id(reboot_sequence).execute();
        float delay_seconds = id(reboot_delay_seconds).state;
        String msg = "Перезагрузка через " + String(delay_seconds) + " сек";
        request->send(200, "application/json", "{\"success\":true,\"message\":\"" + msg + "\"}");
    });

    custom_web_server->on("/api/set_reboot_delay", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!web_enabled) { request->send(503, "application/json", "{\"error\":\"Service Unavailable\"}"); return; }
        if(!request->authenticate("admin", current_password.c_str())) return request->requestAuthentication();
        if(request->hasParam("delay")) {
            float delay_val = request->getParam("delay")->value().toFloat();
            id(reboot_delay_seconds).make_call().set_value(delay_val).perform();
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Время сохранено\"}");
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Параметр delay не указан\"}");
        }
    });

    custom_web_server->on("/api/set_power_behavior", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!web_enabled) { request->send(503, "application/json", "{\"error\":\"Service Unavailable\"}"); return; }
        if(!request->authenticate("admin", current_password.c_str())) return request->requestAuthentication();
        if(request->hasParam("behavior")) {
            String behavior = request->getParam("behavior")->value();
            id(power_on_behavior).make_call().set_option(behavior.c_str()).perform();
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Поведение сохранено\"}");
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Параметр behavior не указан\"}");
        }
    });

    custom_web_server->on("/api/change_password", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!web_enabled) { request->send(503, "application/json", "{\"error\":\"Service Unavailable\"}"); return; }
        if(!request->authenticate("admin", current_password.c_str())) return request->requestAuthentication();
        if(request->hasParam("new")) {
            String newPass = request->getParam("new")->value();
            if(newPass.length() < 4) {
                request->send(200, "application/json", "{\"success\":false,\"message\":\"Пароль слишком короткий (мин. 4 символа)!\"}");
                publish_password_status("Ошибка: пароль слишком короткий");
                return;
            }
            current_password = newPass;
            savePassword(current_password);
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Пароль изменен!\"}");
            publish_password_status("Пароль изменен через веб-интерфейс");
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Параметр new не указан\"}");
        }
    });

    custom_web_server->begin();
    ESP_LOGI("password_manager", "Веб-сервер запущен на порту 8080");
}



void initPasswordManager() {
    ESP_LOGI("password_manager", ">>> ИНИЦИАЛИЗАЦИЯ PASSWORD MANAGER <<<");

    ESP.rtcUserMemoryRead(0, (uint32_t*)&rtc_data, sizeof(rtc_data));
    if(rtc_data.magic != 0xA5A5) {
        rtc_data.magic = 0xA5A5;
        rtc_data.reset_counter = 0;
    }

    password_pref = global_preferences->make_preference<PasswordData>(3974051234UL);
    wifi_reset_pref = global_preferences->make_preference<WifiResetData>(3974051987UL);
    web_enabled_pref = global_preferences->make_preference<WebEnabledData>(3974052123UL);
    ESP_LOGI("password_manager", "Preferences инициализированы");

    rst_info *resetInfo = ESP.getResetInfoPtr();
    ESP_LOGI("password_manager", "Причина перезагрузки: %d", resetInfo->reason);
    ESP_LOGI("password_manager", "Счетчик RST: %d", rtc_data.reset_counter);

    if(resetInfo->reason == REASON_RST_PIN) {
        rtc_data.reset_counter++;
        ESP_LOGI("password_manager", "Увеличиваем счетчик: %d", rtc_data.reset_counter);

        if(rtc_data.reset_counter >= 7) {
            ESP_LOGW("password_manager", "ПОЛНЫЙ СБРОС! 7 нажатий RST подряд");
            performFullReset();
        } else if(rtc_data.reset_counter >= 5) {
            ESP_LOGW("password_manager", "СБРОС ПАРОЛЯ! 5 нажатий RST подряд");
            performPasswordReset();
        } else {
            scheduleCounterWindowReset();
        }
    }

    ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc_data, sizeof(rtc_data));
    loadPassword();
    loadWifiResetFlag();
    loadWebEnabledFlag();

    if (!web_enabled) {
        ESP_LOGW("password_manager", "Веб-интерфейс ОТКЛЮЧЕН через HA. Запуск пропущен.");
        return;
    }

    ESP_LOGI("password_manager", "Запускаем веб-сервер...");
    startWebServer();

    App.scheduler.set_timeout(nullptr, "pm_check", 30000, []() {
        ESP_LOGI("password_manager", "✓ Password Manager работает нормально");
    });
}

void set_web_enabled(bool enabled) {
    setWebEnabledFlag(enabled);
    if (!enabled) {
        ESP_LOGW("password_manager", "Веб-интерфейс заблокирован (возврат 403)");
    } else {
        ESP_LOGI("password_manager", "Веб-интерфейс разблокирован");
    }
}

void set_new_password(const char* new_pass) {
    String pass_str = String(new_pass);
    if(pass_str.length() < 4) {
        ESP_LOGW("password_manager", "Пароль слишком короткий, отклонен");
        publish_password_status("Ошибка: пароль слишком короткий");
        return;
    }
    current_password = pass_str;
    savePassword(current_password);
    ESP_LOGI("password_manager", "Пароль изменен через HA (длина: %d)", pass_str.length());
    publish_password_status("Пароль изменен");
}

