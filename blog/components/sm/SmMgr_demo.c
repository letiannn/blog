/**
 * @file SmMgr_demo.c
 * @brief TCP连接平台状态机示例
 */

#include "SmMgr.h"
#include <stdio.h>
#include "alog/alog.h"

/* ============================================================================
 * 状态ID定义
 * ============================================================================ */
enum
{
    STATE_DISCONNECTED = 0, /* 未连接 */
    STATE_CONNECTING,       /* 连接中 */
    STATE_CONNECTED,        /* 已连接 */
    STATE_AUTHENTICATING,   /* 认证中 */
    STATE_AUTHENTICATED,    /* 已认证 */
    STATE_RECONNECTING,     /* 重连中 */
    STATE_ERROR,            /* 错误状态 */
    STATE_MAX
};

/* ============================================================================
 * 事件ID定义
 * ============================================================================ */
enum
{
    EVT_CONNECT = 0,   /* 发起连接 */
    EVT_CONNECT_OK,    /* 连接成功 */
    EVT_CONNECT_FAIL,  /* 连接失败 */
    EVT_DISCONNECT,    /* 主动断开 */
    EVT_REMOTE_CLOSE,  /* 远端关闭 */
    EVT_SEND_AUTH,     /* 发送认证 */
    EVT_AUTH_OK,       /* 认证成功 */
    EVT_AUTH_FAIL,     /* 认证失败 */
    EVT_TIMEOUT,       /* 超时 */
    EVT_NETWORK_ERROR, /* 网络错误 */
    EVT_RECONNECT,     /* 开始重连 */
    EVT_MAX
};

/* ============================================================================
 * 用户数据结构体
 * ============================================================================ */
typedef struct
{
    int      socket_fd;           /* socket文件描述符 */
    uint32_t connect_retry_count; /* 连接重试次数 */
    uint32_t reconnect_count;     /* 重连次数 */
    uint32_t auth_retry_count;    /* 认证重试次数 */
    uint32_t last_error_code;     /* 最后错误码 */
    bool     need_reconnect;      /* 是否需要重连 */
    uint32_t keepalive_tick;      /* 保活计时器 */
    char     server_ip[64];       /* 服务器IP */
    uint16_t server_port;         /* 服务器端口 */
} TcpSessionData;

/* ============================================================================
 * 状态机实例定义(包含用户数据)
 * ============================================================================ */
typedef struct
{
    SmMachine      sm;
    TcpSessionData session_data;
} TcpSessionSm;

/* ============================================================================
 * 日志相关函数
 * ============================================================================ */

/**
 * @brief 获取事件名称
 */
static const char *GetEventName(SmEventId event_id)
{
    static const char *event_names[] = {
        "CONNECT", "CONNECT_OK", "CONNECT_FAIL", "DISCONNECT",
        "REMOTE_CLOSE", "SEND_AUTH", "AUTH_OK", "AUTH_FAIL",
        "TIMEOUT", "NETWORK_ERROR", "RECONNECT"
    };

    if (event_id >= 0 && event_id < (SmEventId)(sizeof(event_names) / sizeof(event_names[0])))
    {
        return event_names[event_id];
    }

    return "UNKNOWN";
}

/**
 * @brief 状态转换日志回调
 */
static void TransLogCallback(const char *class_name,
                             const char *from_state,
                             const char *to_state,
                             SmEventId   event_id,
                             const char *event_name)
{
    if (event_name != NULL)
    {
        ALOG_E("[TCP:%s] %s -> %s (event: %s)",
               class_name, from_state, to_state, event_name);
    }
    else
    {
        ALOG_E("[TCP:%s] %s -> %s (event_id: %d)",
               class_name, from_state, to_state, event_id);
    }
}

/* ============================================================================
 * 条件判断函数
 * ============================================================================ */

static bool CanRetryConnect(SmHandle handle, void *user_data)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;
    ALOG_E("[Condition] CanRetryConnect: retry_count=%d, max=5", data->connect_retry_count);
    return (data->connect_retry_count < 5); /* Max retry 5 times */
}

