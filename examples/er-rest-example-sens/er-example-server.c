/*
 * Copyright (c) 2011, Matthias Kovatsch and other contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Erbium (Er) REST Engine example (with CoAP-specific code)
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/clock.h"
#include "sys/rtimer.h"
#include "contiki.h"
#include "contiki-net.h"

/* Define which resources to include to meet memory constraints. */
#define REST_RES_TEMP 1
#define REST_RES_GROUP 1
#define REST_RES_HELLO 0
#define REST_RES_MIRROR 0 /* causes largest code size */
#define REST_RES_CHUNKS 0
#define REST_RES_SEPARATE 0
#define REST_RES_PUSHING 0
#define REST_RES_EVENT 0
#define REST_RES_SUB 0
#define REST_RES_LEDS 0
#define REST_RES_TOGGLE 0
#define REST_RES_LIGHT 0
#define REST_RES_BATTERY 0
#define REST_RES_RADIO 0

#include "erbium.h"

/* for temperature sensor */
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"

#if defined (PLATFORM_HAS_BATTERY)
#include "dev/battery-sensor.h"
#endif

/* For CoAP-specific example: not required for normal RESTful Web service. */
#if WITH_COAP == 3
#include "er-coap-03.h"
#elif WITH_COAP == 7
#include "er-coap-07.h"
#else
#warning "Erbium example without CoAP-specifc functionality"
#endif /* CoAP-specific example */

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

/******************************************************************************/
#if REST_RES_TEMP

#define CHUNKS_TOTAL      1024 
#define TEMP_MSG_MAX_SIZE 140   // more than enough right now
#define TEMP_BUFF_MAX     7     // -234.6\0

/******************************************************************************/
/* globals ********************************************************************/
/******************************************************************************/
char tempstring[TEMP_BUFF_MAX];

/******************************************************************************/
/* helper functions ***********************************************************/
/******************************************************************************/
int temp_to_buff(char* buffer) {
  int16_t  tempint;
  uint16_t tempfrac;
  int16_t  raw;
  uint16_t absraw;
  int16_t  sign = 1;

  /* get temperature */
  raw = tmp102_read_temp_raw();

  absraw = raw;
  if (raw < 0)
  {
    // Perform 2C's if sensor returned negative data
    absraw = (raw ^ 0xFFFF) + 1;
    sign = -1;
  }
  tempint  = (absraw >> 8) * sign;
  tempfrac = ((absraw>>4) % 16) * 625; // Info in 1/10000 of degree
  tempfrac = ((tempfrac) / 1000); // Round to 1 decimal

  return snprintf(buffer, TEMP_BUFF_MAX, "%d.%1d", tempint, tempfrac);
}

int temp_to_default_buff() {

  return temp_to_buff(tempstring);

}

uint8_t create_response(int num, int accept, char *buffer)
{
  size_t size_temp;
  int size_msgp1, size_msgp2;
  const char *msgp1, *msgp2;
  uint8_t size_msg;

  if (num && accept==REST.type.APPLICATION_XML)
  {
    msgp1 = "<obj href=\"http://myhome/temp\">\n\t<real name=\"temp\" units=\"obix:units/celsius\" val=\"";
    msgp2 = "\"/>\n</obj>\0";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 83;
    size_msgp2 = 10;
  }
  else if (num && accept==REST.type.APPLICATION_EXI)
  {
    msgp1 = "\x80\x10\x01\x04\x6F\x62\x6A\x01\x01\x00\x02\x14\x68\x74\x74\x70\x3A\x2F\x2F\x6D\x79\x68\x6F\x6D\x65\x2F\x74\x65\x6D\x70\x01\x02\x01\x05\x72\x65\x61\x6C\x01\x01\x00\x08\x06\x74\x65\x6D\x70\x01\x01\x01\x06\x75\x6E\x69\x74\x73\x14\x6F\x62\x69\x78\x3A\x75\x6E\x69\x74\x73\x2F\x63\x65\x6C\x73\x69\x75\x73\x02\x01\x01\x00\x11\x09";
    msgp2 = "\x03\x00\x00\0";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 81;
    size_msgp2 = 3;
  }
  else
  {
    PRINTF("Unsupported encoding!\n");
    return -1;
  }

  if ((size_temp = temp_to_default_buff()) < 0 )
  {
    PRINTF("Error preparing temperature string!\n");
    return -1;
  }

  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  /* we assume null-appended strings behind msgp2 and tempstring */
  size_msg = size_msgp1 + size_msgp2 + size_temp + 1;

  if (size_msg > TEMP_MSG_MAX_SIZE)
  {
    PRINTF("Message too big!\n");
    return -1;
  }

  memcpy(buffer, msgp1, size_msgp1);
  memcpy(buffer + size_msgp1, tempstring, size_temp);
  memcpy(buffer + size_msgp1 + size_temp, msgp2, size_msgp2 + 1);

  return size_msg;
}

