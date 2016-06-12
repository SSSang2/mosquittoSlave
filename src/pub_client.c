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
static count = 0;
pthread_t tid_pub;
pthread_mutex_t mut;
char* _itoa(int a, char* buf){
		int tmp = a;
		char result[100];
		int i =0,j=0;
		int b =0;
		memset(result, NULL, sizeof(result));
		if(tmp==0)
				buf[i]='0';
		else{
				while(tmp>0){
						b = tmp % 10;
						result[i++] = (char)(b + '0');
						tmp /= 10;
				}

				for(j=0; j<i; j++){
						buf[j] = result[i-j-1];
				}
				buf[i] = '\0';
		}
		return buf;
}

void my_connect_callback_pub(struct mosquitto *mosq, void *obj, int result)
{
	int rc = MOSQ_ERR_SUCCESS;

	if(!result){
		switch(mode){
			case MSGMODE_CMD:
			case MSGMODE_FILE:
			case MSGMODE_STDIN_FILE:
				rc = mosquitto_publish(mosq, &mid_sent, topic, msglen, message, qos, retain);
				break;
			case MSGMODE_NULL:
				rc = mosquitto_publish(mosq, &mid_sent, topic, 0, NULL, qos, retain);
				break;
			case MSGMODE_STDIN_LINE:
				status = STATUS_CONNACK_RECVD;
				break;
		}
		if(rc){
			if(!quiet){
				switch(rc){
					case MOSQ_ERR_INVAL:
						fprintf(stderr, "Error: Invalid input. Does your topic contain '+' or '#'?\n");
						break;
					case MOSQ_ERR_NOMEM:
						fprintf(stderr, "Error: Out of memory when trying to publish message.\n");
						break;
					case MOSQ_ERR_NO_CONN:
						fprintf(stderr, "Error: Client not connected when trying to publish.\n");
						break;
					case MOSQ_ERR_PROTOCOL:
						fprintf(stderr, "Error: Protocol error when communicating with broker.\n");
						break;
					case MOSQ_ERR_PAYLOAD_SIZE:
						fprintf(stderr, "Error: Message payload is too large.\n");
						break;
				}
			}
			mosquitto_disconnect(mosq);
		}
	}else{
		if(result && !quiet){
			fprintf(stderr, "%s\n", mosquitto_connack_string(result));
		}
	}
}

void my_disconnect_callback_pub(struct mosquitto *mosq, void *obj, int rc)
{
	connected = false;
}

void my_publish_callback(struct mosquitto *mosq, void *obj, int mid)
{
	last_mid_sent = mid;
	if(mode == MSGMODE_STDIN_LINE){
		if(mid == last_mid){
			mosquitto_disconnect(mosq);
			disconnect_sent = true;
		}
	}else if(disconnect_sent == false){
		mosquitto_disconnect(mosq);
		disconnect_sent = true;
	}
}

void my_log_callback_pub(struct mosquitto *mosq, void *obj, int level, const char *str)
{
	printf("%s\n", str);
}

int load_stdin(void)
{
	long pos = 0, rlen;
	char buf[1024];
	char *aux_message = NULL;

	mode = MSGMODE_STDIN_FILE;

	while(!feof(stdin)){
		rlen = fread(buf, 1, 1024, stdin);
		aux_message = realloc(message, pos+rlen);
		if(!aux_message){
			if(!quiet) fprintf(stderr, "Error: Out of memory.\n");
			free(message);
			return 1;
		} else
		{
			message = aux_message;
		}
		memcpy(&(message[pos]), buf, rlen);
		pos += rlen;
	}
	msglen = pos;

	if(!msglen){
		if(!quiet) fprintf(stderr, "Error: Zero length input.\n");
		return 1;
	}

	return 0;
}

