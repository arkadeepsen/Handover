/*
 * nicira-new.h
 *
 *  Created on: Nov 5, 2015
 *      Author: arkadeep
 */

#ifndef NICIRA_NEW_H_
#define NICIRA_NEW_H_

#include "openflow.h"
#include "nicira-ext.h"

/* Configures the "role" of the sending controller.  The default role is:
 *
 *    - Other (NX_ROLE_OTHER), which allows the controller access to all
 *      OpenFlow features.
 *
 * The other possible roles are a related pair:
 *
 *    - Master (NX_ROLE_MASTER) is equivalent to Other, except that there may
 *      be at most one Master controller at a time: when a controller
 *      configures itself as Master, any existing Master is demoted to the
 *      Slave role.
 *
 *    - Slave (NX_ROLE_SLAVE) allows the controller read-only access to
 *      OpenFlow features.  In particular attempts to modify the flow table
 *      will be rejected with an OFPBRC_EPERM error.
 *
 *      Slave controllers do not receive OFPT_PACKET_IN or OFPT_FLOW_REMOVED
 *      messages, but they do receive OFPT_PORT_STATUS messages.
 */
enum nx_subtype
{
	NXT_ROLE_REQUEST = 10,
	NXT_ROLE_REPLY
};

struct nx_role_request {
	uint32_t role;              /* One of NX_ROLE_*. */
};
OFP_ASSERT(sizeof(struct nx_role_request) == 4);

enum nx_role {
    NX_ROLE_OTHER,              /* Default role, full access. */
    NX_ROLE_MASTER,             /* Full access, at most one. */
    NX_ROLE_SLAVE               /* Read-only access. */
};


#endif /* NICIRA_NEW_H_ */
