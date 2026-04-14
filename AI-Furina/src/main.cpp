#include <esp-ai.h>
#include <Wire.h>
#include <Battery.h>
#include "main.h"
#include "SimpleTimer.h"
#include <OneButton.h>
#include <Arduino.h>
#include <NTPClient.h>
// ==================版本定义=========================
String _version = "Fruina.V3";

// ==================OTA 升级定义=========================
// 是否为官方固件， 如果是您自己的固件请改为 "0"
String is_official = "0";

// 固件ID， 请正确填写，否则无法进行 OTA 升级。
// 获取方式： 我的固件 -> 固件ID
String BIN_ID = "0";
// ====================================================
// NTP时间客户端设置
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 8 * 3600); // 东八区
String currentTime = "N/A"; // 全局存储时间
unsigned long lastTimeUpdate = 0;
bool timeInitialized = false;

// ==================全局对象===========================
// 屏幕显示对象
Face *face;

// 定时器对象
SimpleTimer timer;

/*** 线上地址 ***/
String domain = "http://api.espai2.fun/";
String yw_ws_ip = "api.espai2.fun";
String yw_ws_path = "";
int yw_ws_port = 80;
ESP_AI_server_config server_config = {"http", "node.espai.fun", 80, "api_key=9e28d2bce8ae4820b1da3a1f3439112e"};
ESP_AI esp_ai;
WebSocketsClient webSocket_yw;
bool is_yw_connected = true; // 业务服务是否已经连接
// 设备id
String device_id = "";
// 网络状态
String _net_status = "";
// 会话状态
String _session_status = "";

// 设备局域网IP
String _device_ip = "";
// 设备地址
String on_position_nation = "";
String on_position_province = "";
String on_position_city = "";

// 是否启用电量检测
String kwh_enable = "0";

// 情绪灯光灯珠数量
int emotion_led_number = 150;
// 情绪灯光
Adafruit_NeoPixel pixelsEmotion(emotion_led_number, EMOTION_LIGHT_PIN, NEO_GRB + NEO_KHZ800);

// OTA 检测升级过了，用于应付重新连接服务后不要重复去进行提示 OTA 升级
bool ota_ed = true;


// ========================= OTA升级逻辑 =========================
ESPOTAManager *otaManager;

// ========================= 错误监听 =========================
void onError(String code, String at_pos, String message)
{
    // some code...
    LOG_D("监听到错误：%s\n", code.c_str());
    String loc_ext7 = esp_ai.getLocalData("ext7");
    if (code == "006" && loc_ext7 != "")
    {
        // 清除板子信息，让板子重启，否则板子会不断重开热点无法操作
        esp_ai.setLocalData("wifi_name", "");
        esp_ai.setLocalData("wifi_pwd", "");
        esp_ai.setLocalData("ext1", "");
        ESP.restart();
    }
    if (message.indexOf("请充值") >= 0)
    {
#if !defined(IS_ESP_AI_S3_NO_SCREEN)
        face->SetChatMessage("额度卡不足，请充值。");
#endif
        play_builtin_audio(yu_e_bu_zu_mp3, yu_e_bu_zu_mp3_len);
        delay(5000);
    }

    if (message.indexOf("设备已解绑") >= 0)
    {
        // 清除 api_key 重启板子
        esp_ai.setLocalData("wifi_name", "");
        esp_ai.setLocalData("wifi_pwd", "");
        esp_ai.setLocalData("ext1", "");
        ESP.restart();
    }
}

// ========================= 获取网络时间 =========================
void updateTime() {
    static unsigned long retryInterval = 10000; // 初始10秒重试

    // 每分钟更新一次（失败时缩短为10秒重试）
    if (millis() - lastTimeUpdate >= retryInterval) {
        if (WiFi.status() == WL_CONNECTED) {
            if (timeClient.update()) {
                currentTime = timeClient.getFormattedTime().substring(0, 5); // HH:MM
                retryInterval = 60000; // 成功后恢复1分钟间隔
                Serial.println("时间更新: " + currentTime);
            } else {
                retryInterval = 10000; // 失败后10秒重试
                Serial.println("NTP更新失败，10秒后重试");
            }
        } else {
            Serial.println("WiFi未连接，跳过更新时间");
        }
        lastTimeUpdate = millis();
    }
}

