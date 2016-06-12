/*
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


#include <errno.h>
#include <fcntl.h>
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

#include <mosquitto.h>
#include <pthread.h>
#include "client_shared.h"

#define STATUS_CONNECTING 0
#define STATUS_CONNACK_RECVD 1
#define STATUS_WAITING 2

/* Global variables for use in callbacks. See sub_client.c for an example of
 * using a struct to hold variables for use in callbacks. */
static char *topic = NULL;
static char *message = NULL;
static long msglen = 0;
static int qos = 0;
static int retain = 0;
static int mode = MSGMODE_NONE;
static int status = STATUS_CONNECTING;
static int mid_sent = 0;
static int last_mid = -1;
static int last_mid_sent = -1;
static bool connected = true;
static char *username = NULL;
static char *password = NULL;
static bool disconnect_sent = false;
static bool quiet = false;
pthread_t tid_pub;
pthread_mutex_t mut;
int count=0;

void my_connect_callback_pub(struct mosquitto *mosq, void *obj, int result);
void my_disconnect_callback_pub(struct mosquitto *mosq, void *obj, int rc);
void my_publish_callback(struct mosquitto *mosq, void *obj, int mid);
void my_log_callback_pub(struct mosquitto *mosq, void *obj, int level, const char *str);
int load_stdin(void);
int load_file(const char *filename);
void print_usage_pub(void);
void publishToMaster(struct client_config *p_cfg);
void publishThread(struct client_config *c_cfg);
int setCount(int newCount);
