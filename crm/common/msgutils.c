/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <crm.h>

#include <portability.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include <stdlib.h>

#include <clplumbing/cl_log.h>

#include <time.h> 

#include <msgutils.h>

#include <ha_msg.h>
#include <ipcutils.h>
#include <xmlutils.h>
#include <xmltags.h>
#include <xmlvalues.h>

#include <crm/dmalloc_wrapper.h>



char *
getNow(void)
{
	char *since_epoch = (char*)ha_malloc(128*(sizeof(char)));
	FNIN();
	sprintf(since_epoch, "%ld", (unsigned long)time(NULL));
	FNRET(since_epoch);
}


xmlNodePtr
createPingAnswerFragment(const char *from, const char *status)
{
	xmlNodePtr ping = create_xml_node(NULL, XML_CRM_TAG_PING);
	FNIN();
	
	set_xml_property_copy(ping, XML_PING_ATTR_STATUS, status);
	set_xml_property_copy(ping, XML_PING_ATTR_SYSFROM, from);

	FNRET(ping);
}

xmlNodePtr
createPingRequest(const char *crm_msg_reference, const char *to)
{
	xmlNodePtr root_xml_node = NULL;
	FNIN();
	
	// 2 = "_" + '\0'
	int sub_type_len = strlen(to) + strlen(XML_MSG_TAG_REQUEST) + 2; 
	char *sub_type_target = (char*)ha_malloc(sizeof(char)*(sub_type_len));

	sprintf(sub_type_target, "%s_%s", to, XML_MSG_TAG_REQUEST);
	root_xml_node   = create_xml_node(NULL, sub_type_target);
	set_xml_property_copy(root_xml_node,
						  XML_MSG_ATTR_REFERENCE,
						  crm_msg_reference);
    
	int msg_type_len = strlen(to) + 10 + 1; // + "_operation" + '\0'
	char *msg_type_target = (char*)ha_malloc(sizeof(char)*(msg_type_len));
	sprintf(msg_type_target, "%s_operation", to);
	set_xml_property_copy(root_xml_node, msg_type_target, "ping");
//    ha_free(msg_type_target);

	FNRET(root_xml_node);
}

#if 0
// hopefully deprecated in favour of the send_* functions at the bottom
xmlNodePtr
createCrmMsg(xmlNodePtr data, gboolean is_request)
{
	xmlDocPtr doc;
	const char *message_type = XML_MSG_TAG_RESPONSE;
	FNIN();

	if (is_request) message_type = XML_MSG_TAG_REQUEST;
    
	doc = xmlNewDoc("1.0");
	doc->children = create_xml_doc_node(doc, XML_MSG_TAG);


	// the root_xml_node node
	set_xml_property_copy(doc->children, XML_ATTR_VERSION, CRM_VERSION);
	set_xml_property_copy(doc->children, XML_MSG_ATTR_MSGTYPE, message_type);
	set_xml_property_copy(doc->children, XML_ATTR_TSTAMP, getNow());
    
	// create a place holder for the eventual data
	xmlNodePtr xml_node   = create_xml_node(doc->children, NULL,
										message_type, NULL);

	if (data != NULL)
		xmlAddChild(xml_node, data);
    
	FNRET(xmlDocGetRootElement(doc));
}
#endif

const char *
generateReference(void)
{
	FNIN();
	FNRET(getNow());
}