static bool CanRetryAuth(SmHandle handle, void *user_data)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;
    ALOG_E("[Condition] CanRetryAuth: auth_retry=%d, max=3", data->auth_retry_count);
    return (data->auth_retry_count < 3); /* Max retry auth 3 times */
}

static bool ShouldReconnect(SmHandle handle, void *user_data)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;
    ALOG_E("[Condition] ShouldReconnect: need_reconnect=%d, reconnect_count=%d, max=10",
           data->need_reconnect, data->reconnect_count);
    return (data->need_reconnect && data->reconnect_count < 10);
}

/* ============================================================================
 * 转换动作函数
 * ============================================================================ */

static SmRetCode OnConnectAction(SmHandle handle, void *user_data)
{
    TcpSessionData *data = (TcpSessionData *)user_data;
    ALOG_E("[Action] OnConnectAction: Initiate TCP connect %s:%d", data->server_ip, data->server_port);
    data->connect_retry_count++;
    return SM_RET_OK;
}

static SmRetCode OnDisconnectAction(SmHandle handle, void *user_data)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;
    ALOG_E("[Action] OnDisconnectAction: Close socket fd=%d", data->socket_fd);
    data->socket_fd = -1;
    data->connect_retry_count = 0;
    data->auth_retry_count = 0;
    return SM_RET_OK;
}

static SmRetCode OnSendAuthAction(SmHandle handle, void *user_data)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;
    ALOG_E("[Action] OnSendAuthAction: Send auth data socket=%d", data->socket_fd);
    data->auth_retry_count++;
    return SM_RET_OK;
}

static SmRetCode OnReconnectStartAction(SmHandle handle, void *user_data)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;
    ALOG_E("[Action] OnReconnectStartAction: Start reconnect reconnect_count=%d", data->reconnect_count);
    data->reconnect_count++;
    data->connect_retry_count = 0;
    data->auth_retry_count = 0;
    return SM_RET_OK;
}

/* ============================================================================
 * 状态进入/退出/处理函数
 * ============================================================================ */

/* DISCONNECTED state */
static SmRetCode Disconnected_OnEnter(SmHandle handle)
{
    TcpSessionSm *tcp_sm = (TcpSessionSm *)handle;
    ALOG_E("[State] Enter DISCONNECTED state");
    tcp_sm->session_data.socket_fd = -1;
    tcp_sm->session_data.need_reconnect = false;
    return SM_RET_OK;
}

static SmRetCode Disconnected_OnExit(SmHandle handle)
{
    ALOG_E("[State] Exit DISCONNECTED state");
    return SM_RET_OK;
}

static SmRetCode Disconnected_OnHandle(SmHandle handle, SmEventId event)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;

    if (event == EVT_CONNECT_OK)
    {
        ALOG_E("[State] DISCONNECTED received EVT_CONNECT_OK, save socket");
        data->socket_fd = 100; /* Simulate socket fd */
    }
    return SM_RET_OK;
}

/* CONNECTING state */
static SmRetCode Connecting_OnEnter(SmHandle handle)
{
    ALOG_E("[State] Enter CONNECTING state");
    return SM_RET_OK;
}

static SmRetCode Connecting_OnExit(SmHandle handle)
{
    ALOG_E("[State] Exit CONNECTING state");
    return SM_RET_OK;
}

static SmRetCode Connecting_OnHandle(SmHandle handle, SmEventId event)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;

    if (event == EVT_CONNECT_OK)
    {
        ALOG_E("[State] CONNECTING received EVT_CONNECT_OK, connect success!");
        data->socket_fd = 100;
    }
    else if (event == EVT_CONNECT_FAIL || event == EVT_TIMEOUT)
    {
        ALOG_E("[State] CONNECTING connect failed or timeout, retry_count=%d", data->connect_retry_count);
        data->last_error_code = (event == EVT_TIMEOUT) ? 1001 : 1000;
    }
    return SM_RET_OK;
}

/* CONNECTED state */
static SmRetCode Connected_OnEnter(SmHandle handle)
{
    TcpSessionSm *tcp_sm = (TcpSessionSm *)handle;
    ALOG_E("[State] Enter CONNECTED state, socket=%d", tcp_sm->session_data.socket_fd);
    tcp_sm->session_data.auth_retry_count = 0;
    return SM_RET_OK;
}

