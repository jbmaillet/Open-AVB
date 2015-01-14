/******************************************************************************

  Copyright (c) 2012, Intel Corporation
  Copyright (c) 2014, Parrot SA
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include "mrpdclient.h"
#include "talker_mrp_client.h"

/* global variables */

volatile int halt_tx = 0;
volatile int listeners = 0;
volatile int mrp_okay;
volatile int mrp_error = 0;;

volatile int domain_a_valid = 0;
int domain_class_a_id = 0;
int domain_class_a_priority = 0;
u_int16_t domain_class_a_vid = 0;

volatile int domain_b_valid = 0;
int domain_class_b_id = 0;
int domain_class_b_priority = 0;
u_int16_t domain_class_b_vid = 0;

pthread_t monitor_thread;
pthread_attr_t monitor_attr;
unsigned char monitor_stream_id[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

static int process_mrp_msg(char *buf, int buflen)
{

	/*
	 * 1st character indicates application
	 * [MVS] - MAC, VLAN or STREAM
	 */
	unsigned int id;
	unsigned int priority;
	unsigned int vid;
	int i, j, k;
	unsigned int substate;
	unsigned char recovered_streamid[8];
	k = 0;
 next_line:if (k >= buflen)
		return 0;
	switch (buf[k]) {
	case 'E':
		printf("%s from mrpd\n", buf);
		fflush(stdout);
		mrp_error = 1;
		break;
	case 'O':
		mrp_okay = 1;
		break;
	case 'M':
	case 'V':
		printf("%s unhandled from mrpd\n", buf);
		fflush(stdout);

		/* unhandled for now */
		break;
	case 'L':

		/* parse a listener attribute - see if it matches our monitor_stream_id */
		i = k;
		while (buf[i] != 'D')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%d", &substate);
		while (buf[i] != 'S')
			i++;
		i += 2;		/* skip the ':' */
		for (j = 0; j < 8; j++) {
			sscanf(&(buf[i + 2 * j]), "%02x", &id);
			recovered_streamid[j] = (unsigned char)id;
		} printf
		    ("FOUND STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
		     recovered_streamid[0], recovered_streamid[1],
		     recovered_streamid[2], recovered_streamid[3],
		     recovered_streamid[4], recovered_streamid[5],
		     recovered_streamid[6], recovered_streamid[7]);
		switch (substate) {
		case 0:
			printf("with state ignore\n");
			break;
		case 1:
			printf("with state askfailed\n");
			break;
		case 2:
			printf("with state ready\n");
			break;
		case 3:
			printf("with state readyfail\n");
			break;
		default:
			printf("with state UNKNOWN (%d)\n", substate);
			break;
		}
		if (substate > MSRP_LISTENER_ASKFAILED) {
			if (memcmp
			    (recovered_streamid, monitor_stream_id,
			     sizeof(recovered_streamid)) == 0) {
				listeners = 1;
				printf("added listener\n");
			}
		}
		fflush(stdout);

		/* try to find a newline ... */
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if (i == buflen)
			return 0;
		if (buf[i] == '\0')
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'D':
		i = k + 4;

		/* save the domain attribute */
		sscanf(&(buf[i]), "%d", &id);
		while (buf[i] != 'P')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%d", &priority);
		while (buf[i] != 'V')
			i++;
		i += 2;		/* skip the ':' */
		sscanf(&(buf[i]), "%x", &vid);
		if (id == 6) {
			domain_class_a_id = id;
			domain_class_a_priority = priority;
			domain_class_a_vid = vid;
			domain_a_valid = 1;
		} else {
			domain_class_b_id = id;
			domain_class_b_priority = priority;
			domain_class_b_vid = vid;
			domain_b_valid = 1;
		}
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if ((i == buflen) || (buf[i] == '\0'))
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'T':

		/* as simple_talker we don't care about other talkers */
		i = k;
		while ((i < buflen) && (buf[i] != '\n') && (buf[i] != '\0'))
			i++;
		if (i == buflen)
			return 0;
		if (buf[i] == '\0')
			return 0;
		i++;
		k = i;
		goto next_line;
		break;
	case 'S':

		/* handle the leave/join events */
		switch (buf[k + 4]) {
		case 'L':
			i = k + 5;
			while (buf[i] != 'D')
				i++;
			i += 2;	/* skip the ':' */
			sscanf(&(buf[i]), "%d", &substate);
			while (buf[i] != 'S')
				i++;
			i += 2;	/* skip the ':' */
			for (j = 0; j < 8; j++) {
				sscanf(&(buf[i + 2 * j]), "%02x", &id);
				recovered_streamid[j] = (unsigned char)id;
			} printf
			    ("EVENT on STREAM ID=%02x%02x%02x%02x%02x%02x%02x%02x ",
			     recovered_streamid[0], recovered_streamid[1],
			     recovered_streamid[2], recovered_streamid[3],
			     recovered_streamid[4], recovered_streamid[5],
			     recovered_streamid[6], recovered_streamid[7]);
			switch (substate) {
			case 0:
				printf("with state ignore\n");
				break;
			case 1:
				printf("with state askfailed\n");
				break;
			case 2:
				printf("with state ready\n");
				break;
			case 3:
				printf("with state readyfail\n");
				break;
			default:
				printf("with state UNKNOWN (%d)\n", substate);
				break;
			}
			switch (buf[k + 1]) {
			case 'L':
				printf("got a leave indication\n");
				if (memcmp
				    (recovered_streamid, monitor_stream_id,
				     sizeof(recovered_streamid)) == 0) {
					listeners = 0;
					printf("listener left\n");
				}
				break;
			case 'J':
			case 'N':
				printf("got a new/join indication\n");
				if (substate > MSRP_LISTENER_ASKFAILED) {
					if (memcmp
					    (recovered_streamid,
					     monitor_stream_id,
					     sizeof(recovered_streamid)) == 0)
						listeners = 1;
				}
				break;
			}

			/* only care about listeners ... */
		default:
			return 0;
			break;
		}
		break;
	case '\0':
		break;
	}
	return 0;
}

static void *mrp_monitor_thread(void *mrpd_sock)
{
	char *msgbuf;
	struct sockaddr_in client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;
	struct pollfd fds;
	int rc;

	if (SOCKET_ERROR == (SOCKET)mrpd_sock)
		return NULL;

	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return NULL;

	while (!halt_tx) {
		fds.fd = (SOCKET)mrpd_sock;
		fds.events = POLLIN;
		fds.revents = 0;
		rc = poll(&fds, 1, 100);
		if (rc < 0) {
			free(msgbuf);
			pthread_exit(NULL);
		}
		if (rc == 0)
			continue;
		if ((fds.revents & POLLIN) == 0) {
			free(msgbuf);
			pthread_exit(NULL);
		}
		memset(&msg, 0, sizeof(msg));
		memset(&client_addr, 0, sizeof(client_addr));
		memset(msgbuf, 0, MAX_MRPD_CMDSZ);
		iov.iov_len = MAX_MRPD_CMDSZ;
		iov.iov_base = msgbuf;
		msg.msg_name = &client_addr;
		msg.msg_namelen = sizeof(client_addr);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		bytes = recvmsg((SOCKET)mrpd_sock, &msg, 0);
		if (bytes < 0)
			continue;
		process_mrp_msg(msgbuf, bytes);
	}
	free(msgbuf);
	pthread_exit(NULL);
}

/*
 * public
 */

int mrp_monitor(SOCKET mrpd_sock)
{
	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	pthread_attr_init(&monitor_attr);
	pthread_create(&monitor_thread, NULL, mrp_monitor_thread, (void *)mrpd_sock);

	return 0;
}

int mrp_register_domain(SOCKET mrpd_sock, int *class_id, int *priority, u_int16_t * vid)
{
	char *msgbuf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	msgbuf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);

	sprintf(msgbuf, "S+D:C=%d,P=%d,V=%04x", *class_id, *priority, *vid);
	mrp_okay = 0;
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (MRPDCLIENT_MAX_MSG_SIZE != rc)
		rc = -1;
	else
		rc = 0;
	free(msgbuf);

	return rc;
}

