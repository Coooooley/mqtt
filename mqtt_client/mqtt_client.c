/**
 * @file mqtt_client.c
 * @version V1.0.0
 * @date 2022-05-10
 * @brief MQTT客户端, MQTT协议参考以下网址:
 *        https://mcxiaoke.gitbooks.io/mqtt-cn/content/
 *        https://www.runoob.com/manual/mqtt/protocol/MQTT-3.1.1-CN.html
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
static uint8_t gs_mqtt_rx_buffer[MQTT_RX_BUFFER_MAX_LEN] = {0};
static MqttParamStruct gsst_mqtt_param_data;
static uint16_t gs_unsubscribe_identifier = 1;
static uint16_t gs_subscribe_identifier = 1;
static uint16_t gs_publish_identifier = 0;

int g_sockfd = -1;

/********************************** Constant ********************************/

/********************************** Function ********************************/
static uint16_t mqtt_receive_data_parse(uint8_t *src_data, uint8_t *msg_data);
static void mqtt_receive_ack_code(uint8_t ack_type, uint8_t ack_code);
static void mqtt_fasync_callback_function(int signal);
static int socket_send_data(int fd, void *buffer, uint64_t len);
static int socket_deinit(int fd);
static int socket_init(void);


/**
 * @brief MQTT初始化和设置相关参数
 * 
 * @param param_data MQTT相关参数
 * @return 0: 成功; -1: 失败
 */
int mqtt_init(MqttParamStruct param_data)
{
    memset(gsst_mqtt_param_data.ipaddr, 0, sizeof(gsst_mqtt_param_data.ipaddr));
    memset(gsst_mqtt_param_data.client_id, 0, sizeof(gsst_mqtt_param_data.client_id));
    memset(gsst_mqtt_param_data.password, 0, sizeof(gsst_mqtt_param_data.password));
    memset(gsst_mqtt_param_data.user_name, 0, sizeof(gsst_mqtt_param_data.user_name));

    gsst_mqtt_param_data.port = param_data.port;
    gsst_mqtt_param_data.keep_alive = param_data.keep_alive;
    memcpy(gsst_mqtt_param_data.ipaddr, param_data.ipaddr, strlen(param_data.ipaddr));
    memcpy(gsst_mqtt_param_data.client_id, param_data.client_id, strlen(param_data.client_id));
    memcpy(gsst_mqtt_param_data.password, param_data.password, strlen(param_data.password));
    memcpy(gsst_mqtt_param_data.user_name, param_data.user_name, strlen(param_data.user_name));
    gsst_mqtt_param_data.mqtt_callback_function = param_data.mqtt_callback_function;

    /* 建立socket网络连接 */
    if (socket_init() < 0)
    {
        PRINT_LOG("mqtt socket init error");
        return -1;
    }
    else 
    {
        return 0;
    }
}

/**
 * @brief MQTT连接服务器
 * 
 */
