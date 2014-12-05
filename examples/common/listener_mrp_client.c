/*
  Copyright (c) 2013 Katja Rohloff <Katja.Rohloff@uni-jena.de>
  Copyright (c) 2014, Parrot SA

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "mrpdclient.h"
#include "listener_mrp_client.h"

/* global variables */

volatile int talker = 0;
unsigned char stream_id[8];

static int msg_process(char *buf, int buflen)
{
	uint32_t id;
	int j, l;

	if (NULL == buf)
		return -1;

	if (strncmp(buf, "SNE T:", 6) == 0 || strncmp(buf, "SJO T:", 6) == 0)
	{
		l = 6; /* skip "Sxx T:" */
		while ((l < buflen) && ('S' != buf[l++]));
		if (l = buflen)
			return -1;
		l++;
		for(j = 0; j < 8 ; l+=2, j++)
		{
			sscanf(&buf[l],"%02x",&id);
			stream_id[j] = (unsigned char)id;
		}
		talker = 1;
	}

	return 0;
}

/*
 * public
 */

int report_domain_status(SOCKET mrpd_sock)
{
	char* buf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	buf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == buf)
		return -1;
	memset(buf, 0, MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(buf, "S+D:C=6,P=3,V=0002");

	rc = mrpdclient_sendto(mrpd_sock, buf, MRPDCLIENT_MAX_MSG_SIZE);
	free(buf);

	return rc;
}

int join_vlan(SOCKET mrpd_sock)
{
	char *buf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	buf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == buf)
		return -1;
	memset(buf, 0, MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(buf, "V++:I=0002");
	rc = mrpdclient_sendto(mrpd_sock, buf, MRPDCLIENT_MAX_MSG_SIZE);
	free(buf);

	return rc;
}

int await_talker(SOCKET mrpd_sock)
{
	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	while (0 == talker)
		mrpdclient_recv(mrpd_sock, msg_process);

	return 0;
}

int send_ready(SOCKET mrpd_sock)
{
	char *buf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	buf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == buf)
		return -1;
	memset(buf, 0, MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(buf, "S+L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=2",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
	rc = mrpdclient_sendto(mrpd_sock, buf, MRPDCLIENT_MAX_MSG_SIZE);

	free(buf);

	return rc;
}

int send_leave(SOCKET mrpd_sock)
{
	char *buf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	buf = malloc(MRPDCLIENT_MAX_MSG_SIZE);
	if (NULL == buf)
		return -1;
	memset(buf, 0, MRPDCLIENT_MAX_MSG_SIZE);
	sprintf(buf, "S-L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=3",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
	rc = mrpdclient_sendto(mrpd_sock, buf, MRPDCLIENT_MAX_MSG_SIZE);
	free(buf);

	return rc;
}
