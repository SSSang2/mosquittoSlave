/*
s
Copyright (c) 2009-2014 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License v1.0
and Eclipse Distribution License v1.0 which accompany this distribution.

The Eclipse Public License is available at
http://www.eclipse.org/legal/epl-v10.html
and the Eclipse Distribution License is available at
http://www.eclipse.org/org/documents/edl-v10.php.

Contributors:
Roger Light - initial implementation and documentation.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <process.h>
#include <winsock2.h>
#define snprintf sprintf_s
#endif

#include <pthread.h>
#include <mosquitto.h>
#include "client_shared.h"

pthread_t tid_sub;
bool p_messages = true;
int m_count = 0;
void my_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
void my_connect_callback(struct mosquitto *mosq, void *obj, int result);
void my_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);
void my_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str);
void print_usage(void);
void subscribeFromMaster(struct client_config *s_cfg);
void subscribeThread(struct client_config *s_cfg);

