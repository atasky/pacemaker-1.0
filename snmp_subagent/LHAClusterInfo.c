/*
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

/*
 * Note: this file originally auto-generated by mib2c using
 *        : mib2c.scalar.conf,v 1.7 2003/04/08 14:57:04 dts12 Exp $
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "hbagent.h"
#include "LHAClusterInfo.h"

/** Initializes the LHAClusterInfo module */
void
init_LHAClusterInfo(void)
{
    static oid LHALiveNodeCount_oid[] = { 1,3,6,1,4,1,4682,1,2 };
    static oid LHATotalNodeCount_oid[] = { 1,3,6,1,4,1,4682,1,1 };

    DEBUGMSGTL(("LHAClusterInfo", "Initializing\n"));

    netsnmp_register_scalar(
        netsnmp_create_handler_registration("LHALiveNodeCount", handle_LHALiveNodeCount,
                               LHALiveNodeCount_oid, OID_LENGTH(LHALiveNodeCount_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("LHATotalNodeCount", handle_LHATotalNodeCount,
                               LHATotalNodeCount_oid, OID_LENGTH(LHATotalNodeCount_oid),
                               HANDLER_CAN_RONLY
        ));
}

int
handle_LHALiveNodeCount(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */
    
    size_t long_ret = 0;
    switch(reqinfo->mode) {

        case MODE_GET:

	    get_int_value(LHA_CLUSTERINFO, LIVE_NODE_COUNT, 0, 
		    (int32_t *) &long_ret);

            snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
                                     (u_char *) &long_ret,
                                     sizeof(long_ret));
            break;


        default:
            /* we should never get here, so this is a really bad error */
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

int
handle_LHATotalNodeCount(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */
    
    size_t long_ret = 0;
    switch(reqinfo->mode) {

        case MODE_GET:

	    get_int_value(LHA_CLUSTERINFO, TOTAL_NODE_COUNT, 0, 
		    (int32_t *) &long_ret);

            snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
                                     (u_char *) &long_ret,
                                     sizeof(long_ret));
            break;


        default:
            /* we should never get here, so this is a really bad error */
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}