gboolean
conditional_add_failure(xmlNodePtr failed,
						xmlNodePtr target,
						int operation,
						int return_code)
{
	FNIN();
	gboolean was_error = FALSE;
    
	if (return_code < 0)
	{
		was_error = TRUE;
	
		cl_log(LOG_DEBUG,
			   "Action %d failed (cde=%d)",
			   operation,
			   return_code);
	
		xmlNodePtr xml_node = create_xml_node(NULL, XML_FAIL_TAG_CIB);
		set_xml_property_copy(xml_node,
							  XML_FAILCIB_ATTR_ID,
							  ID(target));
		set_xml_property_copy(xml_node,
							  XML_FAILCIB_ATTR_OBJTYPE,
							  TYPE(target));
	
		char buffer[20]; // will handle 64 bit integers
	
		/* for now just put in the operation code
		 * later, convert it to text 
		 */
		sprintf(buffer, "%d", operation);
		set_xml_property_copy(xml_node, XML_FAILCIB_ATTR_OP, buffer);
	
		/* for now just put in the return code
		 * later, convert it to text based on the operation
		 */
		sprintf(buffer, "%d", return_code);
		set_xml_property_copy(xml_node, XML_FAILCIB_ATTR_REASON, buffer);
	
		xmlAddChild(failed, xml_node);
	}

	FNRET(was_error);
}


xmlNodePtr
validate_crm_message(xmlNodePtr root_xml_node,
					 const char *sys,
					 const char *uid,
					 const char *msg_type)
{
	const char *from = NULL;
	const char *to = NULL;
	const char *type = NULL;
	const char *crm_msg_reference = NULL;
	
	FNIN();
	if (root_xml_node == NULL) FNRET(NULL);

	from = xmlGetProp(root_xml_node, XML_MSG_ATTR_SYSFROM);
	to = xmlGetProp(root_xml_node, XML_MSG_ATTR_SYSTO);
	type = xmlGetProp(root_xml_node, XML_MSG_ATTR_MSGTYPE);
	crm_msg_reference = xmlGetProp(root_xml_node, XML_MSG_ATTR_REFERENCE);

	cl_log(LOG_DEBUG, "Recieved XML message with (version=%s)",
		   xmlGetProp(root_xml_node, XML_ATTR_VERSION));
	cl_log(LOG_DEBUG, "Recieved XML message with (from=%s)", from);
	cl_log(LOG_DEBUG, "Recieved XML message with (to=%s)"  , to);
	cl_log(LOG_DEBUG, "Recieved XML message with (type=%s)", type);
	cl_log(LOG_DEBUG, "Recieved XML message with (ref=%s)" ,
		   crm_msg_reference);

	const char *true_sys = sys;
	if (uid != NULL) true_sys = generate_hash_key(sys, uid);

	if (to == NULL) {
		cl_log(LOG_INFO, "No sub-system defined.");
		FNRET(NULL);
	} else if (true_sys != NULL && strcmp(to, true_sys) != 0) {
		cl_log(LOG_DEBUG,
			   "The message is not for this sub-system (%s != %s).",
			   to,
			   true_sys);
		FNRET(NULL);
	}
    
	if (type == NULL) {
		cl_log(LOG_INFO, "No message type defined.");
		FNRET(NULL);
	} else if (msg_type != NULL && strcmp(msg_type, type) != 0) {
		cl_log(LOG_INFO, "Expecting a (%s) message but receieved a (%s).",
			   msg_type, type);
		FNRET(NULL);
	}

	if (crm_msg_reference == NULL) {
		cl_log(LOG_INFO, "No message crm_msg_reference defined.");
		FNRET(NULL);
	}
    
	xmlNodePtr action = find_xml_node(root_xml_node, type);
	if (action == NULL) {
		cl_log(LOG_INFO,
			   "Malformed XML.  Message type (%s) not found.",
			   type);
		FNRET(NULL);
	}

	/* when the msg is a request, the message will contain
	 * <dest_sys>_request node.
	 * Otherwise it will contain a <src_sys>_response node.
	 */
	const char *action_sys = from;
	if (strcmp("request", type) == 0) {
		action_sys = to;
	}

	int action_len = strlen(action_sys) + strlen(type) + 2;
	char *action_target = (char*)ha_malloc(sizeof(char)*(action_len));
	sprintf(action_target, "%s_%s", action_sys, type);
	action_target[action_len-1] = '\0';
    
	action = find_xml_node(action, action_target);
	if (action == NULL) {
		cl_log(LOG_ERR, "Malformed XML.  Message action (%s) not found... "
			   "discarding message.",
			   action_target);
		FNRET(NULL);
	}

    
	cl_log(LOG_DEBUG, "XML is valid and node with message type (%s) found.",
		   type);
	cl_log(LOG_DEBUG, "Returning node (%s)", xmlGetNodePath(action));
	FNRET(action);
}