static SmRetCode Connected_OnExit(SmHandle handle)
{
    ALOG_E("[State] Exit CONNECTED state");
    return SM_RET_OK;
}

static SmRetCode Connected_OnHandle(SmHandle handle, SmEventId event)
{
    ALOG_E("[State] CONNECTED handle event: %d", event);
    return SM_RET_OK;
}

/* AUTHENTICATING state */
static SmRetCode Authenticating_OnEnter(SmHandle handle)
{
    ALOG_E("[State] Enter AUTHENTICATING state");
    return SM_RET_OK;
}

static SmRetCode Authenticating_OnExit(SmHandle handle)
{
    ALOG_E("[State] Exit AUTHENTICATING state");
    return SM_RET_OK;
}

static SmRetCode Authenticating_OnHandle(SmHandle handle, SmEventId event)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;

    if (event == EVT_AUTH_OK)
    {
        ALOG_E("[State] AUTHENTICATING auth success!");
    }
    else if (event == EVT_AUTH_FAIL || event == EVT_TIMEOUT)
    {
        ALOG_E("[State] AUTHENTICATING auth failed, auth_retry=%d", data->auth_retry_count);
        data->last_error_code = (event == EVT_TIMEOUT) ? 2001 : 2000;
    }
    return SM_RET_OK;
}

/* AUTHENTICATED state */
static SmRetCode Authenticated_OnEnter(SmHandle handle)
{
    TcpSessionSm *tcp_sm = (TcpSessionSm *)handle;
    ALOG_E("[State] Enter AUTHENTICATED state, socket=%d", tcp_sm->session_data.socket_fd);
    tcp_sm->session_data.reconnect_count = 0; /* Reset reconnect count after auth success */
    return SM_RET_OK;
}

static SmRetCode Authenticated_OnExit(SmHandle handle)
{
    ALOG_E("[State] Exit AUTHENTICATED state");
    return SM_RET_OK;
}

static SmRetCode Authenticated_OnHandle(SmHandle handle, SmEventId event)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;

    if (event == EVT_REMOTE_CLOSE || event == EVT_NETWORK_ERROR)
    {
        ALOG_E("[State] AUTHENTICATED received disconnect event, set need reconnect");
        data->need_reconnect = true;
        data->last_error_code = (event == EVT_REMOTE_CLOSE) ? 3000 : 3001;
    }
    else if (event == EVT_TIMEOUT)
    {
        /* Keepalive timeout */
        ALOG_E("[State] AUTHENTICATED keepalive timeout tick=%d", data->keepalive_tick);
    }
    return SM_RET_OK;
}

/* RECONNECTING state */
static SmRetCode Reconnecting_OnEnter(SmHandle handle)
{
    ALOG_E("[State] Enter RECONNECTING state");
    return SM_RET_OK;
}

static SmRetCode Reconnecting_OnExit(SmHandle handle)
{
    ALOG_E("[State] Exit RECONNECTING state");
    return SM_RET_OK;
}

static SmRetCode Reconnecting_OnHandle(SmHandle handle, SmEventId event)
{
    TcpSessionSm   *tcp_sm = (TcpSessionSm *)handle;
    TcpSessionData *data = &tcp_sm->session_data;

    if (event == EVT_CONNECT_OK)
    {
        ALOG_E("[State] RECONNECTING reconnect success!");
        data->socket_fd = 100;
    }
    else if (event == EVT_CONNECT_FAIL || event == EVT_TIMEOUT)
    {
        ALOG_E("[State] RECONNECTING reconnect failed, reconnect_count=%d", data->reconnect_count);
        data->last_error_code = (event == EVT_TIMEOUT) ? 4001 : 4000;
    }
    return SM_RET_OK;
}

/* ERROR state */
static SmRetCode Error_OnEnter(SmHandle handle)
{
    TcpSessionSm *tcp_sm = (TcpSessionSm *)handle;
    ALOG_E("[State] Enter ERROR state, error_code=0x%x", tcp_sm->session_data.last_error_code);
    return SM_RET_OK;
}