/*
 * Resources are defined by the RESOURCE macro.
 * Signature: resource name, the RESTful methods it handles, and its URI path (omitting the leading slash).
 */
PERIODIC_RESOURCE(temp, METHOD_GET, "temp", "title=\"Hello temp: ?len=0..\";rt=\"Text\"", 5*CLOCK_SECOND);

/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
void
temp_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* we save the message as static variable, so it is retained through multiple calls (chunked resource) */
  static char message[TEMP_MSG_MAX_SIZE];
  static uint8_t size_msg;

  const uint16_t *accept = NULL;
  int num = 0, length = 0;
  char *err_msg;

  const char *len = NULL;

  /* Check the offset for boundaries of the resource data. */
  if (*offset>=CHUNKS_TOTAL)
  {
    REST.set_response_status(response, REST.status.BAD_OPTION);
    /* A block error message should not exceed the minimum block size (16). */
    err_msg = "BlockOutOfScope";
    REST.set_response_payload(response, err_msg, strlen(err_msg));
    return;
  }

  /* compute message once */
  if (*offset <= 0)
  {
    /* decide upon content-format */
    num = REST.get_header_accept(request, &accept);

    if (num && accept[0]==REST.type.APPLICATION_XML)
    {
      REST.set_header_content_type(response, REST.type.APPLICATION_XML);
    }
    else if (num && accept[0]==REST.type.APPLICATION_EXI)
    {
      REST.set_header_content_type(response, REST.type.APPLICATION_EXI);
    }
    else
    {
      PRINTF("wrong content-type!\n");
      REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
      REST.set_response_status(response, REST.status.UNSUPPORTED_MEDIA_TYPE);
      err_msg = "Supporting content-types application/xml and application/exi";
      REST.set_response_payload(response, err_msg, strlen(err_msg));
      return;
    }

    if ((size_msg = create_response(num, accept[0], message)) <= 0)
    {
      PRINTF("ERROR while creating message!\n");
      REST.set_response_status(response, REST.status.INTERNAL_SERVER_ERROR);
      err_msg = "ERROR while creating message :\\";
      REST.set_response_payload(response, err_msg, strlen(err_msg));
      return;
    }

  }
  
  length = size_msg - *offset;

  if (length <= 0)
  {
    PRINTF("AHOYHOY?!\n");
    REST.set_response_status(response, REST.status.INTERNAL_SERVER_ERROR);
    err_msg = "calculation of message length error, this should not happen :\\";
    REST.set_response_payload(response, err_msg, strlen(err_msg));
    return;
  }

  /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
  if (REST.get_query_variable(request, "len", &len))
  {
    length = atoi(len);
    if (length<0) length = 0;
  }

  if (length>REST_MAX_CHUNK_SIZE)     
  {
    length = REST_MAX_CHUNK_SIZE;

    memcpy(buffer, message + *offset, length);

    /* Truncate if above CHUNKS_TOTAL bytes. */
    if (*offset+length > CHUNKS_TOTAL)
    {
      length = CHUNKS_TOTAL - *offset;
    }

    /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
    *offset += length;

    /* Signal end of resource representation. */
    if (*offset>=CHUNKS_TOTAL)
    {
      *offset = -1;
    }
  } else {
    memcpy(buffer, message + *offset, length);
    *offset = -1;
  }

  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);

}