// ========================= 会话状态监听 =========================
void on_command(String command_id, String data)
{
   if (command_id == "on_iat_cb")
    {
        face->ClearChatMessage();
        face->SetChatMessage(data);
    }
    else if (command_id == "on_llm_cb")
    {
        face->SetChatMessage(data);
    }
}

void on_session_status(String status)
{
    _session_status = status;
    if (otaManager->isUpdating()) {
        return;
    }

#if !defined(IS_ESP_AI_S3_NO_SCREEN)
    if (status == "iat_start") {
        face->SetEmotion("聆听中");
        face->ShowNotification("聆听中"); 
    } 
    else if (status == "iat_end") {
        face->SetEmotion("说话中");
        face->ShowNotification("说话中"); 
    } 
    else if (status == "tts_real_end") {
        face->SetEmotion("默认");     
        face->ShowNotification(currentTime);       
    } 

#endif
}

// ========================= 网络状态监听 =========================
void on_net_status(String status)
{
    // LOG_D("网络状态: " + status);
    _net_status = status;

    if (status == "0_ing")
    {
#if !defined(IS_ESP_AI_S3_NO_SCREEN)
        face->SetChatMessage("正在连接网络");
#endif
    };
    if (status == "0_ap")
    {
#if !defined(IS_ESP_AI_S3_NO_SCREEN)
        face->ShowNotification("配网模式");
        face->SetChatMessage("连接" + WiFi.softAPSSID() + "热点配网");
#endif
    };

    if (status == "2")
    {
#if !defined(IS_ESP_AI_S3_NO_SCREEN)
        face->SetChatMessage("正在连接服务");
#endif
    };

    // 已连接服务
    if (status == "3")
    {
#if !defined(IS_ESP_AI_S3_NO_SCREEN)
        // face->SetChatMessage("服务连接成功");
#endif
    };
}

void on_ready()
{
    // 检测升级
    if (!ota_ed)
    {
        ota_ed = true;
        String loc_api_key = esp_ai.getLocalData("ext1");
        auto_update(device_id, loc_api_key, BIN_ID, is_official, domain, _version, esp_ai, *otaManager, *face);
    }

    String ext3 = esp_ai.getLocalData("ext3");
    if (ext3 == "")
    {
        esp_ai.setLocalData("ext3", "1");
        esp_ai.awaitPlayerDone();
        esp_ai.stopSession();
        esp_ai.tts("设备激活成功，现在可以和我聊天了哦。");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        esp_ai.awaitPlayerDone();
    }
}

// ========================= 业务服务的 ws 连接 =========================
void webSocketEvent_ye(WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        LOG_D("业务ws服务已断开");
        is_yw_connected = false;

        // 玳瑁黄
        esp_ai_pixels.setPixelColor(0, esp_ai_pixels.Color(218, 164, 90));

        if (esp_ai_pixels.getBrightness() != 100)
            esp_ai_pixels.setBrightness(10); // 亮度设置
        else
            esp_ai_pixels.setBrightness(10); // 亮度设置
        esp_ai_pixels.show();
        break;
    case WStype_CONNECTED:
    {
        LOG_D("√ 业务ws服务已连接");
        is_yw_connected = true;
        // 上报设备信息
        // 指示灯状态应该由设备状态进行控制
        on_net_status(_net_status);
        break;
    }
    case WStype_TEXT:
    {
        JSONVar parseRes = JSON.parse((char *)payload);
        if (JSON.typeof(parseRes) == "undefined")
        {
            return;
        }
        if (parseRes.hasOwnProperty("type"))
        {
            String type = (const char *)parseRes["type"];
            if (type == "ota_update")
            {
                String url = (const char *)parseRes["url"];
                LOG_D("收到ota升级指令");
                // 初始化 OTA
                otaManager->init(device_id);
                otaManager->update(url);
            }
            else if (type == "stop_session")
            {
                LOG_D("业务服务要求停止会话");
                esp_ai.stopSession();
            }
            else if (type == "set_volume")
            {
                double volume = (double)parseRes["volume"];
                LOG_D("接收到音量：%d", volume);
                esp_ai.setVolume(volume);
                // 存储到本地
                esp_ai.setLocalData("ext2", String(volume, 1));
            }
            else if (type == "message")
            {
                String message = (const char *)parseRes["message"];
                LOG_D("收到服务提示：%d", message);
            }
            else if (type == "get_local_data")
            {
                LOG_D("收到 get_local_data 指令!");
                String loc_ext1 = esp_ai.getLocalData("ext1");

                // 上报服务器
                JSONVar local_data = esp_ai.getLocalAllData();

                JSONVar json_data;
                json_data["type"] = "get_local_data_res";
                json_data["data"] = local_data;
                json_data["device_id"] = device_id;
                json_data["api_key"] = loc_ext1;
                String send_data = JSON.stringify(json_data);
                webSocket_yw.sendTXT(send_data);
            }
            else if (type == "set_local_data")
            {
                JSONVar data = parseRes["data"];
                JSONVar keys = data.keys();
                LOG_D("收到 set_local_data 指令：");
                Serial.print(data);
                for (int i = 0; i < keys.length(); i++)
                {
                    String key = keys[i];
                    JSONVar value = data[key];
                    esp_ai.setLocalData(key, String((const char *)value));
                }
                vTaskDelay(pdMS_TO_TICKS(200));
                LOG_D("%s", "即将重启设备更新设置............");
                ESP.restart();
            }
        }
        break;
    }
    case WStype_ERROR:
        LOG_D("业务服务 WebSocket 连接错误");
        break;
    }
}