static SmRetCode Error_OnExit(SmHandle handle)
{
    ALOG_E("[State] Exit ERROR state");
    return SM_RET_OK;
}

static SmRetCode Error_OnHandle(SmHandle handle, SmEventId event)
{
    ALOG_E("[State] ERROR handle event: %d", event);
    return SM_RET_OK;
}

/* ============================================================================
 * State Transition Table Definition
 * ============================================================================ */

/* DISCONNECTED state transitions */
static SmTransition disconnected_transitions[] = {
    /* Initiate connection */
    SM_TRANS_ACTION(EVT_CONNECT, STATE_CONNECTING, OnConnectAction, NULL),

    /* End marker */
    SM_TRANS_END()
};

/* CONNECTING state transitions */
static SmTransition connecting_transitions[] = {
    /* Connect success -> Connected */
    SM_TRANS(EVT_CONNECT_OK, STATE_CONNECTED),

    /* Connect failed, retry if condition met */
    SM_TRANS_FULL(EVT_CONNECT_FAIL, STATE_CONNECTING, CanRetryConnect, OnConnectAction, NULL),

    /* Timeout, retry if condition met */
    SM_TRANS_FULL(EVT_TIMEOUT, STATE_CONNECTING, CanRetryConnect, OnConnectAction, NULL),

    /* Condition not met or network error -> Error state */
    SM_TRANS(EVT_NETWORK_ERROR, STATE_ERROR),
    SM_TRANS(EVT_DISCONNECT, STATE_DISCONNECTED),

    /* End marker */
    SM_TRANS_END()
};

/* CONNECTED state transitions */
static SmTransition connected_transitions[] = {
    /* Send auth */
    SM_TRANS_ACTION(EVT_SEND_AUTH, STATE_AUTHENTICATING, OnSendAuthAction, NULL),

    /* Active disconnect */
    SM_TRANS_ACTION(EVT_DISCONNECT, STATE_DISCONNECTED, OnDisconnectAction, NULL),

    /* Remote close or network error */
    SM_TRANS(EVT_REMOTE_CLOSE, STATE_ERROR),
    SM_TRANS(EVT_NETWORK_ERROR, STATE_ERROR),

    /* End marker */
    SM_TRANS_END()
};

/* AUTHENTICATING state transitions */
static SmTransition authenticating_transitions[] = {
    /* Auth success -> Authenticated */
    SM_TRANS(EVT_AUTH_OK, STATE_AUTHENTICATED),

    /* Auth failed, retry if condition met */
    SM_TRANS_FULL(EVT_AUTH_FAIL, STATE_AUTHENTICATING, CanRetryAuth, OnSendAuthAction, NULL),

    /* Timeout, retry if condition met */
    SM_TRANS_FULL(EVT_TIMEOUT, STATE_AUTHENTICATING, CanRetryAuth, OnSendAuthAction, NULL),

    /* Condition not met -> Error state */
    SM_TRANS(EVT_NETWORK_ERROR, STATE_ERROR),
    SM_TRANS(EVT_DISCONNECT, STATE_DISCONNECTED),

    /* 结束标记 */
    SM_TRANS_END()
};

/* AUTHENTICATED state transitions */
static SmTransition authenticated_transitions[] = {
    /* Remote close, reconnect if condition met */
    SM_TRANS_FULL(EVT_REMOTE_CLOSE, STATE_RECONNECTING, ShouldReconnect, OnReconnectStartAction, NULL),

    /* Network error, reconnect if condition met */
    SM_TRANS_FULL(EVT_NETWORK_ERROR, STATE_RECONNECTING, ShouldReconnect, OnReconnectStartAction, NULL),

    /* Active disconnect */
    SM_TRANS_ACTION(EVT_DISCONNECT, STATE_DISCONNECTED, OnDisconnectAction, NULL),

    /* End marker */
    SM_TRANS_END()
};