int load_file(const char *filename)
{
	long pos, rlen;
	FILE *fptr = NULL;

	fptr = fopen(filename, "rb");
	if(!fptr){
		if(!quiet) fprintf(stderr, "Error: Unable to open file \"%s\".\n", filename);
		return 1;
	}
	mode = MSGMODE_FILE;
	fseek(fptr, 0, SEEK_END);
	msglen = ftell(fptr);
	if(msglen > 268435455){
		fclose(fptr);
		if(!quiet) fprintf(stderr, "Error: File \"%s\" is too large (>268,435,455 bytes).\n", filename);
		return 1;
	}else if(msglen == 0){
		fclose(fptr);
		if(!quiet) fprintf(stderr, "Error: File \"%s\" is empty.\n", filename);
		return 1;
	}else if(msglen < 0){
		fclose(fptr);
		if(!quiet) fprintf(stderr, "Error: Unable to determine size of file \"%s\".\n", filename);
		return 1;
	}
	fseek(fptr, 0, SEEK_SET);
	message = malloc(msglen);
	if(!message){
		fclose(fptr);
		if(!quiet) fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}
	pos = 0;
	while(pos < msglen){
		rlen = fread(&(message[pos]), sizeof(char), msglen-pos, fptr);
		pos += rlen;
	}
	fclose(fptr);
	return 0;
}

void print_usage_pub(void)
{
		int major, minor, revision;
		mosquitto_lib_version(&major, &minor, &revision);
}