void temp_periodic_handler(resource_t *r)
{
  static char new_value[TEMP_BUFF_MAX];
  static char buffer[TEMP_MSG_MAX_SIZE];
  static uint8_t obs_counter = 0;
  size_t size_msg;

  if (temp_to_buff(new_value) <= 0) {
    PRINTF("ERROR while creating message!\n");
    return;
  }

  if (strncmp(new_value, tempstring, TEMP_BUFF_MAX) != 0)
  {
    if ((size_msg = create_response(1, REST.type.APPLICATION_XML, buffer)) <= 0)
    {
      PRINTF("ERROR while creating message!\n");
      return;
    }
    
    /* Build notification. */
    coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
    coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
    coap_set_payload(notification, buffer, size_msg);

    /* Notify the registered observers with the given message type, observe option, and payload. */
    REST.notify_subscribers(r, obs_counter, notification);
  }
}

#endif

/******************************************************************************/
#if REST_RES_HELLO
/*
 * Resources are defined by the RESOURCE macro.
 * Signature: resource name, the RESTful methods it handles, and its URI path (omitting the leading slash).
 */
RESOURCE(helloworld, METHOD_GET, "hello", "title=\"Hello world: ?len=0..\";rt=\"Text\"");

/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
void
helloworld_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const char *len = NULL;
  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  char const * const message = "Hello World! ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy";
  int length = 12; /*           |<-------->| */

  /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
  if (REST.get_query_variable(request, "len", &len)) {
    length = atoi(len);
    if (length<0) length = 0;
    if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;
    memcpy(buffer, message, length);
  } else {
    memcpy(buffer, message, length);
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);
}
#endif

/******************************************************************************/
#if REST_RES_GROUP
//RESOURCE(join_group, METHOD_GET | METHOD_POST, "groupComm/joinGroup", "title=\"join a multicast group\";rt=\"Debug\"");
PERIODIC_RESOURCE(join_group, METHOD_GET | METHOD_POST, "groupComm/joinGroup", "title=\"join a multicast group\";rt=\"Debug\"", 5*CLOCK_SECOND);

char group_addr[39] = "\0";

uint8_t create_join_group_response(int num, int accept, char *buffer)
{
  size_t size_addr;
  int size_msgp1, size_msgp2;
  const char *msgp1, *msgp2;
  uint8_t size_msg;

  if (num && accept==REST.type.APPLICATION_XML)
  {
    msgp1 = "<str name=\"addr\" val=\"";
    msgp2 = "\"/>\0";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 23;
    size_msgp2 = 4;
  }
  #if 0
  else if (num && accept==REST.type.APPLICATION_EXI)
  {
    msgp1 = "";
    msgp2 = "";
    /* hardcoded length, ugly but faster and necc. for exi-answer */
    size_msgp1 = 81;
    size_msgp2 = 3;
  }
  #endif
  else
  {
    PRINTF("Unsupported encoding!\n");
    return -1;
  }

  size_addr = strlen(group_addr);

  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  /* we assume null-appended strings behind msgp2 and tempstring */
  size_msg = size_msgp1 + size_msgp2 + size_addr+1;

  if (size_msg > TEMP_MSG_MAX_SIZE)
  {
    PRINTF("Message too big!\n");
    return -1;
  }

  memcpy(buffer, msgp1, size_msgp1);
  memcpy(buffer + size_msgp1, group_addr, size_addr);
  memcpy(buffer + size_msgp1 + size_addr, msgp2, size_msgp2 + 1);

  return size_msg;
}