/* RECONNECTING state transitions */
static SmTransition reconnecting_transitions[] = {
    /* Reconnect success -> Connected */
    SM_TRANS(EVT_CONNECT_OK, STATE_CONNECTED),

    /* Reconnect failed, retry if condition met */
    SM_TRANS_FULL(EVT_CONNECT_FAIL, STATE_RECONNECTING, CanRetryConnect, OnConnectAction, NULL),

    /* Timeout, retry if condition met */
    SM_TRANS_FULL(EVT_TIMEOUT, STATE_RECONNECTING, CanRetryConnect, OnConnectAction, NULL),

    /* Condition not met or network error -> Error state */
    SM_TRANS(EVT_NETWORK_ERROR, STATE_ERROR),
    SM_TRANS(EVT_DISCONNECT, STATE_DISCONNECTED),

    /* End marker */
    SM_TRANS_END()
};

/* ERROR state transitions */
static SmTransition error_transitions[] = {
    /* Can reconnect from error state */
    SM_TRANS_ACTION(EVT_CONNECT, STATE_CONNECTING, OnConnectAction, NULL),

    /* End marker */
    SM_TRANS_END()
};

/* ============================================================================
 * State Table Definition
 * ============================================================================ */
static SmState tcp_states[] = {
    SM_STATE(STATE_DISCONNECTED, "DISCONNECTED", Disconnected_OnEnter, Disconnected_OnExit, Disconnected_OnHandle, disconnected_transitions),
    SM_STATE(STATE_CONNECTING, "CONNECTING", Connecting_OnEnter, Connecting_OnExit, Connecting_OnHandle, connecting_transitions),
    SM_STATE(STATE_CONNECTED, "CONNECTED", Connected_OnEnter, Connected_OnExit, Connected_OnHandle, connected_transitions),
    SM_STATE(STATE_AUTHENTICATING, "AUTHENTICATING", Authenticating_OnEnter, Authenticating_OnExit, Authenticating_OnHandle, authenticating_transitions),
    SM_STATE(STATE_AUTHENTICATED, "AUTHENTICATED", Authenticated_OnEnter, Authenticated_OnExit, Authenticated_OnHandle, authenticated_transitions),
    SM_STATE(STATE_RECONNECTING, "RECONNECTING", Reconnecting_OnEnter, Reconnecting_OnExit, Reconnecting_OnHandle, reconnecting_transitions),
    SM_STATE(STATE_ERROR, "ERROR", Error_OnEnter, Error_OnExit, Error_OnHandle, error_transitions),
};

/* ============================================================================
 * State Machine Class Definition
 * ============================================================================ */

static SmRetCode Tcp_OnInit(SmHandle handle)
{
    TcpSessionSm *tcp_sm = (TcpSessionSm *)handle;
    ALOG_E("[StateMachine] TCP session init");
    tcp_sm->session_data.socket_fd = -1;
    tcp_sm->session_data.connect_retry_count = 0;
    tcp_sm->session_data.reconnect_count = 0;
    tcp_sm->session_data.auth_retry_count = 0;
    tcp_sm->session_data.last_error_code = 0;
    tcp_sm->session_data.need_reconnect = false;
    tcp_sm->session_data.keepalive_tick = 0;
    sprintf(tcp_sm->session_data.server_ip, "192.168.1.100");
    tcp_sm->session_data.server_port = 8080;
    return SM_RET_OK;
}

static SmRetCode Tcp_OnDeinit(SmHandle handle)
{
    TcpSessionSm *tcp_sm = (TcpSessionSm *)handle;
    ALOG_E("[StateMachine] TCP session deinit, cleanup resources");
    if (tcp_sm->session_data.socket_fd >= 0)
    {
        ALOG_E("  Close socket fd=%d", tcp_sm->session_data.socket_fd);
    }
    return SM_RET_OK;
}

static const SmClass tcp_sm_class = SM_CLASS_DEF("TcpSessionSm", tcp_states, Tcp_OnInit, Tcp_OnDeinit);

/* ============================================================================
 * Demo主函数
 * ============================================================================ */