int mrp_join_vlan(SOCKET mrpd_sock)
{
	char *msgbuf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	msgbuf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);

	sprintf(msgbuf, "V++:I=0002");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (MRPDCLIENT_MAX_MSG_SIZE != rc)
		rc = -1;
	else
		rc = 0;
	free(msgbuf);

	return rc;
}

int
mrp_advertise_stream(SOCKET mrpd_sock,
		uint8_t * streamid,
		uint8_t * destaddr,
		u_int16_t vlan,
		int pktsz, int interval, int priority, int latency)
{
	char *msgbuf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	msgbuf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);

	sprintf(msgbuf, "S++:S=%02X%02X%02X%02X%02X%02X%02X%02X"
		",A=%02X%02X%02X%02X%02X%02X"
		",V=%04X"
		",Z=%d"
		",I=%d"
		",P=%d"
		",L=%d", streamid[0], streamid[1], streamid[2],
		streamid[3], streamid[4], streamid[5], streamid[6],
		streamid[7], destaddr[0], destaddr[1], destaddr[2],
		destaddr[3], destaddr[4], destaddr[5], vlan, pktsz,
		interval, priority << 5, latency);
	mrp_okay = 0;
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (MRPDCLIENT_MAX_MSG_SIZE != rc)
		rc = -1;
	else
		rc = 0;
	free(msgbuf);

	return rc;
}