void webScoket_yw_main()
{
    String device_id = esp_ai.getLocalData("device_id");
    String ext1 = esp_ai.getLocalData("ext1");
    String ext2 = esp_ai.getLocalData("ext2");
    String ext3 = esp_ai.getLocalData("ext3");
    String ext4 = esp_ai.getLocalData("ext4");
    String ext5 = esp_ai.getLocalData("ext5");

    if (ext4 != "" && ext5 != "")
    {
        return;
    }

    LOG_D("开始连接业务 ws 服务");
    if (_net_status == "0_ap")
    {
        LOG_D("设备还未配网，忽略业务服务连接。");
        return;
    }
    // ws 服务
    webSocket_yw.begin(
        yw_ws_ip,
        yw_ws_port,
        yw_ws_path + "/connect_espai_node?device_type=hardware&device_id=" + device_id + "&version=" + _version + "&api_key=" + ext1 + "&ext2=" + ext2 + "&ext3=" + ext3 + "&ext4=" + ext4 + "&ext5=" + ext5);

    webSocket_yw.onEvent(webSocketEvent_ye);
    webSocket_yw.setReconnectInterval(3000);
    webSocket_yw.enableHeartbeat(10000, 5000, 0);
}

// ========================= 绑定设备 =========================
String encodeURIComponent(String input)
{
    String encoded = "";
    char hexBuffer[4]; // 用于存储百分比编码后的值

    for (int i = 0; i < input.length(); i++)
    {
        char c = input[i];

        // 检查是否为不需要编码的字符（字母、数字、-、_、.、~）
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += c; // 不需要编码的字符直接添加
        }
        else
        {
            // 对需要编码的字符进行百分比编码
            snprintf(hexBuffer, sizeof(hexBuffer), "%%%02X", c);
            encoded += hexBuffer;
        }
    }

    return encoded;
}

HTTPClient on_bind_device_http;