void
join_group_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const uint8_t *bytes;
  char *addr_new;
  uint8_t len1 = 0;
  char *addr_end;
  uint8_t addr_len = 0;
  const uint16_t *accept = NULL;

  /* static variables so they are retained over consecutive calls (chunked resource) */
  static char message[TEMP_MSG_MAX_SIZE];
  static uint8_t size_msg;

  int num = 0, length = 0;
  char *err_msg;

  const char *len = NULL;
  
  if ((len1 = REST.get_request_payload(request, &bytes)) > 0)
  {
    PRINTF("received payload size: %d\n", len1);
    PRINTF("received payload: %s\n", bytes);
    
    if ((addr_new = strstr(bytes, "val=\"")) != NULL)
    {
      addr_new += 5;

      addr_end = strchr(addr_new, '"');

      if ((addr_len = addr_end - addr_new) > 39)
      {
        PRINTF("Address to long!");
        return;
      }
      PRINTF("Address length: %d\n", addr_len);
      memcpy(group_addr, addr_new, addr_len);
      PRINTF("Address: %s\n", group_addr);
    }
  }

  /* Check the offset for boundaries of the resource data. */
  if (*offset>=CHUNKS_TOTAL)
  {
    REST.set_response_status(response, REST.status.BAD_OPTION);
    /* A block error message should not exceed the minimum block size (16). */
    err_msg = "BlockOutOfScope";
    REST.set_response_payload(response, err_msg, strlen(err_msg));
    return;
  }

  /* compute message once */
  if (*offset <= 0)
  {
    /* decide upon content-format */
    num = REST.get_header_accept(request, &accept);

    if (num && accept[0]==REST.type.APPLICATION_XML)
    {
      REST.set_header_content_type(response, REST.type.APPLICATION_XML);
    }
    #if 0
    else if (num && accept[0]==REST.type.APPLICATION_EXI)
    {
      REST.set_header_content_type(response, REST.type.APPLICATION_EXI);
    }
    #endif
    else
    {
      PRINTF("wrong content-type!\n");
      REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
      REST.set_response_status(response, REST.status.UNSUPPORTED_MEDIA_TYPE);
      err_msg = "Supporting content-type application/xml";
      REST.set_response_payload(response, err_msg, strlen(err_msg));
      return;
    }

    if ((size_msg = create_join_group_response(num, accept[0], message)) <= 0)
    {
      PRINTF("ERROR while creating message!\n");
      REST.set_response_status(response, REST.status.INTERNAL_SERVER_ERROR);
      err_msg = "ERROR while creating message :\\";
      REST.set_response_payload(response, err_msg, strlen(err_msg));
      return;
    }

  }
  
  length = size_msg - *offset;

  if (length <= 0)
  {
    PRINTF("AHOYHOY?!\n");
    REST.set_response_status(response, REST.status.INTERNAL_SERVER_ERROR);
    err_msg = "calculation of message length error, this should not happen :\\";
    REST.set_response_payload(response, err_msg, strlen(err_msg));
    return;
  }

  /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
  if (REST.get_query_variable(request, "len", &len))
  {
    length = atoi(len);
    if (length<0) length = 0;
  }

  if (length>REST_MAX_CHUNK_SIZE)     
  {
    length = REST_MAX_CHUNK_SIZE;

    memcpy(buffer, message + *offset, length);

    /* Truncate if above CHUNKS_TOTAL bytes. */
    if (*offset+length > CHUNKS_TOTAL)
    {
      length = CHUNKS_TOTAL - *offset;
    }

    /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
    *offset += length;

    /* Signal end of resource representation. */
    if (*offset>=CHUNKS_TOTAL)
    {
      *offset = -1;
    }
  } else {
    memcpy(buffer, message + *offset, length);
    *offset = -1;
  }

  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);
}

#include "er-coap-07-transactions.h"

void join_group_periodic_handler(resource_t *r)
{
  static uip_ipaddr_t ipaddr;
  int size_msg;
  char buffer[TEMP_BUFF_MAX];

  if (strlen(group_addr) > 0)
  {
    if (uiplib_ip6addrconv(group_addr, &ipaddr) <= 0)
    {
      PRINTF("Error converting ip-addr\n");
      return;
    }
    PRINTF("Sending to: ");
    PRINT6ADDR(&ipaddr);
    PRINTF("\n");

    if ((size_msg = create_response(1, REST.type.APPLICATION_XML, buffer)) <= 0)
    {
      PRINTF("ERROR while creating message!\n");
      return;
    }
    
    /* Build notification. */
    coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
    coap_init_message(notification, COAP_TYPE_NON, COAP_POST, 0 );
    coap_set_header_uri_path(notification, "temp");
    coap_set_payload(notification, buffer, size_msg);

    coap_transaction_t *transaction = NULL;
    if ( (transaction = coap_new_transaction(coap_get_mid(), &ipaddr, UIP_HTONS(COAP_DEFAULT_PORT))) )
    {
      /*
       * Be aware to respect the Block2 option, which is also stored in the coap_separate_t.
       * As it is a critical option, this example resource pretends to handle it for compliance.
       */
      //coap_set_header_block2(response, separate_store->request_metadata.block2_num, 0, separate_store->request_metadata.block2_size);

      /* Warning: No check for serialization error. */
      transaction->packet_len = coap_serialize_message(notification, transaction->packet);
      coap_send_transaction(transaction);
      /* The engine will clear the transaction (right after send for NON, after acked for CON). */

    PRINTF("SENT!\n");
    }

  }
}
#endif /* REST_RES_GROUP */