gboolean
decodeNVpair(const char *srcstring,
			 char separator,
			 char **name,
			 char **value)
{
	FNIN();
	int lpc = 0;
	const char *temp = NULL;

	CRM_DEBUG2("Attempting to decode: [%s]", srcstring);
	if (srcstring != NULL) {
		int len = strlen(srcstring);
		while(lpc < len) {
			if (srcstring[lpc++] == separator) {
				*name = (char*)ha_malloc(sizeof(char)*lpc);
				CRM_DEBUG2("Malloc ok %d", lpc);
				strncpy(*name, srcstring, lpc-1);
				CRM_DEBUG2("Strcpy ok %d", lpc-1);
				(*name)[lpc-1] = '\0';
				CRM_DEBUG2("Found token [%s]", *name);

				// this sucks but as the strtok *is* a bug
				len = len-lpc+1;
				*value = (char*)ha_malloc(sizeof(char)*len);
				CRM_DEBUG2("Malloc ok %d", len);
				temp = srcstring+lpc;
				CRM_DEBUG("Doing str copy");
				strncpy(*value, temp, len-1);
				(*value)[len-1] = '\0';
				CRM_DEBUG2("Found token [%s]", *value);

				FNRET(TRUE);
			}
		}
	}

	*name = NULL;
	*value = NULL;
    
	FNRET(FALSE);
}

char *
generate_hash_key(const char *crm_msg_reference, const char *sys)
{
	FNIN();
	int ref_len = strlen(sys) + strlen(crm_msg_reference) + 2;
	char *hash_key = (char*)ha_malloc(sizeof(char)*(ref_len));
	sprintf(hash_key, "%s_%s", sys, crm_msg_reference);
	hash_key[ref_len-1] = '\0';
	cl_log(LOG_INFO, "created hash key: (%s)", hash_key);
	FNRET(hash_key);
}

char *
generate_hash_value(const char *src_node, const char *src_subsys)
{
	FNIN();
	int ref_len;
	char *hash_value;

	if (src_node == NULL || src_subsys == NULL) {
		FNRET(NULL);
	}
    
	if (strcmp("dc", src_subsys) == 0) {
		hash_value = ha_strdup(src_subsys);
		if (!hash_value) {
			cl_log(LOG_ERR, "memory allocation failed in "
			       "generate_hash_value()\n");
			FNRET(NULL);
		}
		FNRET(hash_value);
	}
    
	ref_len = strlen(src_subsys) + strlen(src_node) + 2;
	hash_value = (char*)ha_malloc(sizeof(char)*(ref_len));
	if (!hash_value) {
		cl_log(LOG_ERR, "memory allocation failed in "
		       "generate_hash_value()\n");
		FNRET(NULL);
	}

	snprintf(hash_value, ref_len-1, "%s_%s", src_node, src_subsys);
	hash_value[ref_len-1] = '\0';// make sure it is null terminated

	cl_log(LOG_INFO, "created hash value: (%s)", hash_value);
	FNRET(hash_value);
}

gboolean
decode_hash_value(gpointer value, char **node, char **subsys)
{
	FNIN();
	char *char_value = (char*)value;
	int value_len = strlen(char_value);
    
	cl_log(LOG_INFO, "Decoding hash value: (%s:%d)", char_value, value_len);
    	
	if (strcmp("dc", (char*)value) == 0) {
		*node = NULL;
		*subsys = (char*)ha_strdup(char_value);
		if (!*subsys) {
			cl_log(LOG_ERR, "memory allocation failed in "
			       "decode_hash_value()\n");
			FNRET(FALSE);
		}
		cl_log(LOG_INFO, "Decoded value: (%s:%d)", *subsys, 
		       (int)strlen(*subsys));
		FNRET(TRUE);
	}
	else if (char_value != NULL) {
		if (decodeNVpair(char_value, '_', node, subsys)) {
			FNRET(TRUE);
		} else {
			*node = NULL;
			*subsys = NULL;
			FNRET(FALSE);
		}
	}
	FNRET(FALSE);
}