int demo(void)
{
    TcpSessionSm tcp_sm = { 0 };

    ALOG_E("========================================");
    ALOG_E("       TCP Connection Platform SM Demo");
    ALOG_E("========================================");

    /* 1. Create state machine instance */
    ALOG_E("[Step 1] Create TCP session state machine");
    if (SmCreate(&tcp_sm.sm, &tcp_sm_class, &tcp_sm.session_data) != SM_RET_OK)
    {
        ALOG_E("Create state machine failed!");
        return -1;
    }

    /* 1.1 Set transition log callback */
    ALOG_E("[Step 1.1] Set state transition log");
    SmSetTransLogFn(&tcp_sm.sm, TransLogCallback);
    SmSetGetEventNameFn(&tcp_sm.sm, GetEventName);

    /* 2. Start state machine */
    ALOG_E("[Step 2] Start state machine (initial state: DISCONNECTED)");
    SmStart(&tcp_sm.sm, STATE_DISCONNECTED);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    /* 3. Simulate connection flow - success scenario */
    ALOG_E("[Step 3] Simulate successful connection flow");
    ALOG_E("  -> Initiate connection");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    ALOG_E("  -> Connection success");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT_OK);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    ALOG_E("  -> Send auth");
    SmSendEvent(&tcp_sm.sm, EVT_SEND_AUTH);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    ALOG_E("  -> Auth success");
    SmSendEvent(&tcp_sm.sm, EVT_AUTH_OK);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    /* 4. Simulate reconnection scenario */
    ALOG_E("[Step 4] Simulate reconnection scenario");
    ALOG_E("  -> Reset state machine");
    SmStart(&tcp_sm.sm, STATE_DISCONNECTED);

    ALOG_E("  -> Initiate connection");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT);

    ALOG_E("  -> Connection success");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT_OK);

    ALOG_E("  -> Send auth");
    SmSendEvent(&tcp_sm.sm, EVT_SEND_AUTH);

    ALOG_E("  -> Auth success");
    SmSendEvent(&tcp_sm.sm, EVT_AUTH_OK);

    ALOG_E("  -> Simulate remote close");
    SmSendEvent(&tcp_sm.sm, EVT_REMOTE_CLOSE);
    ALOG_E("  Current state: %s (should enter RECONNECTING)", SmGetCurrentStateName(&tcp_sm.sm));

    ALOG_E("  -> Reconnect success");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT_OK);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    /* 5. Simulate connection retry scenario */
    ALOG_E("[Step 5] Simulate connection retry scenario");
    SmStart(&tcp_sm.sm, STATE_DISCONNECTED);

    ALOG_E("  -> Initiate connection");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT);

    ALOG_E("  -> Connection failed (retry 1)");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT_FAIL);

    ALOG_E("  -> Connection failed (retry 2)");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT_FAIL);

    ALOG_E("  -> Connection success");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT_OK);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    /* 6. Simulate auth retry scenario */
    ALOG_E("[Step 6] Simulate auth retry scenario");
    SmStart(&tcp_sm.sm, STATE_DISCONNECTED);

    SmSendEvent(&tcp_sm.sm, EVT_CONNECT);
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT_OK);
    SmSendEvent(&tcp_sm.sm, EVT_SEND_AUTH);

    ALOG_E("  -> Auth failed (retry 1)");
    SmSendEvent(&tcp_sm.sm, EVT_AUTH_FAIL);

    ALOG_E("  -> Auth failed (retry 2)");
    SmSendEvent(&tcp_sm.sm, EVT_AUTH_FAIL);

    ALOG_E("  -> Auth success");
    SmSendEvent(&tcp_sm.sm, EVT_AUTH_OK);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    /* 7. Simulate timeout entering error state */
    ALOG_E("[Step 7] Simulate multiple timeouts entering error state");
    SmStart(&tcp_sm.sm, STATE_DISCONNECTED);

    SmSendEvent(&tcp_sm.sm, EVT_CONNECT);

    ALOG_E("  -> Connection timeout (retry 1)");
    SmSendEvent(&tcp_sm.sm, EVT_TIMEOUT);

    ALOG_E("  -> Connection timeout (retry 2)");
    SmSendEvent(&tcp_sm.sm, EVT_TIMEOUT);

    ALOG_E("  -> Connection timeout (retry 3)");
    SmSendEvent(&tcp_sm.sm, EVT_TIMEOUT);

    ALOG_E("  -> Connection timeout (retry 4)");
    SmSendEvent(&tcp_sm.sm, EVT_TIMEOUT);

    ALOG_E("  -> Connection timeout (retry 5)");
    SmSendEvent(&tcp_sm.sm, EVT_TIMEOUT);

    ALOG_E("  -> Connection timeout (exceed max retry count)");
    SmSendEvent(&tcp_sm.sm, EVT_TIMEOUT);
    ALOG_E("  Current state: %s (should enter ERROR)", SmGetCurrentStateName(&tcp_sm.sm));

    /* 8. Recover from error state */
    ALOG_E("[Step 8] Reconnect from error state");
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT);
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT_OK);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    /* 9. Simulate active disconnect */
    ALOG_E("[Step 9] Simulate active disconnect");
    SmStart(&tcp_sm.sm, STATE_DISCONNECTED);
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT);
    SmSendEvent(&tcp_sm.sm, EVT_CONNECT_OK);
    SmSendEvent(&tcp_sm.sm, EVT_SEND_AUTH);
    SmSendEvent(&tcp_sm.sm, EVT_AUTH_OK);

    ALOG_E("  -> Active disconnect");
    SmSendEvent(&tcp_sm.sm, EVT_DISCONNECT);
    ALOG_E("  Current state: %s", SmGetCurrentStateName(&tcp_sm.sm));

    /* 10. Stop state machine */
    ALOG_E("[Step 10] Stop state machine");
    SmStop(&tcp_sm.sm);
    ALOG_E("Current state: %d (SM_STATE_INVALID=%d)", SmGetCurrentState(&tcp_sm.sm), SM_STATE_INVALID);

    /* 11. Destroy state machine */
    ALOG_E("[Step 11] Destroy state machine instance");
    SmDestroy(&tcp_sm.sm);

    ALOG_E("========================================");
    ALOG_E("       TCP Session Demo Complete");
    ALOG_E("========================================");

    return 0;
}