/******************************************************************************/
#if REST_RES_MIRROR
/* This resource mirrors the incoming request. It shows how to access the options and how to set them for the response. */
RESOURCE(mirror, METHOD_GET | METHOD_POST | METHOD_PUT | METHOD_DELETE, "debug/mirror", "title=\"Returns your decoded message\";rt=\"Debug\"");

void
mirror_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* The ETag and Token is copied to the header. */
  uint8_t opaque[] = {0x0A, 0xBC, 0xDE};

  /* Strings are not copied, so use static string buffers or strings in .text memory (char *str = "string in .text";). */
  static char location[] = {'/','f','/','a','?','k','&','e', 0};

  /* Getter for the header option Content-Type. If the option is not set, text/plain is returned by default. */
  unsigned int content_type = REST.get_header_content_type(request);

  /* The other getters copy the value (or string/array pointer) to the given pointers and return 1 for success or the length of strings/arrays. */
  uint32_t max_age = 0;
  const char *str = NULL;
  uint32_t observe = 0;
  const uint8_t *bytes = NULL;
  uint32_t block_num = 0;
  uint8_t block_more = 0;
  uint16_t block_size = 0;
  const char *query = "";
  int len = 0;

  /* Mirror the received header options in the response payload. Unsupported getters (e.g., rest_get_header_observe() with HTTP) will return 0. */

  int strpos = 0;
  /* snprintf() counts the terminating '\0' to the size parameter.
   * The additional byte is taken care of by allocating REST_MAX_CHUNK_SIZE+1 bytes in the REST framework.
   * Add +1 to fill the complete buffer. */
  strpos += snprintf((char *)buffer, REST_MAX_CHUNK_SIZE+1, "CT %u\n", content_type);

  /* Some getters such as for ETag or Location are omitted, as these options should not appear in a request.
   * Max-Age might appear in HTTP requests or used for special purposes in CoAP. */
  if (strpos<=REST_MAX_CHUNK_SIZE && REST.get_header_max_age(request, &max_age))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "MA %lu\n", max_age);
  }

  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_header_host(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "UH %.*s\n", len, str);
  }

/* CoAP-specific example: actions not required for normal RESTful Web service. */
#if WITH_COAP > 1
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_observe(request, &observe))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Ob %lu\n", observe);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_token(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "To 0x");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", bytes[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_etag(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "ET 0x");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", bytes[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_uri_path(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "UP ");
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%.*s\n", len, str);
  }
#if WITH_COAP == 3
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Lo %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Bl %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
#else
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location_path(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "LP %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location_query(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "LQ %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block2(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "B2 %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
  /*
   * Critical Block1 option is currently rejected by engine.
   *
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block1(request, &block_num, &block_more, &block_size, NULL))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "B1 %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
  */
#endif /* CoAP > 03 */
#endif /* CoAP-specific example */

  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_query(request, &query)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Qu %.*s\n", len, query);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_request_payload(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%.*s", len, bytes);
  }

  if (strpos >= REST_MAX_CHUNK_SIZE)
  {
      buffer[REST_MAX_CHUNK_SIZE-1] = 0xBB; /* '»' to indicate truncation */
  }

  REST.set_response_payload(response, buffer, strpos);

  PRINTF("/mirror options received: %s\n", buffer);

  /* Set dummy header options for response. Like getters, some setters are not implemented for HTTP and have no effect. */
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_max_age(response, 10); /* For HTTP, browsers will not re-request the page for 10 seconds. CoAP action depends on the client. */
  REST.set_header_etag(response, opaque, 2);
  REST.set_header_location(response, location); /* Initial slash is omitted by framework */