void
send_hello_message(IPC_Channel *ipc_client,
				   const char *uid,
				   const char *client_name,
				   const char *major_version,
				   const char *minor_version)
{
	FNIN();
	if (uid == NULL || strlen(uid) == 0
	   || client_name == NULL || strlen(client_name) == 0
	   || major_version == NULL || strlen(major_version) == 0
	   || minor_version == NULL || strlen(minor_version) == 0) {
		cl_log(LOG_ERR, "Missing fields, Hello message will not be valid.");
		return;
	}

	xmlDocPtr hello = xmlNewDoc("1.0");

	hello->children = create_xml_doc_node(hello, "hello");
	set_xml_property_copy(hello->children, "client_uuid", uid);
	set_xml_property_copy(hello->children, "client_name", client_name);
	set_xml_property_copy(hello->children, "major_version", major_version);
	set_xml_property_copy(hello->children, "minor_version", minor_version);

	send_xmlipc_message(ipc_client, xmlDocGetRootElement(hello));
}


gboolean
process_hello_message(IPC_Message *hello_message,
					  char **uid,
					  char **client_name,
					  char **major_version,
					  char **minor_version)
{
	FNIN();
	*uid = NULL;
	*client_name = NULL;
	*major_version = NULL;
	*minor_version = NULL;

	if (hello_message == NULL || hello_message->msg_body == NULL) {
		FNRET(FALSE);
	}

	xmlDocPtr hello_doc = xmlParseMemory(
		hello_message->msg_body,
		strlen(hello_message->msg_body));
	if (hello_doc == NULL) {
		cl_log(LOG_ERR,
			   "Expected a Hello message, Got: %s",
			   (char*)hello_message->msg_body);
		FNRET(FALSE);
	}
    
	xmlNodePtr hello = xmlDocGetRootElement(hello_doc);
	if (hello == NULL) {
		FNRET(FALSE);
	} else if (strcmp("hello", hello->name) != 0) {
		FNRET(FALSE);
	}
	char *local_uid = xmlGetProp(hello, "client_uuid");
	char *local_client_name = xmlGetProp(hello, "client_name");
	char *local_major_version = xmlGetProp(hello, "major_version");
	char *local_minor_version = xmlGetProp(hello, "minor_version");
    
	if (local_uid == NULL || strlen(local_uid) == 0
		|| local_client_name == NULL || strlen(local_client_name) == 0
		|| local_major_version == NULL || strlen(local_major_version) == 0
		|| local_minor_version == NULL || strlen(local_minor_version) == 0) {
		cl_log(LOG_ERR,
			   "Hello message was not valid, discarding. Message: %s",
			   (char*)hello_message->msg_body);
		FNRET(FALSE);
	}
    
	*uid = ha_strdup(local_uid);
	*client_name = ha_strdup(local_client_name);
	*major_version = ha_strdup(local_major_version);
	*minor_version = ha_strdup(local_minor_version);

	(void)_ha_msg_h_Id; // until the lmb cleanup
    
	FNRET(TRUE);
}

/*
 * Caution... this method WILL unlink xml_response_data from its
 * current context
 */
gboolean
forward_ipc_request(IPC_Channel *ipc_channel,
					xmlNodePtr xml_request, xmlNodePtr xml_response_data,
					const char *sys_to, const char *sys_from)
{
	FNIN();
	gboolean was_sent = FALSE;
	xmlNodePtr forward = create_forward(xml_request,
										xml_response_data,
										sys_to);
	if (forward != NULL)
	{
		was_sent = send_xmlipc_message(ipc_channel, forward);

		// unlink xml_response_data from wrapper and free the doc
		unlink_xml_node(xml_response_data);
		free_xml(forward);
	}
	FNRET(was_sent);
}


