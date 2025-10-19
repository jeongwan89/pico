
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Centralized topic definitions for publish/subscribe
// User-specified topics: 8 sensor subscription topics (GH1..GH4 center Temp/Hum), 2 spare subs
// and 2 publish topics (Lastwill + spare)

// Publish topics
#define TOPIC_PUB_1 "Lastwill/Esp01Modem/Status"
#define TOPIC_PUB_2 "Lastwill/Esp01Modem/Spare"

// Subscription topics (8 sensor topics)
#define TOPIC_SUB_1 "Sensor/GH1/Center/Temp"
#define TOPIC_SUB_2 "Sensor/GH1/Center/Hum"
#define TOPIC_SUB_3 "Sensor/GH2/Center/Temp"
#define TOPIC_SUB_4 "Sensor/GH2/Center/Hum"
#define TOPIC_SUB_5 "Sensor/GH3/Center/Temp"
#define TOPIC_SUB_6 "Sensor/GH3/Center/Hum"
#define TOPIC_SUB_7 "Sensor/GH4/Center/Temp"
#define TOPIC_SUB_8 "Sensor/GH4/Center/Hum"

// Spare subscription topics
#define TOPIC_SUB_9 "Sensor/Spare/1"
#define TOPIC_SUB_10 "Sensor/Spare/2"

#ifdef __cplusplus
}
#endif
