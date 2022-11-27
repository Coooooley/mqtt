/**
 * @file mqtt_client.c
 * @version V1.0.0
 * @date 2022-05-10
 * @brief MQTT client sample
 * 
 * @copyright 2018-2022 (C) Cooley Chan (https://github.com/Coooooley/mqtt)
 * 
 * @par 版本记录
 * 
 * 修改日期 | 版本 | 修改人 | 修改内容
 * -|-|-|-
 * 2022-05-10 | V1.0.0 | Cooley Chan | First edition
 * 
 */

#include "mqtt_client.h"

/****************************** Global Variable *****************************/
static MqttParamStruct gsst_mqtt_param;

/********************************** Constant ********************************/

/********************************** Function ********************************/
static void mqtt_data_process(uint8_t *msg_data, uint16_t msg_len);


/**
 * @brief main function
 * 
 * @return 0
 */
int main(void)
{
    char ipaddr[]    = {"xxx.xx.xxx.xx"};
    char client_id[] = {"mqtt_client_id"};
    char password[]  = {"ABCDEFGHIJK"};
    char user_name[] = {"mqtt"};

    memset(gsst_mqtt_param.ipaddr, 0, sizeof(gsst_mqtt_param.ipaddr));
    memset(gsst_mqtt_param.client_id, 0, sizeof(gsst_mqtt_param.client_id));
    memset(gsst_mqtt_param.password, 0, sizeof(gsst_mqtt_param.password));
    memset(gsst_mqtt_param.user_name, 0, sizeof(gsst_mqtt_param.user_name));

    gsst_mqtt_param.port = 1883;
    gsst_mqtt_param.keep_alive = 120;
    memcpy(gsst_mqtt_param.ipaddr, ipaddr, strlen(ipaddr));
    memcpy(gsst_mqtt_param.client_id, client_id, strlen(client_id));
    memcpy(gsst_mqtt_param.password, password, strlen(password));
    memcpy(gsst_mqtt_param.user_name, user_name, strlen(user_name));
    gsst_mqtt_param.mqtt_callback_function = mqtt_data_process;
    if (mqtt_init(gsst_mqtt_param) == 0)
    {
        mqtt_connect();

        char subscribe_name[128] = {0};
        sprintf(subscribe_name, "/xxxxxx/%s/", gsst_mqtt_param.client_id);
        mqtt_subscribe(subscribe_name, QOS_VALUE0);
    }
    else 
    {
        printf("mqtt init error, need to reinit or reconnect");
    }

    while (1)
    {
        char payload[8] = {0};
        char publish_name[128] = {0};

        for (uint8_t i = 0; i < 8; i++)
        {
            payload[i] = i;
        }
        sprintf(publish_name, "/yyyyyyy/%s/", gsst_mqtt_param.client_id);
        mqtt_publish(publish_name, payload, 8, 0, QOS_VALUE0);

        sleep(10);
    }

    return 0;
}

/**
 * @brief 接收并解析云服务器下发的MQTT数据
 * 
 * @param data MQTT数据
 */
static void mqtt_data_process(uint8_t *msg_data, uint16_t msg_len)
{
    printf("msg_len = %d", msg_len);

    for (uint16_t i = 0; i < msg_len; i++)
    {
        printf("data[%d] = 0x%x", i, msg_data[i]);
    }
}