/*
 * Caution... this method WILL unlink xml_response_data from its
 * current context
 */
gboolean
send_ipc_request(IPC_Channel *ipc_channel, xmlNodePtr xml_msg_node,
				 const char *host_to, const char *sys_to,
				 const char *sys_from, const char *uid_from,
				 const char *crm_msg_reference)
{
	FNIN();
	gboolean was_sent = FALSE;
	const char *message_type = XML_MSG_TAG_REQUEST;

	const char *true_from = sys_from;
	if (uid_from != NULL)
		true_from = generate_hash_key(sys_from, uid_from);
	// else make sure we are internal
	else
	{
		if (strcmp(CRM_SYSTEM_LRMD, sys_from) != 0
			&& strcmp(CRM_SYSTEM_PENGINE, sys_from) != 0
			&& strcmp(CRM_SYSTEM_TENGINE, sys_from) != 0
			&& strcmp(CRM_SYSTEM_DC, sys_from) != 0
			&& strcmp(CRM_SYSTEM_CRMD, sys_from) != 0)
		{
			cl_log(LOG_ERR,
				   "only internal systems can leave uid_from blank");
			FNRET(FALSE);
		}
	}

	if (crm_msg_reference == NULL)
		crm_msg_reference = generateReference();
    
	// host_from will get set for us if necessary by CRMd when routed
	xmlDocPtr doc = xmlNewDoc("1.0");
	doc->children = create_xml_doc_node(doc, XML_MSG_TAG);

	// the root_xml_node node
	set_xml_property_copy(doc->children,
						  XML_ATTR_VERSION,
						  CRM_VERSION);
	set_xml_property_copy(doc->children,
						  XML_MSG_ATTR_MSGTYPE,
						  message_type);
	set_xml_property_copy(doc->children,
						  XML_ATTR_TSTAMP,
						  getNow());
    set_xml_property_copy(doc->children,
						  XML_MSG_ATTR_REFERENCE,
						  crm_msg_reference);
	set_xml_property_copy(doc->children,
						  XML_MSG_ATTR_HOSTTO,
						  host_to);
	set_xml_property_copy(doc->children,
						  XML_MSG_ATTR_SYSTO,
						  sys_to);
	set_xml_property_copy(doc->children,
						  XML_MSG_ATTR_SYSFROM,
						  true_from);

	// create a place holder for the eventual data
	xmlNodePtr xml_node = create_xml_node(doc->children, message_type);

	CRM_DEBUG("Created request wrapper...");
	xml_message_debug(xmlDocGetRootElement(doc));
    
	if (xml_msg_node != NULL)
	{
		unlink_xml_node(xml_msg_node);
		xmlAddChild(xml_node, xml_msg_node);
	}

	was_sent =
		send_xmlipc_message(ipc_channel, xmlDocGetRootElement(doc));

	// unlink xml_msg_node from xml_node and free the doc
	if (xml_msg_node != NULL) unlink_xml_node(xml_msg_node);
	free_xml(xml_node);
    
	FNRET(was_sent);
}

/*
 * Caution... this method WILL unlink xml_response_data from its
 * current context
 */
gboolean
send_ha_request()
{
	FNIN();
	// host_from will get set for us by CRMd when routed
	FNRET(FALSE);
}

/*
 * Caution... this method WILL unlink xml_response_data from its
 * current context
 */