//int publishToMaster(char _host[] ,int _port, char _topic[], char _message[])
void publishToMaster(struct client_config *p_cfg)
{
		struct mosq_config cfg;
		struct mosquitto *mosq = NULL;
		int rc;
		int rc2;
		char *buf;
		int buf_len = 1024;
		int buf_len_actual;
		int read_len;
		int pos;
		char *temp[] = {"mosquitto_puba",};
		char msgs[256];
		char itoa[10];
		const char cfgId[10]; 
		buf = malloc(buf_len);
		if(!buf){
				fprintf(stderr, "Error: Out of memory.\n");
				return ;
		}
		memset(&cfg, 0, sizeof(struct mosq_config));


		rc = client_config_load(&cfg, CLIENT_PUB, 1, temp);
		if(rc){
				client_config_cleanup(&cfg);
				if(rc == 2){
						print_usage_pub();
				}else{
						fprintf(stderr, "\nUse 'mosquitto_pub --help' to see usage.\n");
				}
				return ;
		}
		sprintf(msgs, "Count : ");

		init_config(cfg);
		cfg.port = p_cfg->port;

		cfg.host = (char*)malloc(strlen(p_cfg->host));
		strcpy(cfg.host, p_cfg->host);

		cfg.topic = (char*)malloc(strlen(p_cfg->topic));
		strcpy(cfg.topic, p_cfg->topic);
		//cfg.topic = strdup(para[1]);
		cfg.message = (char*)malloc(strlen(msgs));
		strcpy(cfg.message, msgs);

		sprintf(cfgId, "master");
		cfg.id = (char*)malloc(strlen(cfgId));
		strcpy(cfg.id,cfgId);

		cfg.msglen = strlen(cfg.message);
		cfg.pub_mode = MSGMODE_STDIN_LINE;

		topic = cfg.topic;
		message = cfg.message;
		msglen = cfg.msglen;
		qos = cfg.qos;
		retain = cfg.retain;
		mode = cfg.pub_mode;
		username = cfg.username;
		password = cfg.password;
		quiet = cfg.quiet;


		if(cfg.pub_mode == MSGMODE_STDIN_FILE){
				if(load_stdin()){
						fprintf(stderr, "Error loading input from stdin.\n");
						return ;
				}
		}

		if(!topic || mode == MSGMODE_NONE){
				printf("a5\n");
				fprintf(stderr, "Error: Both topic and message must be supplied.\n");
				print_usage_pub();
				return ;
		}

		//mosquitto_lib_init();

		if(client_id_generate(&cfg, "Slave3")){
				return ;
		}

		//mosq = mosquitto_new(cfg.id, true, NULL);
		if(masterMosq == NULL)
				mosq = mosquitto_new("Slave3", true, NULL);
		else
				mosq  = masterMosq;
		//mosq->id = (char*)malloc(strlen(cfgId));
		mosq->id = strdup(cfgId);

		printf("#id : %s, %s\n", mosq->id, cfgId);

		if(!mosq){
				switch(errno){
						case ENOMEM:
								if(!quiet) fprintf(stderr, "Error: Out of memory.\n");
								break;
						case EINVAL:
								if(!quiet) fprintf(stderr, "Error: Invalid id.\n");
								break;
				}
				mosquitto_lib_cleanup();
				return ;
		}
		if(cfg.debug){
				mosquitto_log_callback_set(mosq, my_log_callback_pub);
		}
		mosquitto_connect_callback_set(mosq, my_connect_callback_pub);
		mosquitto_disconnect_callback_set(mosq, my_disconnect_callback_pub);
		mosquitto_publish_callback_set(mosq, my_publish_callback);

		/*	
			if(client_opts_set(mosq, &cfg)){
			return 1;
			}
		 */ 

		rc = client_connect(mosq, &cfg);
		if(rc) return rc;
		while(true){
				//_itoa(numOfclient, itoa);
				_itoa(count, itoa);
				//sprintf(message, "Count : ");
				sprintf(message, itoa);	
				//strcat(message, itoa);
				masterMosq = mosq;
				msglen = strlen(message);
				printf("		Publish Message to Master\n");
				printf("# num : %d\n", count);
				printf("# itoa : %s\n", itoa);
				printf("# msg : %s\n", message);
				printf("# mosq_id : %s, mosq_sock : %d\n", mosq->id, mosq->sock);
				printf("# masterMosq_id : %s, masterMosq_sock : %d\n", masterMosq->id, masterMosq->sock);

				//mosquitto_loop_start(mosq);
				/*	
					printf("===========================================\n");
					printf("-------------------------------------------\n");
					printf("		Publish Message to Master\n");
					printf("#Host		: %s\n", cfg.host);
					printf("#Port		: %d\n", cfg.port);
					printf("#topic		: %s\n", topic);
					printf("#message	: %s\n", message);
					printf("#client		: %d\n", numOfclient);
				//printf("#msglen		: %d\n", msglen);
				//printf("#qos		: %d\n", qos);
				//printf("#retain		: %d\n", retain);
				//printf("mode		: %d\n", mode);
				printf("-------------------------------------------\n");
				printf("===========================================\n");
				 */	
				message[msglen] = '\0';
				rc2 = mosquitto_publish(mosq, &mid_sent, topic, msglen, message, qos, retain);

				//mosquitto_loop_stop(mosq, false);
				usleep(5000000);
		}

		if(mode == MSGMODE_STDIN_LINE){
				mosquitto_loop_stop(mosq, false);
		}
		if(message && mode == MSGMODE_FILE){
				free(message);
		}
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();

		if(rc){
				fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		}
		return ;
}
void publishThread(struct client_config *c_cfg){
		printf("#HOST   : %s\n", c_cfg->host);
		printf("#PORT    : %d\n", c_cfg->port);
		printf("#TOPIC   : %s\n", c_cfg->topic);
		pthread_create(&tid_pub, NULL, (void *)publishToMaster, (void *)c_cfg);
}
void systemPush(struct mosquitto_db *db, char message[], struct mosquitto_msg_store *stored){
		uint16_t mid_sent =0;
		char *topic = "NAMU/SYSTEM/PUBLISH";
		int msglen = strlen(message);
		mid_sent = _mosquitto_mid_generate(masterMosq);
		printf("#systemPublish : %s, %s\n", topic, message);
		printf("#masterMosq->id : %s\n", masterMosq->id);
		printf("topic : %s\n", topic);

		mqtt3_db_message_store(db, masterMosq->id, mid_sent, topic, 0,msglen, message, 0, &stored, 0);
		mqtt3_db_messages_queue(db, masterMosq->id, topic, 0, 0, &stored);
		_mosquitto_send_publish(masterMosq, mid_sent, topic, msglen, message, 0, 0, false);
}
void systemPublish(struct mosquitto_db *db, char message[], struct mosquitto_msg_store *stored){
		uint16_t mid_sent =0;
		char topic[128];
		char msg[1024];
		char* ptr;
		int msglen=0; 

		ptr = strtok(message, ",");
		strcpy(topic, ptr);
		ptr = strtok(NULL, ",");
		strcpy(msg, ptr);
		msglen = strlen(msg);
		mid_sent = _mosquitto_mid_generate(masterMosq);

		printf("topic : %s\n", topic);
		printf("msg : %s\n", msg);

		mqtt3_db_message_store(db, masterMosq->id, mid_sent, topic, 0,msglen, msg, 0, &stored, 0);
		mqtt3_db_messages_queue(db, masterMosq->id, topic, 0, 0, &stored);
}
int setCount(int newCount){
	count = newCount;
}