int
mrp_unadvertise_stream(SOCKET mrpd_sock,
		uint8_t * streamid,
		uint8_t * destaddr,
		u_int16_t vlan,
		int pktsz, int interval, int priority, int latency)
{
	char *msgbuf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	msgbuf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);

	sprintf(msgbuf, "S--:S=%02X%02X%02X%02X%02X%02X%02X%02X"
		",A=%02X%02X%02X%02X%02X%02X"
		",V=%04X"
		",Z=%d"
		",I=%d"
		",P=%d"
		",L=%d", streamid[0], streamid[1], streamid[2],
		streamid[3], streamid[4], streamid[5], streamid[6],
		streamid[7], destaddr[0], destaddr[1], destaddr[2],
		destaddr[3], destaddr[4], destaddr[5], vlan, pktsz,
		interval, priority << 5, latency);
	mrp_okay = 0;
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (MRPDCLIENT_MAX_MSG_SIZE != rc)
		rc = -1;
	else
		rc = 0;
	free(msgbuf);

	return rc;
}


int mrp_await_listener(SOCKET mrpd_sock, unsigned char *streamid)
{
	char *msgbuf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	memcpy(monitor_stream_id, streamid, sizeof(monitor_stream_id));
	msgbuf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(msgbuf, "S??");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (MRPDCLIENT_MAX_MSG_SIZE != rc)
		rc = -1;
	else
		rc = 0;
	free(msgbuf);
	if (rc == -1)
		return -1;

	/* either already there ... or need to wait ... */
	while (!halt_tx && (listeners == 0))
		usleep(20000);

	return 0;
}

/*
 * actually not used
 */

int mrp_get_domain(SOCKET mrpd_sock,
		int *class_a_id, int *a_priority, u_int16_t * a_vid,
		int *class_b_id, int *b_priority, u_int16_t * b_vid)
{
	char *msgbuf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	/* we may not get a notification if we are joining late,
	 * so query for what is already there ...
	 */
	msgbuf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);

	sprintf(msgbuf, "S??");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (MRPDCLIENT_MAX_MSG_SIZE != rc)
		rc = -1;
	else
		rc = 0;
	free(msgbuf);
	if (rc == -1)
		return -1;

	while (!halt_tx && (domain_a_valid == 0) && (domain_b_valid == 0))
		usleep(20000);

	*class_a_id = 0;
	*a_priority = 0;
	*a_vid = 0;
	*class_b_id = 0;
	*b_priority = 0;
	*b_vid = 0;
	if (domain_a_valid) {
		*class_a_id = domain_class_a_id;
		*a_priority = domain_class_a_priority;
		*a_vid = domain_class_a_vid;
	}
	if (domain_b_valid) {
		*class_b_id = domain_class_b_id;
		*b_priority = domain_class_b_priority;
		*b_vid = domain_class_b_vid;
	}
	return 0;
}

int mrp_join_listener(SOCKET mrpd_sock, uint8_t * streamid)
{
	char *msgbuf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	msgbuf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, MRPDCLIENT_MAX_MSG_SIZE);

	sprintf(msgbuf, "S+L:S=%02X%02X%02X%02X%02X%02X%02X%02X"
		",D=2", streamid[0], streamid[1], streamid[2], streamid[3],
		streamid[4], streamid[5], streamid[6], streamid[7]);
	mrp_okay = 0;
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, MRPDCLIENT_MAX_MSG_SIZE);
	if (MRPDCLIENT_MAX_MSG_SIZE != rc)
		rc = -1;
	else
		rc = 0;
	free(msgbuf);

	return rc;
}

// TODO remove
int recv_mrp_okay()
{
	while ((mrp_okay == 0) && (mrp_error == 0))
		usleep(20000);
	return 0;
}