gboolean
send_ipc_reply(IPC_Channel *ipc_channel,
			   xmlNodePtr xml_request,
			   xmlNodePtr xml_response_data)
{
	FNIN();
	gboolean was_sent = FALSE;
	xmlNodePtr reply = create_reply(xml_request, xml_response_data);
	if (reply != NULL) {
		was_sent = send_xmlipc_message(ipc_channel, reply);

		// unlink xml_response_data from wrapper and free the doc
		unlink_xml_node(xml_response_data);
		char *reply_text = dump_xml(reply);
		CRM_DEBUG2("freeing reply: [%s]", reply_text);
		ha_free(reply_text);
		free_xml(reply);
	}
	FNRET(was_sent);
}


/*
 * Caution... this method WILL unlink xml_response_data from its
 * current context
 */
// required?  or just send to self an let relay_message do its thing?
gboolean
send_ha_reply(ll_cluster_t *hb_cluster,
			  xmlNodePtr xml_request,
			  xmlNodePtr xml_response_data)
{
	FNIN();
	gboolean was_sent = FALSE;
	xmlNodePtr reply = create_reply(xml_request, xml_response_data);
	if (reply != NULL) {
		was_sent = send_xmlha_message(hb_cluster, reply);

		// unlink xml_response_data from wrapper and free the doc
		unlink_xml_node(xml_response_data);
		free_xml(reply);
	}
	FNRET(was_sent);
}

/*
 * Caution... this method WILL unlink xml_response_data from its
 * current context
 */
xmlNodePtr
create_reply(xmlNodePtr original_request,
			 xmlNodePtr xml_response_data)
{
	const char *message_type = XML_MSG_TAG_RESPONSE;
	const char *crm_msg_reference = NULL;
	const char *host_from = NULL;
	const char *host_to   = NULL;
	const char *sys_from  = NULL;
	const char *sys_to    = NULL;
	const char *type      = NULL;

	crm_msg_reference =
		xmlGetProp(original_request, XML_MSG_ATTR_REFERENCE);
	host_from = xmlGetProp(original_request, XML_MSG_ATTR_HOSTFROM);
	host_to   = xmlGetProp(original_request, XML_MSG_ATTR_HOSTTO);
	sys_from  = xmlGetProp(original_request, XML_MSG_ATTR_SYSFROM);
	sys_to    = xmlGetProp(original_request, XML_MSG_ATTR_SYSTO);
	type      = xmlGetProp(original_request, XML_MSG_ATTR_MSGTYPE);

	if (type == NULL)
	{
		cl_log(LOG_ERR,
			   "Cannot create reply, no message type in original message");
		FNRET(NULL);
	}
	else if (strcmp(XML_MSG_TAG_REQUEST, type) != 0)
	{
		cl_log(LOG_ERR,
			   "Cannot create reply, original message was not a request");
		FNRET(NULL);
	}

#if 0
	CRM_DEBUG("creating reply from...");
	CRM_DEBUG2("\t%s", crm_msg_reference);
	CRM_DEBUG2("\t%s", host_from);
	CRM_DEBUG2("\t%s", host_to);
	CRM_DEBUG2("\t%s", sys_from);
	CRM_DEBUG2("\t%s", sys_to);
	CRM_DEBUG2("\t%s", type);
#endif
	
	xmlDocPtr doc = xmlNewDoc("1.0");
	doc->children = create_xml_doc_node(doc, XML_MSG_TAG);

	// the root_xml_node node
	set_xml_property_copy(doc->children, XML_ATTR_VERSION,     CRM_VERSION);
	set_xml_property_copy(doc->children, XML_MSG_ATTR_MSGTYPE, message_type);
	set_xml_property_copy(doc->children, XML_ATTR_TSTAMP,      getNow());

	/* since this is a reply, we reverse the from and to */

	// HOSTTO will be ignored if it is to the DC anyway.
	set_xml_property_copy(doc->children, XML_MSG_ATTR_HOSTTO,   host_from);
	set_xml_property_copy(doc->children, XML_MSG_ATTR_HOSTFROM, host_to);
	set_xml_property_copy(doc->children, XML_MSG_ATTR_SYSTO,    sys_from);
	set_xml_property_copy(doc->children, XML_MSG_ATTR_SYSFROM,  sys_to);

	set_xml_property_copy(doc->children,
						  XML_MSG_ATTR_REFERENCE,
						  crm_msg_reference);

	// create a place holder for the eventual data
	xmlNodePtr xml_node   = create_xml_node(doc->children, message_type);

	CRM_DEBUG("Created reply wrapper...");
	xml_message_debug(xmlDocGetRootElement(doc));
    
	if (xml_response_data != NULL)
	{
		unlink_xml_node(xml_response_data);
		xmlAddChild(xml_node, xml_response_data);
	}
    
	FNRET(xmlDocGetRootElement(doc));
}

