/**
 * @file mqtt_client.h
 * @version V1.0.0
 * @date 2022-05-10
 * @brief MQTT客户端, MQTT协议参考以下网址
 *        https://mcxiaoke.gitbooks.io/mqtt-cn/content/
 *        https://www.runoob.com/manual/mqtt/protocol/MQTT-3.1.1-CN.html
 * 
 * @copyright 2018-2022 (C) Cooley Chan (https://github.com/Coooooley/mqtt)
 * 
 * @par 版本记录
 * 
 * 修改日期 | 版本 | 修改人 | 修改内容
 * -|-|-|-
 * 2022-05-10 | V1.0.0 | Cooley Chen | First edition
 * 
 */

#ifndef MQTT_CLIENT_H_
#define MQTT_CLIENT_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <getopt.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/********************************** Typedef *********************************/
typedef void (*callback_function)(uint8_t *msg_data, uint16_t msg_len);

#pragma pack(1)
typedef struct 
{
    uint16_t keep_alive;
    uint16_t port;
    char ipaddr[32];
    char user_name[64];
    char client_id[64];
    char password[64];
    callback_function mqtt_callback_function;
} MqttParamStruct;
#pragma pack()

/*********************************** Macro **********************************/
/* QoS消息服务质量 */
#define QOS_VALUE0                      0
#define QOS_VALUE1                      1
#define QOS_VALUE2                      2

/* MQTT消息类型 */
#define MQTT_MSG_CONNECT                0x10
#define MQTT_MSG_CONNACK                0x20
#define MQTT_MSG_PUBLISH                0x30
#define MQTT_MSG_PUBACK                 0x40
#define MQTT_MSG_PUBREC                 0x50
#define MQTT_MSG_PUBREL                 0x60
#define MQTT_MSG_PUBCOMP                0x70
#define MQTT_MSG_SUBSCRIBE              (0x80 | 0x02)
#define MQTT_MSG_SUBACK                 0x90
#define MQTT_MSG_UNSUBSCRIBE            (0xA0 | 0x02)
#define MQTT_MSG_UNSUBACK               0xB0
#define MQTT_MSG_PINGREQ                0xC0
#define MQTT_MSG_PINGRESP               0xD0
#define MQTT_MSG_DISCONNECT             0xE0

/* 连接标志 */
#define MQTT_CLEAN_SESSION              (1 << 1)
#define MQTT_WILL_FLAG                  (1 << 2)
#define MQTT_WILL_RETAIN                (1 << 5)
#define MQTT_PASSWORD_FLAG              (1 << 6)
#define MQTT_USERNAME_FLAG              (1 << 7)

#define MQTT_DUP_FLAG                   (1 << 3)
#define MQTT_QOS0_FLAG                  (0 << 1)
#define MQTT_QOS1_FLAG                  (1 << 1)
#define MQTT_QOS2_FLAG                  (2 << 1)
#define MQTT_RETAIN_FLAG                1

#define MQTT_RX_BUFFER_MAX_LEN          1024

/********************************** Function ********************************/
int mqtt_init(MqttParamStruct param_data);

void mqtt_connect(void);
int mqtt_reconnect(void);
void mqtt_disconnect(void);
void mqtt_subscribe(char *topic, uint8_t qos);
void mqtt_unsubscribe(char *topic);
void mqtt_pingreq(void);
int mqtt_publish(const char *topic, const char *msg, uint16_t msg_len, uint8_t retain, uint8_t qos);


#endif /* MQTT_CLIENT_H_ */