/* CoAP-specific example: actions not required for normal RESTful Web service. */
#if WITH_COAP > 1
  coap_set_header_uri_host(response, "tiki");
  coap_set_header_observe(response, 10);
#if WITH_COAP == 3
  coap_set_header_block(response, 42, 0, 64); /* The block option might be overwritten by the framework when blockwise transfer is requested. */
#else
  coap_set_header_proxy_uri(response, "ftp://x");
  coap_set_header_block2(response, 42, 0, 64); /* The block option might be overwritten by the framework when blockwise transfer is requested. */
  coap_set_header_block1(response, 23, 0, 16);
  coap_set_header_accept(response, TEXT_PLAIN);
  coap_set_header_if_none_match(response);
#endif /* CoAP > 03 */
#endif /* CoAP-specific example */
}
#endif /* REST_RES_MIRROR */

/******************************************************************************/
#if REST_RES_CHUNKS
/*
 * For data larger than REST_MAX_CHUNK_SIZE (e.g., stored in flash) resources must be aware of the buffer limitation
 * and split their responses by themselves. To transfer the complete resource through a TCP stream or CoAP's blockwise transfer,
 * the byte offset where to continue is provided to the handler as int32_t pointer.
 * These chunk-wise resources must set the offset value to its new position or -1 of the end is reached.
 * (The offset for CoAP's blockwise transfer can go up to 2'147'481'600 = ~2047 M for block size 2048 (reduced to 1024 in observe-03.)
 */
RESOURCE(chunks, METHOD_GET, "test/chunks", "title=\"Blockwise demo\";rt=\"Data\"");

#define CHUNKS_TOTAL    2050

void
chunks_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int32_t strpos = 0;

  /* Check the offset for boundaries of the resource data. */
  if (*offset>=CHUNKS_TOTAL)
  {
    REST.set_response_status(response, REST.status.BAD_OPTION);
    /* A block error message should not exceed the minimum block size (16). */

    const char *error_msg = "BlockOutOfScope";
    REST.set_response_payload(response, error_msg, strlen(error_msg));
    return;
  }

  /* Generate data until reaching CHUNKS_TOTAL. */
  while (strpos<preferred_size)
  {
    strpos += snprintf((char *)buffer+strpos, preferred_size-strpos+1, "|%ld|", *offset);
  }

  /* snprintf() does not adjust return value if truncated by size. */
  if (strpos > preferred_size)
  {
    strpos = preferred_size;
  }

  /* Truncate if above CHUNKS_TOTAL bytes. */
  if (*offset+(int32_t)strpos > CHUNKS_TOTAL)
  {
    strpos = CHUNKS_TOTAL - *offset;
  }

  REST.set_response_payload(response, buffer, strpos);

  /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
  *offset += strpos;

  /* Signal end of resource representation. */
  if (*offset>=CHUNKS_TOTAL)
  {
    *offset = -1;
  }
}
#endif

/******************************************************************************/
#if REST_RES_SEPARATE && defined (PLATFORM_HAS_BUTTON) && WITH_COAP > 3
/* Required to manually (=not by the engine) handle the response transaction. */
#include "er-coap-07-separate.h"
#include "er-coap-07-transactions.h"
/*
 * CoAP-specific example for separate responses.
 * Note the call "rest_set_pre_handler(&resource_separate, coap_separate_handler);" in the main process.
 * The pre-handler takes care of the empty ACK and updates the MID and message type for CON requests.
 * The resource handler must store all information that required to finalize the response later.
 */
RESOURCE(separate, METHOD_GET, "test/separate", "title=\"Separate demo\"");

/* A structure to store the required information */
typedef struct application_separate_store {
  /* Provided by Erbium to store generic request information such as remote address and token. */
  coap_separate_t request_metadata;
  /* Add fields for addition information to be stored for finalizing, e.g.: */
  char buffer[16];
} application_separate_store_t;