/* ============================================================================
 * Usage Instructions
 * ============================================================================ */
/*
 * TCP Connection Platform State Machine Description:
 *
 * State Definitions:
 *   - DISCONNECTED:  Not connected, waiting to initiate connection
 *   - CONNECTING:   Connecting, attempting to establish TCP connection
 *   - CONNECTED:    Connected, TCP connection established successfully
 *   - AUTHENTICATING: Authenticating, performing authentication
 *   - AUTHENTICATED:  Authenticated, authentication complete, normal business communication
 *   - RECONNECTING:   Reconnecting, attempting to reconnect after disconnection
 *   - ERROR:          Error state, retry count exceeded or other serious errors
 *
 * Event Definitions:
 *   - EVT_CONNECT:      Initiate connection request
 *   - EVT_CONNECT_OK:   Connection success
 *   - EVT_CONNECT_FAIL: Connection failed
 *   - EVT_DISCONNECT:   Active disconnect
 *   - EVT_REMOTE_CLOSE: Remote close connection
 *   - EVT_SEND_AUTH:    Send auth request
 *   - EVT_AUTH_OK:      Auth success
 *   - EVT_AUTH_FAIL:    Auth failed
 *   - EVT_TIMEOUT:      Operation timeout
 *   - EVT_NETWORK_ERROR: Network error
 *   - EVT_RECONNECT:    Start reconnect
 *
 * Retry Mechanism:
 *   - Connection retry: Max 5 times
 *   - Auth retry: Max 3 times
 *   - Reconnect count: Max 10 times
 *
 * Usage Steps:
 *   1. Define state IDs and event IDs (enum)
 *   2. Define user data structure (TcpSessionData)
 *   3. Implement condition check functions - optional
 *   4. Implement transition action functions - optional
 *   5. Implement state enter/exit/handle callback functions
 *   6. Define state transition table (use SM_TRANS related macros)
 *   7. Define state table (use SM_STATE macro)
 *   8. Define state machine class (use SM_CLASS_DEF macro)
 *   9. Create SmMachine instance (static allocation)
 *  10. Call SmCreate to initialize
 *  11. Call SmStart to start
 *  12. Call SmSendEvent to send events
 *  13. Call SmDestroy to cleanup
 */