/*
 * Caution... this method WILL unlink xml_response_data from its
 * current context
 */
xmlNodePtr
create_forward(xmlNodePtr original_request,
			   xmlNodePtr xml_response_data,
			   const char *sys_to)
{
	const char *message_type = XML_MSG_TAG_REQUEST;
	const char *crm_msg_reference = NULL;
	const char *host_from = NULL;
	const char *host_to   = NULL;
	const char *sys_from  = NULL;
	const char *type      = NULL;
	
	FNIN();
	crm_msg_reference = xmlGetProp(original_request, XML_MSG_ATTR_REFERENCE);
	host_from = xmlGetProp(original_request, XML_MSG_ATTR_HOSTFROM);
	host_to   = xmlGetProp(original_request, XML_MSG_ATTR_HOSTTO);
	sys_from  = xmlGetProp(original_request, XML_MSG_ATTR_SYSFROM);
	type      = xmlGetProp(original_request, XML_MSG_ATTR_MSGTYPE);
	
	if (type == NULL)
	{
		cl_log(LOG_ERR,
			   "Cannot create reply, no message type in original message");
		FNRET(NULL);
	}
	else if (strcmp(XML_MSG_TAG_REQUEST, type) != 0)
	{
		cl_log(LOG_ERR,
			   "Cannot create reply, original message was not a request");
		FNRET(NULL);
	}

#if 0
	CRM_DEBUG("creating forward from...");
	CRM_DEBUG2("\t%s", crm_msg_reference);
	CRM_DEBUG2("\t%s", host_from);
	CRM_DEBUG2("\t%s", host_to);
	CRM_DEBUG2("\t%s", sys_from);
	CRM_DEBUG2("\t%s", sys_to);
	CRM_DEBUG2("\t%s", type);
#endif

	xmlDocPtr doc = xmlNewDoc("1.0");
	doc->children = create_xml_doc_node(doc, XML_MSG_TAG);

	// the root_xml_node node
	set_xml_property_copy(doc->children, XML_ATTR_VERSION,     CRM_VERSION);
	set_xml_property_copy(doc->children, XML_MSG_ATTR_MSGTYPE, message_type);
	set_xml_property_copy(doc->children, XML_ATTR_TSTAMP,      getNow());
    
	// HOSTTO will be ignored if it is to the DC anyway.
	set_xml_property_copy(doc->children, XML_MSG_ATTR_HOSTTO,   host_to);
	set_xml_property_copy(doc->children, XML_MSG_ATTR_HOSTFROM, host_from);

	set_xml_property_copy(doc->children, XML_MSG_ATTR_SYSTO,    sys_to);
	set_xml_property_copy(doc->children, XML_MSG_ATTR_SYSFROM,  sys_from);

	set_xml_property_copy(doc->children, XML_MSG_ATTR_REFERENCE,
						  crm_msg_reference);
    
	// create a place holder for the eventual data
	xmlNodePtr xml_node   = create_xml_node(doc->children, message_type);

	CRM_DEBUG("Created forward wrapper...");
	xml_message_debug(xmlDocGetRootElement(doc));
    
	if (xml_response_data != NULL) {
		unlink_xml_node(xml_response_data);
		xmlAddChild(xml_node, xml_response_data);
	}
    
	FNRET(xmlDocGetRootElement(doc));
}