static uint8_t separate_active = 0;
static application_separate_store_t separate_store[1];

void
separate_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /*
   * Example allows only one open separate response.
   * For multiple, the application must manage the list of stores.
   */
  if (separate_active)
  {
    coap_separate_reject();
  }
  else
  {
    separate_active = 1;

    /* Take over and skip response by engine. */
    coap_separate_accept(request, &separate_store->request_metadata);
    /* Be aware to respect the Block2 option, which is also stored in the coap_separate_t. */

    /*
     * At the moment, only the minimal information is stored in the store (client address, port, token, MID, type, and Block2).
     * Extend the store, if the application requires additional information from this handler.
     * buffer is an example field for custom information.
     */
    snprintf(separate_store->buffer, sizeof(separate_store->buffer), "StoredInfo");
  }
}

void
separate_finalize_handler()
{
  if (separate_active)
  {
    coap_transaction_t *transaction = NULL;
    if ( (transaction = coap_new_transaction(separate_store->request_metadata.mid, &separate_store->request_metadata.addr, separate_store->request_metadata.port)) )
    {
      coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */

      /* Restore the request information for the response. */
      coap_separate_resume(response, &separate_store->request_metadata, CONTENT_2_05);

      coap_set_payload(response, separate_store->buffer, strlen(separate_store->buffer));

      /*
       * Be aware to respect the Block2 option, which is also stored in the coap_separate_t.
       * As it is a critical option, this example resource pretends to handle it for compliance.
       */
      coap_set_header_block2(response, separate_store->request_metadata.block2_num, 0, separate_store->request_metadata.block2_size);

      /* Warning: No check for serialization error. */
      transaction->packet_len = coap_serialize_message(response, transaction->packet);
      coap_send_transaction(transaction);
      /* The engine will clear the transaction (right after send for NON, after acked for CON). */

      separate_active = 0;
    }
    else
    {
      /*
       * Set timer for retry, send error message, ...
       * The example simply waits for another button press.
       */
    }
  } /* if (separate_active) */
}
#endif

/******************************************************************************/
#if REST_RES_PUSHING
/*
 * Example for a periodic resource.
 * It takes an additional period parameter, which defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions by managing a list of subscribers to notify.
 */
PERIODIC_RESOURCE(pushing, METHOD_GET, "test/push", "title=\"Periodic demo\";obs", 5*CLOCK_SECOND);

void
pushing_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

  /* Usually, a CoAP server would response with the resource representation matching the periodic_handler. */
  const char *msg = "It's periodic!";
  REST.set_response_payload(response, msg, strlen(msg));

  /* A post_handler that handles subscriptions will be called for periodic resources by the REST framework. */
}

/*
 * Additionally, a handler function named [resource name]_handler must be implemented for each PERIODIC_RESOURCE.
 * It will be called by the REST manager process with the defined period.
 */
void
pushing_periodic_handler(resource_t *r)
{
  static uint16_t obs_counter = 0;
  static char content[11];

  ++obs_counter;

  PRINTF("TICK %u for /%s\n", obs_counter, r->url);

  /* Build notification. */
  coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
  coap_set_payload(notification, content, snprintf(content, sizeof(content), "TICK %u", obs_counter));

  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r, obs_counter, notification);
}
#endif

/******************************************************************************/
#if REST_RES_EVENT && defined (PLATFORM_HAS_BUTTON)
/*
 * Example for an event resource.
 * Additionally takes a period parameter that defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions and manages a list of subscribers to notify.
 */
EVENT_RESOURCE(event, METHOD_GET, "sensors/button", "title=\"Event demo\";obs");

void
event_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  /* Usually, a CoAP server would response with the current resource representation. */
  const char *msg = "It's eventful!";
  REST.set_response_payload(response, (uint8_t *)msg, strlen(msg));

  /* A post_handler that handles subscriptions/observing will be called for periodic resources by the framework. */
}

/* Additionally, a handler function named [resource name]_event_handler must be implemented for each PERIODIC_RESOURCE defined.
 * It will be called by the REST manager process with the defined period. */