String on_bind_device(JSONVar data)
{
    LOG_D("\n[Info] -> 绑定设备中");
    String device_id = data["device_id"];
    String wifi_name = data["wifi_name"];
    String wifi_pwd = data["wifi_pwd"];
    String ext1 = data["ext1"];
    String ext4 = data["ext4"];
    String ext5 = data["ext5"];

    esp_ai_pixels.setBrightness(10); // 亮度设置
    esp_ai_pixels.show();

    // ext4 存在时是请求的自己的服务器，所以不去请求绑定服务
    // if (ext4.length() > 0 && ext5.length() > 0) {
    if (ext4 != "" && ext5 != "")
    {
#if !defined(IS_ESP_AI_S3_NO_SCREEN)
        face->SetChatMessage("设备激活成功");
#endif
        return "{\"success\":true,\"message\":\"设备激活成功。\"}";
    }

    String loc_api_key = esp_ai.getLocalData("ext1");
    String loc_ext7 = esp_ai.getLocalData("ext7");

    // 标识为临时数据
    String _ble_temp_ = esp_ai.getLocalData("_ble_temp_");

    // 如果 api_key 没有变化，那不应该去请求服务器绑定
    if (loc_api_key == ext1 && _ble_temp_ != "1")
    {
        LOG_D("\n[Info] -> 仅仅改变 wifi 等配置信息");
        return "{\"success\":true,\"message\":\"设备激活成功。\"}";
    }

    // 创建一个HTTP客户端对象
    LOG_D("[Info] ext4: %s", ext4.c_str());
    LOG_D("[Info] ext5: %s", ext5.c_str());
    LOG_D("[Info] api_key: %s", ext1.c_str());
    LOG_D("[Info] device_id: %s", device_id.c_str());
    // 如果已经连接了wifi了的话，这个应该为修改  --这个情况废弃
    String url = domain + "devices/add";
    on_bind_device_http.begin(url);
    on_bind_device_http.addHeader("Content-Type", "application/json");

    JSONVar json_params;
    json_params["version"] = _version;
    json_params["bin_id"] = BIN_ID;
    json_params["device_id"] = device_id;
    json_params["api_key"] = ext1;
    json_params["wifi_ssid"] = wifi_name;
    json_params["wifi_pwd"] = wifi_pwd;
    String send_data = JSON.stringify(json_params);

    // 发送POST请求并获取响应代码
    int httpCode = on_bind_device_http.POST(send_data);

    esp_ai_pixels.setBrightness(100); // 亮度设置
    esp_ai_pixels.show();

    if (httpCode > 0)
    {
        String payload = on_bind_device_http.getString();
        LOG_D("[HTTP] POST code: %d\n", httpCode);
        // LOG_D("[HTTP] POST res: %d\n", payload);
        JSONVar parse_res = JSON.parse(payload);

        if (JSON.typeof(parse_res) == "undefined" || String(httpCode) != "200")
        {
            on_bind_device_http.end();
            LOG_D("[Error HTTP] 请求网址: %s", url.c_str());

#if !defined(IS_ESP_AI_S3_NO_SCREEN)
            face->SetChatMessage("设备激活失败，请重试。");
#endif

            play_builtin_audio(bind_err_mp3, bind_err_mp3_len);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            esp_ai.awaitPlayerDone();
            vTaskDelay(1000 / portTICK_PERIOD_MS);

            // 这个 json 数据中的 message 会在配网页面弹出
            return "{\"success\":false,\"message\":\"设备绑定失败，错误码:" + String(httpCode) + "，重启设备试试呢。\"}";
        }

        if (parse_res.hasOwnProperty("success"))
        {
            bool success = (bool)parse_res["success"];
            String message = (const char *)parse_res["message"];
            String code = (const char *)parse_res["code"];
            if (success == false)
            {
#if !defined(IS_ESP_AI_S3_NO_SCREEN)
                face->SetChatMessage("绑定设备失败：" + message);
#endif
                // 绑定设备失败
                LOG_D("[Error] -> 绑定设备失败，错误信息：%s", message.c_str());

                // esp_ai.tts("绑定设备失败，重启设备试试呢，本次错误原因：" + message);
                on_bind_device_http.end();

                if (message.indexOf("不可以重复绑定") != -1)
                {
                    play_builtin_audio(chong_fu_bang_ding_mp3, chong_fu_bang_ding_mp3_len);
                }
                else
                {
                    play_builtin_audio(bind_err_mp3, bind_err_mp3_len);
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
                esp_ai.awaitPlayerDone();

                // 这个 json 数据中的 message 会在配网页面弹出
                return "{\"success\":false,\"message\":\"绑定设备失败，错误原因：" + message + "\"}";
            }
            else
            {
#if !defined(IS_ESP_AI_S3_NO_SCREEN)
                face->SetChatMessage("设备激活成功！");
#endif
                // 设备激活成功！
                LOG_D("[Info] -> 设备激活成功！");
                on_bind_device_http.end();
                // 这个 json 数据中的 message 会在配网页面弹出
                return "{\"success\":true,\"message\":\"设备激活成功，即将重启设备。\"}";
            }
        }
        else
        {
#if !defined(IS_ESP_AI_S3_NO_SCREEN)
            face->SetChatMessage("设备激活失败，请求服务失败！");
#endif
            LOG_D("[Error HTTP] 请求网址: %s", url.c_str());
            on_bind_device_http.end();
            return "{\"success\":false,\"message\":\"设备绑定失败，错误码:" + String(httpCode) + "，重启设备试试呢。\"}";
        }
    }
    else
    {
        LOG_D("[Error HTTP] 绑定设备失败: %d", httpCode);
        LOG_D("[Error HTTP] 请求网址: %s", url.c_str());
        on_bind_device_http.end();
        return "{\"success\":false,\"message\":\"请求服务失败，请刷新页面重试。\"}";
    }
}

void on_position(String ip, String nation, String province, String city, String latitude, String longitude)
{

    while (is_yw_connected == false)
    {
        delay(300);
    }

    String device_id = esp_ai.getLocalData("device_id");
    String loc_ext1 = esp_ai.getLocalData("ext1");
    String loc_wifi_name = esp_ai.getLocalData("wifi_name");
    String loc_wifi_pwd = esp_ai.getLocalData("wifi_pwd");

    JSONVar json_params;
    json_params["api_key"] = loc_ext1;
    json_params["version"] = _version;
    json_params["bin_id"] = BIN_ID;
    json_params["device_id"] = device_id;
    json_params["wifi_ssid"] = loc_wifi_name;
    json_params["wifi_pwd"] = loc_wifi_pwd;
    // 本地ip
    json_params["net_ip"] = _device_ip;
    // 设备地址
    if (nation)
    {
        json_params["nation"] = nation;
    }
    if (province)
    {
        json_params["province"] = province;
    }
    if (city)
    {
        json_params["city"] = city;
    }
    if (latitude)
    {
        json_params["latitude"] = latitude;
    }
    if (longitude)
    {
        json_params["longitude"] = longitude;
    }

    JSONVar sendJson;
    sendJson["api_key"] = loc_ext1;
    sendJson["device_id"] = device_id;
    sendJson["type"] = "upload_device_info";
    sendJson["data"] = json_params;

    String send_data = JSON.stringify(sendJson);
    webSocket_yw.sendTXT(send_data);
}

void on_connected_wifi(String device_ip)
{
    _device_ip = device_ip;
}

#if !defined(IS_ESP_AI_S3_NO_SCREEN)
void faceTask(void *arg)
{
    while (true)
    {
        // OTA 升级时更新慢一点
        if (otaManager->isUpdating())
        {
            face->Update();
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        else
        {
            face->Update();
#if defined(IS_ESP_AI_S3_TFT)
            vTaskDelay(30 / portTICK_PERIOD_MS); // 多增加点时间
#endif
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}
#endif

long prve_emotion_time = 0;

static String prve_emotion = "无情绪";

void emotion_led_control(String &emotion)
{
    // 情绪灯光
#if defined(IS_ESP_AI_S3_OLED) || defined(IS_ESP_AI_S3_DOUBLE_OLED) || defined(IS_ESP_AI_S3_TFT) || defined(IS_ESP_AI_S3_NO_SCREEN)
    if (prve_emotion != emotion)
    {
        pixelsEmotion.clear();
        if (emotion == "快乐")
        {
            pixelsEmotion.fill(pixelsEmotion.Color(255, 255, 255), 0, emotion_led_number);
        }
        else if (emotion == "伤心")
        {
            pixelsEmotion.fill(pixelsEmotion.Color(255, 255, 0), 0, emotion_led_number);
        }
        else
        {
            // 全部作为无情绪处理：默认米黄色
            pixelsEmotion.fill(pixelsEmotion.Color(250, 138, 0), 0, emotion_led_number);
        }
        pixelsEmotion.show();
        prve_emotion = emotion;
    }
#endif
}

// 情绪检测
void onEmotion(String emotion)
{

    // Serial.print("情绪下发当前情绪：");
    // Serial.println(emotion);

#if !defined(IS_ESP_AI_S3_NO_SCREEN)
    face->SetEmotion(emotion);
#endif

    emotion_led_control(emotion);
}

void websocket_timer2(int arg)
{
    webSocket_yw.loop();
}

void update_check_timer3(int arg)
{
    if (otaManager->updateFailed())
    {
        // 10 没有反应就说明升级失败了
        LOG_D("OTA 升级失败");
        // start_update_ed = false;
        String device_id = esp_ai.getLocalData("device_id");
        // 将错误发往业务服务
        JSONVar ota_update_error;
        ota_update_error["type"] = "ota_update_error";
        ota_update_error["device_id"] = device_id;
        String sendData = JSON.stringify(ota_update_error);
        webSocket_yw.sendTXT(sendData);
    }
}

void espai_loop_timer5(int arg)
{
    if (otaManager->isUpdating() == false)
    {
        esp_ai.loop();
    }
}

void setup()
{

    Serial.begin(115200);
    delay(500);
    // timeClient.begin();

    String oled_type = "091";
    String oled_sck = esp_ai.getLocalData("oled_sck");
    String oled_sda = esp_ai.getLocalData("oled_sda");

    if (oled_sck == "")
    {
        oled_sck = "38";
    }
    if (oled_sda == "")
    {
        oled_sda = "39";
    }

#if defined(IS_ESP_AI_S3_OLED)
    face = new Face(4, "096", oled_sck.toInt(), oled_sda.toInt(), 2);
#endif

    otaManager = new ESPOTAManager(&webSocket_yw, &esp_ai, face);

    face->SetChatMessage("ESP-AI:" + _version + "  - 热点配网版 "); 

    xTaskCreate(faceTask, "faceTask", 1024 * 5, NULL, 1, NULL);


    String ext4 = esp_ai.getLocalData("ext4");
    String ext5 = esp_ai.getLocalData("ext5");
    String ext6 = esp_ai.getLocalData("ext6");
    String ext7 = esp_ai.getLocalData("ext7");
    String ext8 = esp_ai.getLocalData("diyServerParams");
    String volume_enable = esp_ai.getLocalData("volume_enable");
    String volume_pin = esp_ai.getLocalData("volume_pin");
    String vad_first = esp_ai.getLocalData("vad_first");
    String vad_course = esp_ai.getLocalData("vad_course");
    String mic_bck = esp_ai.getLocalData("mic_bck");
    String mic_ws = esp_ai.getLocalData("mic_ws");
    String mic_data = esp_ai.getLocalData("mic_data");
    String speaker_bck = esp_ai.getLocalData("speaker_bck");
    String speaker_ws = esp_ai.getLocalData("speaker_ws");
    String speaker_data = esp_ai.getLocalData("speaker_data");
    String lights_data = esp_ai.getLocalData("lights_data");
    kwh_enable = esp_ai.getLocalData("kwh_enable");
    String _emotion_led_number = esp_ai.getLocalData("emotion_led_number");
    if (_emotion_led_number != "")
    {
        emotion_led_number = _emotion_led_number.toInt();
    }

    if (kwh_enable == "")
    {
        kwh_enable = "1";
    }

    ESP_AI_volume_config volume_config = {volume_pin.toInt(), 4096, 1, volume_enable == "0" ? false : true};
    ESP_AI_lights_config lights_config = {18}; // esp32s3 开发板是 48
    ESP_AI_reset_btn_config reset_btn_config = {};

    LOG_D("%s", "===============================");
    LOG_D("\n当前设备版本:%s", _version.c_str());
    if (ext4 != "" && ext5 != "")
    {
        LOG_D("你配置了服务IP, 所以将会请求你的服务！如果想要请求开放平台，请打开配网页面更改。");
        LOG_D("服务协议：%s", ext4.c_str());
        LOG_D("服务IP：%s", ext5.c_str());
        LOG_D("服务端口：%s", ext6.c_str());
    }
    LOG_D("%s", "===============================");

    // [必  填] 是否调试模式， 会输出更多信息
    bool debug = true;
    // [必  填] wifi 配置： { wifi 账号， wifi 密码, "热点名字" } 可不设置，连不上wifi时会打开热点：ESP-AI，连接wifi后打开地址： 192.168.4.1 进行配网(控制台会输出地址，或者在ap回调中也能拿到信息)
    ESP_AI_wifi_config wifi_config = {"WLAN", "192.168.0.1", "Fruina", html_str};




    // [必  填] 唤醒方案： { 方案, 语音唤醒用的阈值(本方案忽略即可), 引脚唤醒方案(本方案忽略), 发送的字符串 }
    ESP_AI_wake_up_config wake_up_config = {};
    if (ext7 == "pin_high")
    {
        wake_up_config.pin = 10;
        strcpy(wake_up_config.wake_up_scheme, "pin_high"); // 唤醒方案
    }
    else if (ext7 == "pin_low")
    {
        wake_up_config.pin = 10;
        strcpy(wake_up_config.wake_up_scheme, "pin_low"); // 唤醒方案
    }
    else if (ext7 == "boot")
    {
        wake_up_config.pin = 0;
        strcpy(wake_up_config.wake_up_scheme, "pin_low"); // 唤醒方案
    }
    else if (ext7 == "pin_high_listen")
    {
        wake_up_config.pin = 10;
        strcpy(wake_up_config.wake_up_scheme, "pin_high_listen"); // 唤醒方案
    }
    else if (ext7 == "boot_listen")
    {
        wake_up_config.pin = 0;
        strcpy(wake_up_config.wake_up_scheme, "pin_low_listen"); // 唤醒方案
    }
    else if (ext7 == "edge_impulse")
    {
        wake_up_config.threshold = 0.9;
        strcpy(wake_up_config.wake_up_scheme, "edge_impulse"); // 唤醒方案
    }
    else
    {
        // 默认用天问
        strcpy(wake_up_config.wake_up_scheme, "asrpro"); // 唤醒方案
        strcpy(wake_up_config.str, "start");             // 串口和天问asrpro 唤醒时需要配置的字符串，也就是从另一个开发版发送来的字符串
    }


    ESP_AI_i2s_config_mic i2s_config_mic = {mic_bck.toInt(), mic_ws.toInt(), mic_data.toInt()};
    ESP_AI_i2s_config_speaker i2s_config_speaker = {speaker_bck.toInt(), speaker_ws.toInt(), speaker_data.toInt()};

    if (lights_data != "")
    {
        lights_config.pin = lights_data.toInt();
    }

    if (ext4 != "" && ext5 != "")
    {
        strcpy(server_config.protocol, ext4.c_str());
        strcpy(server_config.ip, ext5.c_str());
        strcpy(server_config.params, ext8.c_str());
        server_config.port = ext6.toInt();
    }

    // 错误监听, 需要放到 begin 前面，否则可能监听不到一些数据
    esp_ai.onReady(on_ready);

    // 设备局域网IP
    esp_ai.onConnectedWifi(on_connected_wifi);
    // 设备网络状态监听, 需要放到 begin 前面，否则可能监听不到一些数据
    esp_ai.onNetStatus(on_net_status);
    // 上报位置, 需要放到 begin 前面
    esp_ai.onPosition(on_position);
    // 错误监听, 需要放到 begin 前面，否则可能监听不到一些数据
    esp_ai.onError(onError);
    // 绑定设备，蓝牙设备会在 begin 之前
    esp_ai.onBindDevice(on_bind_device);
    esp_ai.begin({debug, wifi_config, server_config, wake_up_config, volume_config, i2s_config_mic, i2s_config_speaker, reset_btn_config, lights_config});
    //状态监听
    esp_ai.onSessionStatus(on_session_status);
    //情绪检测
    esp_ai.onEmotion(onEmotion);
    //指令监听
    esp_ai.onEvent(on_command);

    device_id = esp_ai.getLocalData("device_id");

    // boot 按钮唤醒方式 esp-ai 库有问题，这begin()后再设置下就好了
    if (ext7 == "boot_listen" || ext7 == "boot")
    {
        pinMode(0, INPUT_PULLUP);
    }

    // 情绪灯光
    pixelsEmotion.begin();
    pixelsEmotion.setBrightness(100); // 亮度设置

    // 连接业务服务器
    webScoket_yw_main();

    // 定时器初始化
    timer.setInterval(1L, websocket_timer2, 0);
    timer.setInterval(1000L, update_check_timer3, 0);
    timer.setInterval(10L, espai_loop_timer5, 0);


}

void loop()
{
    updateTime(); // 持续更新时间
    timer.run();
    static unsigned long lastTest = 0;
    if (millis() - lastTest >= 5000) {
        on_session_status("tts_real_end");
        lastTest = millis();
    
}
}

long last_log_time = 0;

void log_fh()
{
    if (millis() - last_log_time > 3000)
    {
        last_log_time = millis();
    }
}