void mqtt_connect(void)
{
    uint8_t flags = 0x00;
    uint8_t *packet = NULL;
    uint16_t packet_length = 0;
    uint16_t packet_offset = 0;
    uint8_t remain_length = 0;
    uint8_t *fixed_header = NULL;
    uint8_t fixed_header_size = 2;
    uint8_t variable_header[10] = {0};
    uint16_t clientid_length = strlen(gsst_mqtt_param_data.client_id);
    uint16_t username_length = strlen(gsst_mqtt_param_data.user_name);
    uint16_t password_length = strlen(gsst_mqtt_param_data.password);
    uint16_t payload_length = clientid_length + 2;

    /* 网络连接成功后, 第一个报文必须是CONNECT报文 */

    if (username_length)
    {
        payload_length += username_length + 2;
        flags |= MQTT_USERNAME_FLAG;
    }
    if (password_length)
    {
        payload_length += password_length + 2;
        flags |= MQTT_PASSWORD_FLAG;
    }
    flags |= MQTT_CLEAN_SESSION;

    remain_length = sizeof(variable_header) + payload_length;   //剩余长度 = 可变报头 + 有效载荷长度
    if (remain_length > 127)
    {
        fixed_header_size++;
    }
    /* 固定报头 */
    fixed_header = (uint8_t *)malloc(fixed_header_size);
    *fixed_header = MQTT_MSG_CONNECT;   //报文类型为connect
    if (remain_length <= 127)
    {
        *(fixed_header + 1) = remain_length;
    }
    else 
    {
        *(fixed_header + 1) = remain_length % 128;
        *(fixed_header + 1) = *(fixed_header + 1) | 0x80;
        *(fixed_header + 2) = remain_length / 128;
    }

    /* 可变报头 */
    variable_header[0] = 0x00;
    variable_header[1] = 0x04;          //协议名长度
    variable_header[2] = 0x4D;
    variable_header[3] = 0x51;
    variable_header[4] = 0x54;
    variable_header[5] = 0x54;          //协议名为"MQTT"
    variable_header[6] = 0x04;          //协议级别, 3.1.1版协议的协议级别字段的值为4(0x04)
    variable_header[7] = flags;         //连接标记
    variable_header[8] = (uint8_t)((gsst_mqtt_param_data.keep_alive >> 8) & 0xFF);
    variable_header[9] = (uint8_t)(gsst_mqtt_param_data.keep_alive & 0xFF);

    packet_length = fixed_header_size + sizeof(variable_header) + payload_length;
    packet = (uint8_t *)malloc(packet_length);
    memset(packet, 0, packet_length);

    /* 有效载荷: 客户端ID、遗嘱主题、遗嘱消息、用户名、密码, 遗嘱主题和遗嘱消息为可选, 此处未使用 */

    /* 填充报文数据包 */
    memcpy(packet, fixed_header, fixed_header_size);                                //填充固定报头
    packet_offset += fixed_header_size;
    memcpy(packet + packet_offset, variable_header, sizeof(variable_header));       //填充可变报头
    packet_offset += sizeof(variable_header);
    packet[packet_offset++] = (uint8_t)((clientid_length >> 8) & 0xFF);             //填充有效载荷
    packet[packet_offset++] = (uint8_t)(clientid_length & 0xFF);
    memcpy(packet + packet_offset, gsst_mqtt_param_data.client_id, clientid_length);
    packet_offset += clientid_length;
    packet[packet_offset++] = (uint8_t)((username_length >> 8) & 0xFF);
    packet[packet_offset++] = (uint8_t)(username_length & 0xFF);
    memcpy(packet + packet_offset, gsst_mqtt_param_data.user_name, username_length);
    packet_offset += username_length;
    packet[packet_offset++] = (uint8_t)((password_length >> 8) & 0xFF);
    packet[packet_offset++] = (uint8_t)(password_length & 0xFF);
    memcpy(packet + packet_offset, gsst_mqtt_param_data.password, password_length);
    packet_offset += password_length;

    /* 发送CONNECT报文数据包给服务器, 并等待服务器的CONNACK响应(异步通知) */
    if (socket_send_data(g_sockfd, packet, packet_length) < 0)
    {
        PRINT_LOG("mqtt send CONNECT packet error");
        free(fixed_header);
        free(packet);
        return;
    }
    free(fixed_header);
    free(packet);

    /* 使能异步通知, 也用于接收已订阅主题的消息 */
    signal(SIGIO, mqtt_fasync_callback_function);
    fcntl(g_sockfd, F_SETOWN, getpid());
    int flag = fcntl(g_sockfd, F_GETFL);
    fcntl(g_sockfd, F_SETFL, flag | FASYNC);
}

/**
 * @brief MQTT重新连接服务器
 * 
 * @return 0: 成功; -1: 失败
 */