void
event_event_handler(resource_t *r)
{
  static uint16_t event_counter = 0;
  static char content[12];

  ++event_counter;

  PRINTF("TICK %u for /%s\n", event_counter, r->url);

  /* Build notification. */
  coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  coap_init_message(notification, COAP_TYPE_CON, CONTENT_2_05, 0 );
  coap_set_payload(notification, content, snprintf(content, sizeof(content), "EVENT %u", event_counter));

  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r, event_counter, notification);
}
#endif /* PLATFORM_HAS_BUTTON */

/******************************************************************************/
#if REST_RES_SUB
/*
 * Example for a resource that also handles all its sub-resources.
 * Use REST.get_url() to multiplex the handling of the request depending on the Uri-Path.
 */
RESOURCE(sub, METHOD_GET | HAS_SUB_RESOURCES, "test/path", "title=\"Sub-resource demo\"");

void
sub_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

  const char *uri_path = NULL;
  int len = REST.get_url(request, &uri_path);
  int base_len = strlen(resource_sub.url);

  if (len==base_len)
  {
  snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "Request any sub-resource of /%s", resource_sub.url);
  }
  else
  {
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, ".%s", uri_path+base_len);
  }

  REST.set_response_payload(response, buffer, strlen((char *)buffer));
}
#endif

/******************************************************************************/
#if REST_RES_BATTERY && defined (PLATFORM_HAS_BATTERY)
/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(battery, METHOD_GET, "sensors/battery", "title=\"Battery status\";rt=\"Battery\"");
void
battery_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int battery = battery_sensor.value(0);

  const uint16_t *accept = NULL;
  int num = REST.get_header_accept(request, &accept);

  if ((num==0) || (num && accept[0]==REST.type.TEXT_PLAIN))
  {
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%d", battery);

    REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
  }
  else if (num && (accept[0]==REST.type.APPLICATION_JSON))
  {
    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'battery':%d}", battery);

    REST.set_response_payload(response, buffer, strlen((char *)buffer));
  }
  else
  {
    REST.set_response_status(response, REST.status.UNSUPPORTED_MADIA_TYPE);
    const char *msg = "Supporting content-types text/plain and application/json";
    REST.set_response_payload(response, msg, strlen(msg));
  }
}
#endif /* PLATFORM_HAS_BATTERY */


PROCESS(rest_server_example, "Erbium Example Server");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("Starting Erbium Example Server\n");

#ifdef RF_CHANNEL
  PRINTF("RF channel: %u\n", RF_CHANNEL);
#endif
#ifdef IEEE802154_PANID
  PRINTF("PAN ID: 0x%04X\n", IEEE802154_PANID);
#endif

  PRINTF("uIP buffer: %u\n", UIP_BUFSIZE);
  PRINTF("LL header: %u\n", UIP_LLH_LEN);
  PRINTF("IP+UDP header: %u\n", UIP_IPUDPH_LEN);
  PRINTF("REST max chunk: %u\n", REST_MAX_CHUNK_SIZE);

  /* Initialize the REST engine. */
  rest_init_engine();

  /* Activate the application-specific resources. */

#if REST_RES_TEMP
  rest_activate_periodic_resource(&periodic_resource_temp);
  //rest_activate_resource(&resource_temp);
  tmp102_init();
#endif
#if REST_RES_GROUP
  rest_activate_periodic_resource(&periodic_resource_join_group);
#endif
#if REST_RES_PUSHING
  rest_activate_periodic_resource(&periodic_resource_pushing);
#endif
#if defined (PLATFORM_HAS_BUTTON) && REST_RES_SEPARATE && WITH_COAP > 3
  /* No pre-handler anymore, user coap_separate_accept() and coap_separate_reject(). */
  rest_activate_resource(&resource_separate);
#endif
#if defined (PLATFORM_HAS_BUTTON) && (REST_RES_EVENT || (REST_RES_SEPARATE && WITH_COAP > 3))
  SENSORS_ACTIVATE(button_sensor);
#endif
#if defined (PLATFORM_HAS_BATTERY) && REST_RES_BATTERY
  SENSORS_ACTIVATE(battery_sensor);
  rest_activate_resource(&resource_battery);
#endif

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
  } /* while (1) */

  PROCESS_END();
}