int mqtt_reconnect(void)
{
    uint64_t ioctl_arg = 1;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(gsst_mqtt_param_data.port);
    server_addr.sin_addr.s_addr = inet_addr(gsst_mqtt_param_data.ipaddr);

    int ret = connect(g_sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
    if (ret < 0)
    {
        PRINT_LOG("mqtt socket reconnect server error");
        return -1;
    }
    else 
    {
        PRINT_LOG("mqtt socket reconnect server ok");
    }
    ioctl(g_sockfd, FIONBIO, &ioctl_arg);
    mqtt_connect();

    return 0;
}

/**
 * @brief MQTT断开连接
 * 
 */
void mqtt_disconnect(void)
{
    uint8_t packet[2] = {0};

    /* 报文数据包: 只包含固定报头 */
    packet[0] = MQTT_MSG_DISCONNECT;
    packet[1] = 0x00;

    /* 发送DISCONNECT报文数据包给服务器 */
    if (socket_send_data(g_sockfd, packet, sizeof(packet)) < 0)
    {
        PRINT_LOG("mqtt send DISCONNECT packet error");
        return;
    }

    socket_deinit(g_sockfd);
}

/**
 * @brief MQTT向某个主题发布消息
 * 
 * @param topic 主题
 * @param msg 消息
 * @param msg_len 消息长度
 * @param retain 保留位
 * @param qos QoS
 * @return -1: 失败; 0: 成功
 */
int mqtt_publish(const char *topic, const char *msg, uint16_t msg_len, uint8_t retain, uint8_t qos)
{
    uint8_t *packet = NULL;
    uint8_t *fixed_header = NULL;
    uint8_t *variable_header = NULL;
    uint8_t fixed_header_size = 2;
    uint8_t variable_header_size = 0;
    uint16_t packet_length = 0;
    uint16_t remain_length = 0;
    uint16_t topic_length = strlen(topic);
    uint8_t qos_flag = MQTT_QOS0_FLAG;
    uint8_t qos_size = 0;

    if (qos == 1)
    {
        qos_size = 2;
        qos_flag = MQTT_QOS1_FLAG;
    }
    else if (qos == 2)
    {
        qos_size = 2;
        qos_flag = MQTT_QOS2_FLAG;
    }

    variable_header_size = 2 + topic_length + qos_size;
    remain_length = variable_header_size + msg_len;         //剩余长度=可变报头的长度(10字节)+有效载荷的长度
    if (remain_length > 127)
    {
        fixed_header_size++;
    }

    /* 固定报头 */
    fixed_header = (uint8_t *)malloc(fixed_header_size);
    memset(fixed_header, 0, fixed_header_size);
    *fixed_header = MQTT_MSG_PUBLISH | qos_flag;            //报文类型为publish
    if (retain)
    {
        *fixed_header |= MQTT_RETAIN_FLAG;                  //若设置为1, 则服务端必须存储这个应用消息和它的服务质量等级(QoS)
    }
    if (remain_length <= 127)
    {
        *(fixed_header + 1) = remain_length;
    }
    else 
    {
        *(fixed_header + 1) = remain_length % 128;          //对于127字节以内的消息, 剩余的长度是1个字节, 大于127则用2个字节
        *(fixed_header + 1) = *(fixed_header + 1) | 0x80;   //剩余的长度可支持4个字节, 目前最大为2个字节, 即最大可处理16383(约16k)大小的消息
        *(fixed_header + 2) = remain_length / 128;
    }

    /* 可变报头 */
    variable_header = (uint8_t *)malloc(variable_header_size);
    memset(variable_header, 0, variable_header_size);
    *variable_header = (uint8_t)((topic_length >> 8) & 0xFF);
    *(variable_header + 1) = (uint8_t)(topic_length & 0xFF);
    memcpy(variable_header + 2, topic, topic_length);
    if (qos_size)
    {
        /* 只有当QoS等级是1或2时, 报文标识符(Packet Identifier)字段才能出现在PUBLISH报文中 */
        gs_publish_identifier++;
        if (gs_publish_identifier == 0)
        {
            gs_publish_identifier = 1;
        }
        variable_header[topic_length + 2] = (uint8_t)((gs_publish_identifier >> 8) & 0xFF);
        variable_header[topic_length + 3] = (uint8_t)(gs_publish_identifier & 0xFF);
    }

    /* 有效载荷: 包含将被发布的应用消息 */

    /* 填充报文数据包 */
    packet_length = fixed_header_size + variable_header_size + msg_len;
    packet = (uint8_t *)malloc(packet_length);
    memset(packet, 0, packet_length);
    memcpy(packet, fixed_header, fixed_header_size);
    memcpy(packet + fixed_header_size, variable_header, variable_header_size);
    memcpy(packet + fixed_header_size + variable_header_size, msg, msg_len);

    /* 发送PUBLISH报文数据包给服务器, 并等待服务器的PUBACK/PUBREC响应(异步通知), QoS=0时无响应 */
    if (socket_send_data(g_sockfd, packet, packet_length) < 0)
    {
        PRINT_LOG("mqtt send PUBLISH packet error");
        free(variable_header);
        free(fixed_header);
        free(packet);
        return -1;
    }

    free(variable_header);
    free(fixed_header);
    free(packet);
    return 0;
}

/**
 * @brief MQTT订阅主题
 * 
 * @param topic 主题
 * @param qos QoS
 */
void mqtt_subscribe(char *topic, uint8_t qos)
{
    uint16_t message_id = 0;
    uint8_t *packet = NULL;
    uint8_t *payload = NULL;
    uint8_t *fixed_header = NULL;
    uint8_t *variable_header = NULL;
    uint8_t payload_size = 0;
    uint8_t fixed_header_size = 2;
    uint8_t variable_header_size = 2;
    uint32_t topic_length = strlen(topic);
    uint32_t packet_length = 0;
    uint16_t remain_length = 0;

    remain_length = variable_header_size + 2 + topic_length + 1;    //剩余长度=(可变报头)报文标示符长度2+主题长度位占用2字节+主题长度+qos标识

    /* 固定报头 */
    fixed_header = (uint8_t *)malloc(fixed_header_size);
    memset(fixed_header, 0, fixed_header_size);
    *fixed_header = MQTT_MSG_SUBSCRIBE;                             //报文类型为subscribe
    *(fixed_header + 1) = remain_length;                            //剩余长度

    /* 可变报头 */
    message_id = gs_subscribe_identifier++;
    variable_header = (uint8_t *)malloc(variable_header_size);
    memset(variable_header, 0, variable_header_size);
    *variable_header = (uint8_t)((message_id >> 8) & 0xFF);         //标识符
    *(variable_header + 1) = (uint8_t)(message_id & 0xFF);

    /* 有效载荷 */
    payload_size = 2 + topic_length + 1;
    payload = (uint8_t *)malloc(payload_size);
    memset(payload, 0, payload_size);
    *payload = (uint8_t)((topic_length >> 8) & 0xFF);
    *(payload + 1) = (uint8_t)(topic_length & 0xFF);
    memcpy(payload + 2, topic, topic_length);
    *(payload + 2 + topic_length) = qos;

    /* 填充报文数据包 */
    packet_length = fixed_header_size + variable_header_size + payload_size;
    packet = (uint8_t *)malloc(packet_length);
    memset(packet, 0, packet_length);
    memcpy(packet, fixed_header, fixed_header_size);
    memcpy(packet + fixed_header_size, variable_header, variable_header_size);
    memcpy(packet + fixed_header_size + variable_header_size, payload, payload_size);

    /* 发送SUBSCRIBE报文数据包给服务器, 并等待服务器的SUBACK响应(异步通知) */
    if (socket_send_data(g_sockfd, packet, packet_length) < 0)
    {
        PRINT_LOG("mqtt send SUBSCRIBE packet error");
        free(variable_header);
        free(fixed_header);
        free(payload);
        free(packet);
        return;
    }

    free(variable_header);
    free(fixed_header);
    free(payload);
    free(packet);
}

/**
 * @brief MQTT取消订阅主题
 * 
 * @param topic 主题
 */
void mqtt_unsubscribe(char *topic)
{
    uint16_t message_id = 0;
    uint8_t *packet = NULL;
    uint8_t *payload = NULL;
    uint8_t *fixed_header = NULL;
    uint8_t *variable_header = NULL;
    uint8_t payload_size = 0;
    uint8_t fixed_header_size = 2;
    uint8_t variable_header_size = 2;
    uint32_t topic_length = strlen(topic);
    uint32_t packet_length = 0;
    uint16_t remain_length = 0;

    remain_length = variable_header_size + 2 + topic_length;        //剩余长度=(可变报头)报文标示符长度2+主题长度位占用2字节+主题长度

    /* 固定报头 */
    fixed_header = (uint8_t *)malloc(fixed_header_size);
    memset(fixed_header, 0, fixed_header_size);
    *fixed_header = MQTT_MSG_UNSUBSCRIBE;                           //报文类型为unsubscribe
    *(fixed_header + 1) = remain_length;                            //剩余长度

    /* 可变报头 */
    message_id = gs_unsubscribe_identifier++;
    variable_header = (uint8_t *)malloc(variable_header_size);
    memset(variable_header, 0, variable_header_size);
    *variable_header = (uint8_t)((message_id >> 8) & 0xFF);         //标识符
    *(variable_header + 1) = (uint8_t)(message_id & 0xFF);

    /* 有效载荷 */
    payload_size = 2 + topic_length;
    payload = (uint8_t *)malloc(payload_size);
    memset(payload, 0, payload_size);
    *payload = (uint8_t)((topic_length >> 8) & 0xFF);
    *(payload + 1) = (uint8_t)(topic_length & 0xFF);
    memcpy(payload + 2, topic, topic_length);

    /* 填充报文数据包 */
    packet_length = fixed_header_size + variable_header_size + payload_size;
    packet = (uint8_t *)malloc(packet_length);
    memset(packet, 0, packet_length);
    memcpy(packet, fixed_header, fixed_header_size);
    memcpy(packet + fixed_header_size, variable_header, variable_header_size);
    memcpy(packet + fixed_header_size + variable_header_size, payload, payload_size);

    /* 发送UNSUBSCRIBE报文数据包给服务器, 并等待服务器的UNSUBACK响应(异步通知) */
    if (socket_send_data(g_sockfd, packet, packet_length) < 0)
    {
        PRINT_LOG("mqtt send UNSUBSCRIBE packet error");
        free(variable_header);
        free(fixed_header);
        free(payload);
        free(packet);
        return;
    }

    free(variable_header);
    free(fixed_header);
    free(payload);
    free(packet);
}

/**
 * @brief MQTT心跳请求
 * 
 */
void mqtt_pingreq(void)
{
    uint8_t packet[2] = {0};

    /* 报文数据包: 只包含固定报头 */
    packet[0] = MQTT_MSG_PINGREQ;
    packet[1] = 0x00;

    /* 发送PINGREQ报文数据包给服务器, 并等待服务器的PINGRESP响应(异步通知) */
    if (socket_send_data(g_sockfd, packet, sizeof(packet)) < 0)
    {
        PRINT_LOG("mqtt send PINGREQ packet error");
        return;
    }
}

/**
 * @brief 异步通知回调函数
 * 
 * @param signal 
 */
static void mqtt_fasync_callback_function(int signal)
{
    (void)signal;
    uint16_t msg_len = 0;
    uint8_t temp_data[MQTT_RX_BUFFER_MAX_LEN] = {0};
    
    memset(gs_mqtt_rx_buffer, 0, MQTT_RX_BUFFER_MAX_LEN);

    if (read(g_sockfd, gs_mqtt_rx_buffer, MQTT_RX_BUFFER_MAX_LEN) < 0)
    {
        PRINT_LOG("read mqtt ack eeror");
    }
    else 
    {
        switch (gs_mqtt_rx_buffer[0])
        {
            case MQTT_MSG_CONNACK:
                PRINT_LOG("receive mqtt CONNACK ack");
                mqtt_receive_ack_code(MQTT_MSG_CONNACK, gs_mqtt_rx_buffer[3]);
                break;

            case MQTT_MSG_PUBACK:
                PRINT_LOG("receive mqtt PUBACK ack");
                break;

            case MQTT_MSG_PUBREC:
                PRINT_LOG("receive mqtt PUBREC ack");
                break;

            case MQTT_MSG_SUBACK:
                PRINT_LOG("receive mqtt SUBACK ack");
                /* 若同时订阅多个主题, 响应码会一起返回, 目前只判断第一个主题的返回码 */
                mqtt_receive_ack_code(MQTT_MSG_SUBACK, gs_mqtt_rx_buffer[4]);
                break;

            case MQTT_MSG_UNSUBACK:
                PRINT_LOG("receive mqtt UNSUBACK ack");
                break;

            case MQTT_MSG_PINGRESP:
                PRINT_LOG("receive mqtt PINGRESP ack");
                break;

            default :
                msg_len = mqtt_receive_data_parse(gs_mqtt_rx_buffer, temp_data);
                gsst_mqtt_param_data.mqtt_callback_function(temp_data, msg_len);
                break;
        }
    }
}

/**
 * @brief 对接收到已订阅主题的报文数据进行解析(报文非JSON数据)
 * 
 * @param src_data 原始报文数据
 * @param msg_data 解析后的消息数据
 * @return 消息数据的长度
 */
static uint16_t mqtt_receive_data_parse(uint8_t *src_data, uint8_t *msg_data)
{
	uint8_t digit = 0;
    uint16_t value = 0;
    uint16_t multiplier = 1;
    uint16_t msg_length = 0;
    uint8_t remain_bytes = 1;
    uint8_t src_data_offset = 0;
    uint8_t *temp_data = (uint8_t *)src_data;

    /* 报文类型 + 剩于长度(1-4, 可变) + 主题长度位(2) + 主题名数据 + QoS标识位(2, 可变, QoS=0时无) + 消息数据 */

    if ((*temp_data & 0xF0) == MQTT_MSG_PUBLISH)
    {
        /* 剩于长度字节数 */
        if ((*(temp_data + 1) & 0x80) == 0x80)
        {
            remain_bytes++;
            if ((*(temp_data + 2) & 0x80) == 0x80)
            {
                remain_bytes++;
                if ((*(temp_data + 3) & 0x80) == 0x80)
                {
                    remain_bytes++;
                }
            }
        }

        /* 主题名长度(2字节) */
        src_data_offset = (*(temp_data + 1 + remain_bytes)) << 8;
        src_data_offset |= *(temp_data + 1 + remain_bytes + 1);

        /* 固定报头+主题长度位(2) */
        src_data_offset += (1 + remain_bytes + 2);

        /* 判断QoS标识位 */
        if ((*temp_data & 0x06) >> 1)
        {
            src_data_offset += 2;
        }

        /* 消息数据长度 = 剩于长度 - 可变报头 (偏移 - 固定报头) */
        temp_data++;
        do 
        {
            digit = *temp_data;
            value += (digit & 127) * multiplier;
            multiplier *= 128;
            temp_data++;
        } while ((digit & 128) != 0);
        msg_length = value - (src_data_offset - (remain_bytes + 1));

        memcpy(msg_data, (src_data + src_data_offset), msg_length);
    }

    return msg_length;
}

/**
 * @brief MQTT接收响应返回码
 * 
 * @param ack_type 响应类型
 * @param ack_code 返回码
 */
static void mqtt_receive_ack_code(uint8_t ack_type, uint8_t ack_code)
{
    if (ack_type == MQTT_MSG_CONNACK)
    {
        switch (ack_code)
        {
            case 0x00:
                PRINT_LOG("The connection has been accepted by the server");
                break;

            case 0x01:
                PRINT_LOG("The server does not support the MQTT protocol level requested by the client");
                break;

            case 0x02:
                PRINT_LOG("The client identifier is the correct UTF-8 encoding, but is not allowed on the server");
                break;

            case 0x03:
                PRINT_LOG("Network connection established, but MQTT service unavailable");
                break;

            case 0x04:
                PRINT_LOG("The data format of the user name or password is invalid");
                break;

            case 0x05:
                PRINT_LOG("The client is not authorized to connect to this server, and check that the password and so on are correct");
                break;

            default :
                break;
        }
    }
    else if (ack_type == MQTT_MSG_SUBACK)
    {
        switch (ack_code)
        {
            case 0x00:
                PRINT_LOG("The maximum QoS 0");
                break;

            case 0x01:
                PRINT_LOG("The maximum QoS 1");
                break;

            case 0x02:
                PRINT_LOG("The maximum QoS 2");
                break;

            case 0x80:
                PRINT_LOG("mqtt SUBACK error");
                break;
            
            default:
                break;
        }
    }
}

/**
 * @brief socket初始化, 连接服务器
 * 
 * @return -1: 失败； 0: 成功 
 */
static int socket_init(void)
{
    int optval = 1;

    /* 创建socket */
    if ((g_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        PRINT_LOG("create socket error");
        return -1;
    }
    if (setsockopt(g_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
    {
        PRINT_LOG("setsockopt error");
        return -1;
    }

    /* 连接服务器 */
    uint64_t ioctl_arg = 1;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(gsst_mqtt_param_data.port);
    server_addr.sin_addr.s_addr = inet_addr(gsst_mqtt_param_data.ipaddr);

    int ret = connect(g_sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
    if (ret < 0)
    {
        PRINT_LOG("socket connect server error");
        return -1;
    }

    /* 设置成非阻塞 */
    ioctl(g_sockfd, FIONBIO, &ioctl_arg);

    return 0;
}

/**
 * @brief 关闭socket连接
 * 
 */
static int socket_deinit(int fd)
{
    return close(fd);
}

/**
 * @brief socket发送数据
 * 
 * @param fd 文件描述符
 * @param buffer 待发送的数据缓冲区
 * @param len 待发送的数据缓冲区的长度
 * @return -1: 失败； 其他: 成功
 */
static int socket_send_data(int fd, void *buffer, uint64_t len)
{
    int nwritten = 0;
    uint64_t nleft = len;
    uint8_t *bufp = (uint8_t *)buffer;

    while (nleft > 0) 
    {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) 
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                nwritten = 0;
            }
            else 
            {
                PRINT_LOG("%s", strerror(errno));
                return -1;
            }
        }
        nleft -= nwritten;
        bufp += nwritten;
    }

    return len;
}
