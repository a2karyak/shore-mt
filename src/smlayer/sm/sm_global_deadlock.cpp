/*<std-header orig-src='shore'>

 $Id: sm_global_deadlock.cpp,v 1.45 1999/08/06 15:22:54 bolo Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#define SM_SOURCE
#define SM_GLOBAL_DEADLOCK_C

#ifdef __GNUG__
#pragma implementation "global_deadlock.h"
#pragma implementation "sm_global_deadlock.h"
#endif

#include <st_error_enum_gen.h>
#include <sm_int_1.h>
#ifdef USE_COORD
#include <sm_global_deadlock.h>
#include <lock_s.h>


#ifdef EXPLICIT_TEMPLATE
template class w_list_t<BlockedElem>;
template class w_list_i<BlockedElem>;
template class w_list_t<WaitForElem>;
template class w_list_i<WaitForElem>;
template class w_list_t<WaitPtrForPtrElem>;
template class w_list_t<IdElem>;
template class w_list_i<IdElem>;
template class w_hash_t<GtidTableElem, const gtid_t>;
template class w_hash_i<GtidTableElem, const gtid_t>;
template class w_list_t<GtidTableElem>;
template class w_list_i<GtidTableElem>;
#endif

W_FASTNEW_STATIC_DECL(BlockedElem, 64);
W_FASTNEW_STATIC_DECL(WaitForElem , 64);
W_FASTNEW_STATIC_DECL(WaitPtrForPtrElem , 64);
W_FASTNEW_STATIC_DECL(IdElem , 64);
W_FASTNEW_STATIC_DECL(GtidTableElem , 64);


/***************************************************************************
 *                                                                         *
 * DeadlockMsg class output operator                                       *
 *                                                                         *
 ***************************************************************************/

const char* DeadlockMsg::msgNameStrings[] = {
	"msgInvalidType",
	"msgVictimizerEndpoint",
	"msgRequestClientId",
	"msgAssignClientId",
	"msgRequestDeadlockCheck",
	"msgRequestWaitFors",
	"msgWaitForList",
	"msgSelectVictim",
	"msgVictimSelected",
	"msgKillGtid",
	"msgQuit",
	"msgClientEndpointDied",
	"msgVictimizerEndpointDied",
	"msgServerEndpointDied"
};



ostream& operator<<(ostream& o, const DeadlockMsg& msg)
{
	o <<     "  msgType:  " << DeadlockMsg::msgNameStrings[msg.MsgType()] << endl;

	o << "      clientId: ";
	if (msg.ClientId() == DeadlockMsgHeader::noId)
		o << "noId";
	else
		o << msg.ClientId();

	o << endl
	  << "      complete: " << (msg.IsComplete() ? "yes" : "no") << endl
	  << "      count:    " << msg.GtidCount() << endl
	  << "      gtids:    [";

	for (uint4_t i = 0; i < msg.GtidCount(); i++)  {
		o << ' ' << i << ":" << msg.gtids[i];
	}
	
	o << " ]" << endl;

	return o;
}

void	DeadlockMsgHeader::hton()
{
	clientId = htonl(clientId);
	count = htons(count);
}

void	DeadlockMsgHeader::ntoh()
{
	clientId = ntohl(clientId);
	count = ntohs(count);
}

void	DeadlockMsg::hton()
{
	for (int i = 0; i < header.count; i++)
		gtids[i].hton();
	header.hton();
}

void	DeadlockMsg::ntoh()
{
	header.ntoh();

	/* Make sure we don't believe a trashed message and trash memory */
	w_assert1(header.count <= MaxNumGtidsPerMsg);

	for (int i = 0; i < header.count; i++)
		gtids[i].ntoh();
}


/***************************************************************************
 *                                                                         *
 * notify_always                                                           *
 *                                                                         *
 ***************************************************************************/

inline rc_t notify_always(Endpoint& endpoint, Endpoint& target, Buffer& msgBuffer)
{
    rc_t rc = endpoint.notify(target, msgBuffer);
    if (rc && rc == RC(scDEAD))  {
	EndpointBox box;
	W_DO( box.set(0, endpoint) );
	W_DO( target.send(msgBuffer, box) );
    }  else if (rc)  {
	W_DO(rc);
    }

    return RCOK;
}


/***************************************************************************
 *                                                                         *
 * IgnoreScDEAD                                                            *
 *                                                                         *
 ***************************************************************************/

inline rc_t IgnoreScDEAD(const rc_t& rc)
{
    if (rc && rc == RC(scDEAD))  {
	return RCOK;
    }  else  {
	return rc;
    }
}


/***************************************************************************
 *                                                                         *
 * DeadlockClientCommunicator class                                        *
 *                                                                         *
 ***************************************************************************/

DeadlockClientCommunicator::DeadlockClientCommunicator(
	const server_handle_t& theServerHandle,
	name_ep_map* ep_map,
	CommSystem& commSystem)
:   deadlockClient(0),
    serverHandle(theServerHandle),
    endpointMap(ep_map),
    myId(noId),
    sendBuffer(sizeof(DeadlockMsg)),
    rcvBuffer(sizeof(DeadlockMsg)),
    serverEndpointDiedBuffer(sizeof(DeadlockMsgHeader)),
    sendMsg(0),
    rcvMsg(0),
    comm(commSystem),
    done(false),
    serverEndpointValid(false)
{
    DeadlockMsgHeader* serverDiedMsg = new (serverEndpointDiedBuffer.start()) DeadlockMsgHeader(msgServerEndpointDied);
    serverDiedMsg->hton();

    sendMsg = new (sendBuffer.start()) DeadlockMsg;
    rcvMsg = new (rcvBuffer.start()) DeadlockMsg;

    W_COERCE( comm.make_endpoint(myEndpoint) );
    W_COERCE( endpointMap->name2endpoint(serverHandle, serverEndpoint) );
    serverEndpointValid = true;

    W_COERCE( notify_always(serverEndpoint, myEndpoint, serverEndpointDiedBuffer) );
}


DeadlockClientCommunicator::~DeadlockClientCommunicator()
{
    if (serverEndpointValid)  {
	W_COERCE( SendClientEndpointDied() );
	W_COERCE( IgnoreScDEAD( serverEndpoint.stop_notify(myEndpoint) ) );
	W_COERCE( serverEndpoint.release() );
    }
    
    W_COERCE( myEndpoint.release() );
}


rc_t DeadlockClientCommunicator::SendRequestClientId()
{
    new (sendMsg) DeadlockMsg(msgRequestClientId);
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockClientCommunicator::ReceivedAssignClientId(uint4_t clientId)
{
    myId = clientId;
    w_assert3(myId != noId);
    W_DO( deadlockClient->AssignClientId() );
    
    return RCOK;
}


rc_t DeadlockClientCommunicator::SendRequestDeadlockCheck()
{
    Buffer buffer(sizeof(DeadlockMsg));
    DeadlockMsg *msg = new (buffer.start()) DeadlockMsg(msgRequestDeadlockCheck, myId);

    W_DO( SendMsg( *msg, buffer ) );
    
    return RCOK;
}


rc_t DeadlockClientCommunicator::SendWaitForList(WaitForList& waitForList)
{
    new (sendMsg) DeadlockMsg(msgWaitForList, myId);
    
    if (waitForList.is_empty())  {
        W_DO( SendMsg() );
    }  else  {
	WaitForElem* waitForElem;
    	while ((waitForElem = waitForList.pop()))  {
    	    sendMsg->AddGtid(waitForElem->waitGtid);
    	    sendMsg->AddGtid(waitForElem->forGtid);
    	    delete waitForElem;
    	    if (sendMsg->GtidCount() >= sendMsg->MaxGtidCount() - 1 || waitForList.is_empty())  {
    	        sendMsg->SetComplete(waitForList.is_empty());
    	        W_DO( SendMsg() );
    	        sendMsg->ResetCount();
    	    }
    	}
    }
    
    return RCOK;
}


rc_t DeadlockClientCommunicator::ReceivedRequestWaitForList()
{
    W_DO( deadlockClient->SendWaitForGraph() );
    return RCOK;
}


rc_t DeadlockClientCommunicator::ReceivedKillGtid(const gtid_t& gtid)
{
    W_DO( deadlockClient->UnblockGlobalXct(gtid) );
    return RCOK;
}


rc_t DeadlockClientCommunicator::SendQuit()
{
    Buffer		buffer(sizeof(DeadlockMsg));
    DeadlockMsg		*msg = new (buffer.start()) DeadlockMsg(msgQuit, myId);

    DBGTHRD( << "Deadlock client sending message:" << endl << " C->" << *msg );
    msg->hton();

    EndpointBox		emptyBox;
    W_DO( myEndpoint.send(buffer, emptyBox) );
    return RCOK;
}


rc_t DeadlockClientCommunicator::SendClientEndpointDied()
{
    new (sendMsg) DeadlockMsg(msgClientEndpointDied, myId);
    W_DO( SendMsg() );

    return RCOK;
}


rc_t DeadlockClientCommunicator::ReceivedServerEndpointDied(Endpoint& theServerEndpoint)
{
    if (serverEndpointValid)  {
	w_assert3(theServerEndpoint == serverEndpoint);

	W_DO( IgnoreScDEAD( serverEndpoint.stop_notify(myEndpoint) ) );
	
	W_DO( serverEndpoint.release() );
	serverEndpointValid = false;
    }
    W_DO( theServerEndpoint.release() );

    return RCOK;
}


rc_t DeadlockClientCommunicator::SendMsg()
{
    return SendMsg(*sendMsg, sendBuffer);
}


rc_t DeadlockClientCommunicator::SendMsg(DeadlockMsg &msg, Buffer &buffer)
{
    if (serverEndpointValid)  {
	msg.SetClientId(myId);
	DBGTHRD( << "Deadlock client sending message:" << endl << " C->" << *sendMsg );
	w_rc_t			e;
	EndpointBox		box;
	if (sendMsg->MsgType() == msgRequestClientId || sendMsg->MsgType() == msgClientEndpointDied)  {
	    W_DO( box.set(0, myEndpoint) );
	}
	msg.hton();
	e = IgnoreScDEAD( serverEndpoint.send(buffer, box) );
	msg.ntoh();
	W_DO(e);
    }
    return RCOK;
}


void DeadlockClientCommunicator::SetDeadlockClient(CentralizedGlobalDeadlockClient* client)
{
    delete deadlockClient;
    deadlockClient = client;
}


rc_t DeadlockClientCommunicator::RcvAndDispatchMsg()
{
    EndpointBox		box;
    W_DO( myEndpoint.receive(rcvBuffer, box) );
    rcvMsg->ntoh();
    DBGTHRD( << "Deadlock client received message:" << endl << " ->C" << *rcvMsg );
    switch (rcvMsg->MsgType())  {
	case msgAssignClientId:
	    W_DO( ReceivedAssignClientId(rcvMsg->ClientId()) );
	    break;
	case msgRequestWaitFors:
	    W_DO( ReceivedRequestWaitForList() );
	    break;
	case msgKillGtid:
	    W_DO( ReceivedKillGtid(rcvMsg->gtids[0]) );
	    break;
	case msgQuit:
	    done = true;
	    break;
	case msgServerEndpointDied:
	    {
		Endpoint theServerEndpoint;
		W_DO( box.get(0, theServerEndpoint) );
		W_DO( ReceivedServerEndpointDied(theServerEndpoint) );
	    }
	    break;
	default:
	    w_assert1(0);
    }
    return RCOK;
}


/***************************************************************************
 *                                                                         *
 * DeadlockServerCommunicator class                                        *
 *                                                                         *
 ***************************************************************************/

PickFirstDeadlockVictimizerCallback selectFirstVictimizer;


DeadlockServerCommunicator::DeadlockServerCommunicator(
	const server_handle_t&		theServerHandle,
	name_ep_map*			ep_map,
	CommSystem&			theCommSystem,
	DeadlockVictimizerCallback&	callback
)
:   deadlockServer(0),
    serverHandle(theServerHandle),
    endpointMap(ep_map),
    commSystem(theCommSystem),
    clientEndpointsSize(0),
    clientEndpoints(0),
    sendBuffer(sizeof(DeadlockMsg)),
    rcvBuffer(sizeof(DeadlockMsg)),
    clientEndpointDiedBuffer(sizeof(DeadlockMsgHeader)),
    victimizerEndpointDiedBuffer(sizeof(DeadlockMsgHeader)),
    sendMsg(0),
    rcvMsg(0),
    selectVictim(callback),
    remoteVictimizer(false),
    done(false)
{
    sendMsg = new (sendBuffer.start()) DeadlockMsg;
    rcvMsg = new (rcvBuffer.start()) DeadlockMsg;

    DeadlockMsgHeader* clientDiedMsg = new (clientEndpointDiedBuffer.start()) DeadlockMsgHeader(msgClientEndpointDied);
    clientDiedMsg->hton();

    DeadlockMsgHeader* victimizerDiedMsg = new (victimizerEndpointDiedBuffer.start()) DeadlockMsgHeader(msgVictimizerEndpointDied);
    victimizerDiedMsg->hton();

    clientEndpoints = new Endpoint[InitialNumberOfEndpoints];
    clientEndpointsSize = InitialNumberOfEndpoints;
    w_assert3(clientEndpointsSize);

    W_COERCE( endpointMap->name2endpoint(serverHandle, myEndpoint) );
}


DeadlockServerCommunicator::~DeadlockServerCommunicator()
{
    W_COERCE( BroadcastServerEndpointDied() );

    int4_t i = 0;
    while ((i = activeClientIds.FirstSetOnOrAfter(i)) != -1)  {
    	W_COERCE( IgnoreScDEAD( clientEndpoints[i].stop_notify(myEndpoint) ) );
	W_COERCE( clientEndpoints[i].release() );
	i++;
    }
    delete [] clientEndpoints;
    
    if (remoteVictimizer)  {
    	W_COERCE( IgnoreScDEAD( victimizerEndpoint.stop_notify(myEndpoint) ) );
    	W_COERCE( victimizerEndpoint.release() );
	remoteVictimizer = false;
    }
    
    W_COERCE( myEndpoint.release() );
}


rc_t DeadlockServerCommunicator::ReceivedVictimizerEndpoint(Endpoint& ep)
{
    victimizerEndpoint = ep;
    W_DO( notify_always(ep, myEndpoint, victimizerEndpointDiedBuffer) );
    remoteVictimizer = true;
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedRequestClientId(Endpoint& ep)
{
    uint4_t clientId = activeClientIds.FirstClearOnOrAfter(0);
    DBG(<<"ReceivedRequestClientId : new clientID is " << clientId);
    activeClientIds.SetBit(clientId);
    W_DO( deadlockServer->AddClient(clientId) );
    SetClientEndpoint(clientId, ep);
    W_DO( SendAssignClientId(clientId) );
    W_DO( notify_always(ep, myEndpoint, clientEndpointDiedBuffer) );
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendAssignClientId(uint4_t clientId)
{
    new (sendMsg) DeadlockMsg(msgAssignClientId, clientId);
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedRequestDeadlockCheck()
{
    W_DO( deadlockServer->CheckDeadlockRequested() );
    return RCOK;
}


rc_t DeadlockServerCommunicator::BroadcastRequestWaitForList()
{
    uint4_t i = 0;
    int4_t clientId;
    while ((clientId = activeClientIds.FirstSetOnOrAfter(i)) != -1)  {
        W_DO( SendRequestWaitForList(clientId) );
        i++;
    }
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendRequestWaitForList(uint4_t clientId)
{
    new (sendMsg) DeadlockMsg(msgRequestWaitFors, clientId);
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedWaitForList(uint4_t clientId, uint2_t count, const gtid_t* gtids, bool complete)
{
    WaitPtrForPtrList waitPtrForPtrList(WaitPtrForPtrElem::link_offset());
    uint4_t i = 0;
    while (i < count)  {
        const gtid_t* waitPtr = &gtids[i++];
        const gtid_t* forPtr = &gtids[i++];
        waitPtrForPtrList.push(new WaitPtrForPtrElem(waitPtr, forPtr));
    }
    W_DO( deadlockServer->AddWaitFors(clientId, waitPtrForPtrList, complete) );
    w_assert3(waitPtrForPtrList.is_empty());
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendSelectVictim(IdList& deadlockList)
{
    w_assert3(!deadlockList.is_empty());

    if (smlevel_0::deadlockEventCallback)  {
	GtidList gtidList(GtidElem::link_offset());
	IdListIter iter(deadlockList);
	IdElem* idElem;
	while ((idElem = iter.next()))  {
	    gtidList.push(new GtidElem(deadlockServer->IdToGtid(idElem->id)));
	}
	smlevel_0::deadlockEventCallback->GlobalDeadlockDetected(gtidList);
	GtidElem* elem;
	while ((elem = gtidList.pop()))  {
	    delete elem;
	}
    }

    if (remoteVictimizer)  {
	new (sendMsg) DeadlockMsg(msgSelectVictim);

	IdElem* idElem;
	while ((idElem = deadlockList.pop()))  {
	    sendMsg->AddGtid(deadlockServer->IdToGtid(idElem->id));
	    delete idElem;
	    if (sendMsg->GtidCount() >= sendMsg->MaxGtidCount() || deadlockList.is_empty())  {
		sendMsg->SetComplete(deadlockList.is_empty());
		W_DO( SendMsg(toVictimizer) );
		sendMsg->ResetCount();
	    }
	}
    }  else  {
	GtidList gtidList(GtidElem::link_offset());
	IdElem* idElem;
	while ((idElem = deadlockList.pop()))  {
	    gtidList.push(new GtidElem(deadlockServer->IdToGtid(idElem->id)));
	    delete idElem;
	}
	gtid_t gtid = selectVictim(gtidList);
	if (smlevel_0::deadlockEventCallback)  {
	    smlevel_0::deadlockEventCallback->GlobalDeadlockVictimSelected(gtid);
	}
	W_DO( deadlockServer->VictimSelected(gtid) );
	GtidElem* gtidElem;
	while ((gtidElem = gtidList.pop()))  {
	    delete gtidElem;
	}
    }
    w_assert3(deadlockList.is_empty());
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedVictimSelected(const gtid_t gtid)
{
    if (smlevel_0::deadlockEventCallback)  {
	smlevel_0::deadlockEventCallback->GlobalDeadlockVictimSelected(gtid);
    }
    W_DO( deadlockServer->VictimSelected(gtid) );
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendKillGtid(uint4_t clientId, const gtid_t &gtid)
{
    new (sendMsg) DeadlockMsg(msgKillGtid, clientId);
    sendMsg->AddGtid(gtid);
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendQuit()
{
    Buffer		buffer(sizeof(DeadlockMsg));
    DeadlockMsg*	msg = new (buffer.start()) DeadlockMsg(msgQuit);

    DBGTHRD( << "Deadlock server sending message:" << endl << " S->" << *msg );
    msg->hton();

    EndpointBox		emptyBox;
    W_DO( myEndpoint.send(buffer, emptyBox) );
    return RCOK;
}


rc_t DeadlockServerCommunicator::BroadcastServerEndpointDied()
{
    new (sendBuffer.start()) DeadlockMsg(msgServerEndpointDied);
    if (remoteVictimizer)  {
	W_DO( SendMsg(toVictimizer) );
    }
    
    int4_t clientId = 0;
    while ((clientId = activeClientIds.FirstSetOnOrAfter(clientId)) != -1)  {
	sendMsg->SetClientId(clientId);
        W_DO( SendMsg() );
        clientId++;
    }

    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedClientEndpointDied(Endpoint& theClientEndpoint)
{
    int4_t clientId = 0;
    while ((clientId = activeClientIds.FirstSetOnOrAfter(clientId)) != -1)  {
	if (theClientEndpoint == clientEndpoints[clientId])  {
	    w_assert3(activeClientIds.IsBitSet(clientId));
	    activeClientIds.ClearBit(clientId);
	    deadlockServer->RemoveClient(clientId);
	    W_DO( IgnoreScDEAD( theClientEndpoint.stop_notify(myEndpoint) ) );
	    W_DO( clientEndpoints[clientId].release() );
	}
	clientId++;
    }
    W_DO( theClientEndpoint.release() );
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedVictimizerEndpointDied(Endpoint& theVictimizerEndpoint)
{
    if (remoteVictimizer)  {
	W_DO( victimizerEndpoint.stop_notify(myEndpoint) );
	W_DO( victimizerEndpoint.release() );
	remoteVictimizer = false;
    }
    W_DO( theVictimizerEndpoint.release() );
    return RCOK;
}


void DeadlockServerCommunicator::SetDeadlockServer(CentralizedGlobalDeadlockServer* server)
{
    delete deadlockServer;
    deadlockServer = server;
}


rc_t DeadlockServerCommunicator::SendMsg(MsgDestination destination)
{
    DBGTHRD( << "Deadlock server sending message:" << endl << " S->" << *sendMsg );
    uint4_t clientId = sendMsg->ClientId();
    w_rc_t		e;
    EndpointBox		box;
    if (sendMsg->MsgType() == msgServerEndpointDied)  {
	W_COERCE( box.set(0, myEndpoint) );
    }
    sendMsg->hton();
    switch (destination) {
    case toVictimizer:
	e = IgnoreScDEAD( victimizerEndpoint.send(sendBuffer, box) );
	break;
    case toClient:
	w_assert3(activeClientIds.IsBitSet(clientId));
	e =  IgnoreScDEAD( clientEndpoints[clientId].send(sendBuffer, box) );
        break;
    default:
	cerr << "Unexpected MsgDestination == " << (int)destination << endl;
	e = RCOK;
	break;
    }
    sendMsg->ntoh();

    return e;
}


rc_t DeadlockServerCommunicator::RcvAndDispatchMsg()
{
    EndpointBox		box;
    W_DO( myEndpoint.receive(rcvBuffer, box) );
    rcvMsg->ntoh();
    DBGTHRD( << "Deadlock server received message:" << endl << " ->S" << *rcvMsg );
    switch (rcvMsg->MsgType())  {
	case msgVictimizerEndpoint:
	    {
		Endpoint ep;
		W_DO( box.get(0, ep) );
		W_DO( ReceivedVictimizerEndpoint(ep) );
	    }
	    break;
	case msgRequestClientId:
	    {
		Endpoint ep;
		W_DO( box.get(0, ep) );
		W_DO( ReceivedRequestClientId(ep) );
	    }
	    break;
	case msgRequestDeadlockCheck:
	    W_DO( ReceivedRequestDeadlockCheck() );
	    break;
	case msgWaitForList:
	    W_DO( ReceivedWaitForList(rcvMsg->ClientId(), rcvMsg->GtidCount(), rcvMsg->gtids, rcvMsg->IsComplete()) );
	    break;
	case msgVictimSelected:
	    W_DO( ReceivedVictimSelected(rcvMsg->gtids[0]) );
	    break;
	case msgQuit:
	    done = true;
	    break;
	case msgClientEndpointDied:
	    {
		Endpoint theServerEndpoint;
		W_DO( box.get(0, theServerEndpoint) );
		W_DO( ReceivedClientEndpointDied(theServerEndpoint) );
	    }
	    break;
	case msgVictimizerEndpointDied:
	    {
		Endpoint theServerEndpoint;
		W_DO( box.get(0, theServerEndpoint) );
		W_DO( ReceivedVictimizerEndpointDied(theServerEndpoint) );
	    }
	    break;
	default:
	    w_assert1(0);
    }
    return RCOK;
}


void DeadlockServerCommunicator::SetClientEndpoint(uint4_t clientId, Endpoint& ep)
{
    if (clientId >= clientEndpointsSize)  {
	ResizeClientEndpointsArray(clientEndpointsSize * 2);
    }

    clientEndpoints[clientId] = ep;
}


void DeadlockServerCommunicator::ResizeClientEndpointsArray(uint4_t newSize)
{
    w_assert3(newSize > 0);
    Endpoint* newClientEndpoints = new Endpoint[newSize];
    for (uint4_t i = 0; i < clientEndpointsSize; i++)  {
	newClientEndpoints[i] = clientEndpoints[i];
    }

    delete [] clientEndpoints;
    clientEndpointsSize = newSize;
    clientEndpoints = newClientEndpoints;
}


/***************************************************************************
 *                                                                         *
 * DeadlockVictimizerCommunicator class                                    *
 *                                                                         *
 ***************************************************************************/

DeadlockVictimizerCommunicator::DeadlockVictimizerCommunicator(
	const server_handle_t&		theServerHandle,
	name_ep_map*			ep_map,
	CommSystem&			commSystem,
	DeadlockVictimizerCallback&	callback)
:   serverHandle(theServerHandle),
    endpointMap(ep_map),
    sendBuffer(sizeof(DeadlockMsg)),
    rcvBuffer(sizeof(DeadlockMsg)),
    serverEndpointDiedBuffer(sizeof(DeadlockMsgHeader)),
    sendMsg(0),
    rcvMsg(0),
    gtidList(GtidElem::link_offset()),
    selectVictim(callback),
    done(false),
    serverEndpointValid(false)
{
    DeadlockMsgHeader* serverDiedMsg = new (serverEndpointDiedBuffer.start()) DeadlockMsgHeader(msgServerEndpointDied);
    serverDiedMsg->hton();

    sendMsg = new (sendBuffer.start()) DeadlockMsg;
    rcvMsg = new (rcvBuffer.start()) DeadlockMsg;

    W_COERCE( commSystem.make_endpoint(myEndpoint) );
    W_COERCE( endpointMap->name2endpoint(serverHandle, serverEndpoint) );
    serverEndpointValid = true;

    W_COERCE( notify_always(serverEndpoint, myEndpoint, serverEndpointDiedBuffer) );
}


DeadlockVictimizerCommunicator::~DeadlockVictimizerCommunicator()
{
    W_COERCE( IgnoreScDEAD( serverEndpoint.stop_notify(myEndpoint) ) );
    W_COERCE( serverEndpoint.release() );
    
    W_COERCE( myEndpoint.release() );
    
    GtidElem* elem;
    while ((elem = gtidList.pop()))
	delete elem;
}


rc_t DeadlockVictimizerCommunicator::ReceivedSelectVictim(uint2_t count, gtid_t* gtids, bool complete)
{
    for (uint4_t i = 0; i < count; i++)  {
	gtidList.push(new GtidElem(gtids[i]));
    }

    if (complete)  {
	gtid_t& gtid = selectVictim(gtidList);

	W_DO( SendVictimSelected(gtid) );

	GtidElem* gtidElem;
	while ((gtidElem = gtidList.pop()))  {
	    delete gtidElem;
	}
	w_assert3(gtidList.is_empty());
    }

    return RCOK;
}


rc_t DeadlockVictimizerCommunicator::SendVictimizerEndpoint()
{
    new (sendMsg) DeadlockMsg(msgVictimizerEndpoint);
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockVictimizerCommunicator::SendVictimSelected(const gtid_t& gtid)
{
    new (sendMsg) DeadlockMsg(msgVictimSelected);
    sendMsg->AddGtid(gtid);
    W_DO( SendMsg() );

    return RCOK;
}


rc_t DeadlockVictimizerCommunicator::ReceivedServerEndpointDied(Endpoint& theServerEndpoint)
{
    if (serverEndpointValid)  {
	W_DO( serverEndpoint.stop_notify(myEndpoint) );
	W_DO( serverEndpoint.release() );
	serverEndpointValid = false;
    }
    W_DO( theServerEndpoint.release() );
    return RCOK;
}


rc_t DeadlockVictimizerCommunicator::SendMsg()
{
    DBGTHRD( << "Deadlock victimizer sending message:" << endl << " V->" << *sendMsg );
    sendMsg->hton();
    EndpointBox emptyBox;
    rc_t e = IgnoreScDEAD( serverEndpoint.send(sendBuffer, emptyBox) );
    sendMsg->ntoh();
    return e;
}


rc_t DeadlockVictimizerCommunicator::RcvAndDispatchMsg()
{
    EndpointBox		box;
    W_DO( myEndpoint.receive(rcvBuffer, box) );
    rcvMsg->ntoh();
    DBGTHRD( << "Deadlock victimizer received message:" << endl << " ->V" << *rcvMsg );
    switch (rcvMsg->MsgType())  {
	case msgSelectVictim:
	    W_DO( ReceivedSelectVictim(rcvMsg->GtidCount(), rcvMsg->gtids, rcvMsg->IsComplete()) );
	    break;
	case msgQuit:
	    done = true;
	    break;
	case msgServerEndpointDied:
	    {
		Endpoint theServerEndpoint;
		W_DO( box.get(0, theServerEndpoint) );
		W_DO( ReceivedServerEndpointDied(theServerEndpoint) );
	    }
	    break;
	default:
	    w_assert1(0);
    }
    return RCOK;
}


/***************************************************************************
 *                                                                         *
 * CentralizedGlobalDeadlockClient class                                   *
 *                                                                         *
 ***************************************************************************/

CentralizedGlobalDeadlockClient::CentralizedGlobalDeadlockClient(
	int4_t				initial,
	int4_t				subsequent,
	DeadlockClientCommunicator*	clientCommunicator
)
:   initialTimeout(initial),
    subsequentTimeout(subsequent),
    communicator(clientCommunicator),
    blockedListMutex("globalDeadlockClient"),
    blockedList(BlockedElem::link_offset()),
    isClientIdAssigned(false)
{
    thread.set(this);
    w_assert3(communicator);
    communicator->SetDeadlockClient(this);
    W_COERCE( thread.Start() );
}


CentralizedGlobalDeadlockClient::~CentralizedGlobalDeadlockClient()
{
    thread.retire();
    W_COERCE( blockedListMutex.acquire() );

    BlockedElem* blockedElem;
    while ((blockedElem = blockedList.pop()))  {
	delete blockedElem;
    }

    blockedListMutex.release();

    delete communicator;
}


rc_t CentralizedGlobalDeadlockClient::AssignClientId()
{
    W_DO( blockedListMutex.acquire() );    
    isClientIdAssigned = true;
    bool is_empty = blockedList.is_empty();
    blockedListMutex.release();
    
    if (!is_empty)  {
        W_DO( communicator->SendRequestDeadlockCheck() );
    }
    
    return RCOK;
}


rc_t CentralizedGlobalDeadlockClient::GlobalXctLockWait(lock_request_t* req, const char * blockname)
{
    timeout_in_ms    timeout = initialTimeout;
    rc_t    rc;

    W_DO( blockedListMutex.acquire() );
    BlockedElem* blockedElem = new BlockedElem(req);
    blockedList.push(blockedElem);
    blockedListMutex.release();

    /* Even though we block/ multiple times, the ONE prepare_to_block()
       that occurs in the lock manager is the one we care about.  If any
       one tries to loose us in the meantime, that release will remain
       until we notice it, or don't care */

    do  {
	DBGTHRD( << "waiting for global deadlock detection or lock, timeout=" << timeout );
	rc = me()->block(timeout, 0, blockname);
	if (rc.err_num() == stTIMEOUT)  {
	    if (blockedElem->collected || !isClientIdAssigned)  {
		timeout = WAIT_FOREVER;
	    }  else  {
	    	/* XXX does this leak elements from the list? */
		W_DO( communicator->SendRequestDeadlockCheck() );
		timeout = subsequentTimeout;
	    }
	}
    }  while (rc.err_num() == stTIMEOUT);

    if (rc)  {
	if (blockedElem->req->status() != lock_m::t_converting)  {
	    blockedElem->req->status(lock_m::t_aborted);
	}  else  {
	    blockedElem->req->status(lock_m::t_aborted_convert);
	}
    }

    W_COERCE( blockedListMutex.acquire() );
    delete blockedElem;
    blockedListMutex.release();

    return rc;
}


rc_t CentralizedGlobalDeadlockClient::UnblockGlobalXct(const gtid_t& gtid)
{
    W_COERCE( blockedListMutex.acquire() );

    BlockedListIter iter(blockedList);
    BlockedElem* blockedElem;
    while ((blockedElem = iter.next()))  {
	if (blockedElem->req->xd->gtid() && *blockedElem->req->xd->gtid() == gtid)  {
	    if (smlevel_0::deadlockEventCallback)  {
		smlevel_0::deadlockEventCallback->KillingGlobalXct(blockedElem->req->xd, blockedElem->req->get_lock_head()->name);
	    }
	    W_DO( blockedElem->req->thread->unblock(RC(smlevel_0::eDEADLOCK)) );
	}
    }

    blockedListMutex.release();

    return RCOK;
}


rc_t CentralizedGlobalDeadlockClient::SendWaitForGraph()
{
    WaitForList waitForList(WaitForElem::link_offset());

    W_DO( blockedListMutex.acquire() );

    BlockedListIter iter(blockedList);
    BlockedElem* blockedElem;
    while ((blockedElem = iter.next()))  {
	blockedElem->collected = true;
	W_DO( AddToWaitForList(waitForList, blockedElem->req, *blockedElem->req->xd->gtid()) );
    }

    blockedListMutex.release();

    W_DO( communicator->SendWaitForList(waitForList) );
    w_assert3(waitForList.is_empty());

    return RCOK;
}


rc_t CentralizedGlobalDeadlockClient::NewServerEndpoint()
{
    W_DO( SendRequestClientId() );

    return RCOK;
}


bool CentralizedGlobalDeadlockClient::Done()
{
    return communicator->Done();
}


rc_t CentralizedGlobalDeadlockClient::RcvAndDispatchMsg()
{
    return communicator->RcvAndDispatchMsg();
}


rc_t CentralizedGlobalDeadlockClient::SendQuit()
{
    return communicator->SendQuit();
}


rc_t CentralizedGlobalDeadlockClient::SendRequestClientId()
{
    return communicator->SendRequestClientId();
}


rc_t CentralizedGlobalDeadlockClient::AddToWaitForList(
	WaitForList&		waitForList,
	const lock_request_t*	req,
	const gtid_t&		gtid
)
{
    xct_t* xd = req->xd;
    w_assert3(xd);

    if (xd->lock_info()->wait != req)  {
	// This request is granted, but the thread which was blocked hasn't run yet
	// to take it off the list of global xct's which are blocked on a lock.
	w_assert3(req->status() == lock_m::t_granted);
	return RCOK;
    }

    w_assert3(req->status() == lock_m::t_waiting || req->status() == lock_m::t_converting);

    w_list_i<lock_request_t> iter(req->get_lock_head()->queue);

    if (req->status() == lock_m::t_waiting)  {
        mode_t req_mode = req->mode;
	lock_request_t* other;
	while ((other = iter.next()))  {
	    if (other->xd == xd)
		break;
	    if (other->status() == lock_m::t_aborted)
		continue;
	    if (!lock_base_t::compat[other->mode][req_mode] || other->status() == lock_m::t_waiting
					|| other->status() == lock_m::t_converting)  {
		if (other->xd->is_extern2pc())  {
		    waitForList.push(new WaitForElem(gtid, *other->xd->gtid()));
		}  else if (other->xd->lock_info()->wait)  {
		    AddToWaitForList(waitForList, other->xd->lock_info()->wait, gtid);
		}
	    }
	}
    }  else  {		// req->status() == lock_m::t_converting
        mode_t req_mode = xd->lock_info()->wait->convert_mode;
	lock_request_t* other;
	while ((other = iter.next()))  {
            if (other->xd == xd || other->status() == lock_m::t_aborted)
                continue;
            if (other->status() == lock_m::t_waiting)
                break;
            if (!lock_base_t::compat[other->mode][req_mode])  {
                if (other->xd->is_extern2pc())  {
                    waitForList.push(new WaitForElem(gtid, *other->xd->gtid()));
                }  else if (other->xd->lock_info()->wait)  {
                    AddToWaitForList(waitForList, other->xd->lock_info()->wait, gtid);
                }
	    }
	}
    }

    return RCOK;
}



/***************************************************************************
 *                                                                         *
 * CentralizedGlobalDeadlockClient::ReceiverThread class                   *
 *                                                                         *
 ***************************************************************************/

NORET
CentralizedGlobalDeadlockClient::ReceiverThread::ReceiverThread()
:   
    smthread_t(t_regular, false, false, "DeadlockClient"),
    deadlockClient(0)
{
}

void
CentralizedGlobalDeadlockClient::ReceiverThread::set(CentralizedGlobalDeadlockClient* client)
{
    deadlockClient = client;
}


NORET
CentralizedGlobalDeadlockClient::ReceiverThread::~ReceiverThread()
{
}


rc_t CentralizedGlobalDeadlockClient::ReceiverThread::Start()
{
    W_DO( deadlockClient->SendRequestClientId() );
    W_DO( this->fork() );
    return RCOK;
}


void CentralizedGlobalDeadlockClient::ReceiverThread::run()
{
    while (!deadlockClient->Done())  {
	W_COERCE( deadlockClient->RcvAndDispatchMsg() );
    }
}


void CentralizedGlobalDeadlockClient::ReceiverThread::retire()
{
    W_COERCE( deadlockClient->SendQuit() );
    this->wait();
}


/***************************************************************************
 *                                                                         *
 * DeadlockGraph class                                                     *
 *                                                                         *
 ***************************************************************************/

DeadlockGraph::DeadlockGraph(uint4_t initialNumberOfXcts)
:   maxId(0),
    maxUsedId(0),
    original(0),
    closure(0)
{
    w_assert1(initialNumberOfXcts % BitsPerWord == 0);
    w_assert1(initialNumberOfXcts > 0);

    maxId = initialNumberOfXcts;

    BitMapVector** newBitMap = new BitMapVector*[maxId];
    uint4_t i=0;
    for (i = 0; i < maxId; i++)  {
	newBitMap[i] = new BitMapVector(initialNumberOfXcts);
    }
    original = newBitMap;

    newBitMap = new BitMapVector*[maxId];
    for (i = 0; i < maxId; i++)  {
	newBitMap[i] = new BitMapVector(initialNumberOfXcts);
    }
    closure = newBitMap;
}


DeadlockGraph::~DeadlockGraph()
{
    uint4_t i;
    if (original)  {
	for (i = 0; i < maxId; i++)
	    delete original[i];
	
	delete [] original;
    }

    if (closure)  {
	for (i = 0; i < maxId; i++)
	    delete closure[i];
	
	delete [] closure;
    }
}


void DeadlockGraph::ClearGraph()
{
    uint4_t i;
    for (i = 0; i < maxId; i++)  {
	original[i]->ClearAll();
	closure[i]->ClearAll();
    }
    maxUsedId = 0;
}


void DeadlockGraph::Resize(uint4_t newSize)
{
    uint4_t i=0;
    if (newSize >= maxId)  {
	newSize = (newSize + BitsPerWord) / BitsPerWord * BitsPerWord;

	BitMapVector** newOriginal = new BitMapVector*[newSize];
	BitMapVector** newClosure = new BitMapVector*[newSize];
	for (i = 0; i < maxId; i++)  {
	    newOriginal[i] = original[i];
	    newClosure[i] = closure[i];
	}
	for (i = maxId; i < newSize; i++)  {
	    newOriginal[i] = new BitMapVector(newSize);
	    newClosure[i] = new BitMapVector(newSize);
	}

	delete [] original;
	delete [] closure;

	original = newOriginal;
	closure = newClosure;
	maxId = newSize;
    }  else  {
	w_assert3(0);
    }
}


void DeadlockGraph::AddEdge(uint4_t waitId, uint4_t forId)
{
    if (waitId >= maxId)  {
	Resize(waitId);
	w_assert3(waitId < maxId);
    }

    if (waitId > maxUsedId)
	maxUsedId = waitId;
    
    if (forId > maxUsedId)
	maxUsedId = forId;

    original[waitId]->SetBit(forId);
}


void DeadlockGraph::ComputeTransitiveClosure(IdList& cycleParticipantsList)
{
    uint4_t i;
    for (i = 0; i <= maxUsedId; i++)
	*closure[i] = *original[i];
    
    bool changed;
    do  {
	changed = false;
	for (uint4_t waitIndex = 0; waitIndex <= maxUsedId; waitIndex++)  {
	    int4_t forIndex = 0;
	    while ((forIndex = closure[waitIndex]->FirstSetOnOrAfter(forIndex)) != -1)  {
		closure[waitIndex]->OrInBitVector(*closure[forIndex], changed);
		forIndex++;
	    }
	}
    }  while (changed);

    for (i = 0; i <= maxUsedId; i++)  {
	if (QueryWaitsFor(i, i))
	    cycleParticipantsList.push(new IdElem(i));
    }
}


void DeadlockGraph::KillId(uint4_t id)
{
    if (id < maxId)
        original[id]->ClearAll();
}


bool DeadlockGraph::QueryWaitsFor(uint4_t waitId, uint4_t forId)
{
    if (waitId < maxId)
        return closure[waitId]->IsBitSet(forId);
    else
	return false;
}


/***************************************************************************
 *                                                                         *
 * CentralizedGlobalDeadlockServer class                                   *
 *                                                                         *
 ***************************************************************************/

CentralizedGlobalDeadlockServer::CentralizedGlobalDeadlockServer(DeadlockServerCommunicator* serverCommunicator)
:   communicator(serverCommunicator),
    state(IdleState),
    checkRequested(false),
    numGtids(0),
    gtidTable(127, offsetof(GtidTableElem, gtid), offsetof(GtidTableElem, _link)),
    idToGtid(0),
    idToGtidSize(0)
{
    thread.set(this);
    w_assert3(communicator);
    communicator->SetDeadlockServer(this);
    idToGtidSize = 32;
    idToGtid = new gtid_t*[idToGtidSize];
    w_assert1(idToGtid);
    for (uint i = 0; i < idToGtidSize; i++)
	idToGtid[i] = 0;
    W_COERCE( thread.Start() );
}


CentralizedGlobalDeadlockServer::~CentralizedGlobalDeadlockServer()
{
    thread.retire();
    delete communicator;
    delete [] idToGtid;
    GtidTableIter iter(gtidTable);
    GtidTableElem* tableElem;
    while ((tableElem = iter.next()))  {
	gtidTable.remove(tableElem);
	delete tableElem;
    }
}


bool CentralizedGlobalDeadlockServer::Done()
{
    return communicator->Done();
}


rc_t CentralizedGlobalDeadlockServer::RcvAndDispatchMsg()
{
    return communicator->RcvAndDispatchMsg();
}


rc_t CentralizedGlobalDeadlockServer::SendQuit()
{
    return communicator->SendQuit();
}


rc_t CentralizedGlobalDeadlockServer::AddClient(uint4_t id)
{
    w_assert3(!collectedIds.IsBitSet(id));
    w_assert3(!activeIds.IsBitSet(id));

    activeIds.SetBit(id); 
    if (state == CollectingState)  {
	W_DO( SendRequestWaitForList(id) );
    }

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::RemoveClient(uint4_t id)
{
    w_assert3(activeIds.IsBitSet(id));
    activeIds.ClearBit(id);
    collectedIds.ClearBit(id);

    GtidTableIter iter(gtidTable);
    GtidTableElem* tableElem;
    while ((tableElem = iter.next()))  {
	tableElem->nodeIds.ClearBit(id);
    }
    
    if (state == CollectingState && collectedIds == activeIds)  {
	W_DO( CheckDeadlock() );
    }

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::KillGtid(uint4_t clientId, const gtid_t& gtid)
{
    W_DO( communicator->SendKillGtid(clientId, gtid) );

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::CheckDeadlockRequested()
{
    if (state == IdleState)  {
	W_DO( BroadcastRequestWaitForList() );
    }  else  {
	checkRequested = true;
    }

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::BroadcastRequestWaitForList()
{
    w_assert3(state == IdleState);
    state = CollectingState;

    W_DO( communicator->BroadcastRequestWaitForList() );

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::SendRequestWaitForList(uint4_t id)
{
    W_DO( communicator->SendRequestWaitForList(id) );
    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::AddWaitFors(uint4_t clientId, WaitPtrForPtrList& waitForList, bool complete)
{
    w_assert3(state == CollectingState);
    w_assert3(activeIds.IsBitSet(clientId));
    w_assert3(!collectedIds.IsBitSet(clientId));

    WaitPtrForPtrElem* elem;
    while ((elem = waitForList.pop()))  {
	uint4_t waitId;
	uint4_t forId;

	GtidTableElem* gtidTableElem = GetGtidTableElem(*elem->waitGtid);
	gtidTableElem->nodeIds.SetBit(clientId);
	waitId = gtidTableElem->id;

	gtidTableElem = GetGtidTableElem(*elem->forGtid);
	forId = gtidTableElem->id;

	deadlockGraph.AddEdge(waitId, forId);

	delete elem;
    }

    if (complete)  {
        collectedIds.SetBit(clientId);
        if (collectedIds == activeIds)  {
	    state = SelectingVictimState;
            W_DO( CheckDeadlock() );
	}
    }

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::SelectVictim(IdList& deadlockedList)
{
    W_DO( communicator->SendSelectVictim(deadlockedList) );
    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::VictimSelected(const gtid_t& gtid)
{
    GtidTableElem* tableElem = gtidTable.lookup(gtid);
    w_assert1(tableElem);

    int4_t i = 0;
    while ((i = tableElem->nodeIds.FirstSetOnOrAfter(i)) != -1)  {
	KillGtid(i, gtid);
	i++;
    }

    deadlockGraph.KillId(tableElem->id);

    W_DO( CheckDeadlock() );
	
    return RCOK;
}

rc_t CentralizedGlobalDeadlockServer::CheckDeadlock()
{
    w_assert3(state == SelectingVictimState);
    IdList cycleParticipantList(IdElem::link_offset());
    deadlockGraph.ComputeTransitiveClosure(cycleParticipantList);

    if (cycleParticipantList.is_empty())  {
        W_DO( ResetServer() );
    }  else  {
        W_DO( SelectVictim(cycleParticipantList) );
    }
    w_assert3(cycleParticipantList.is_empty());

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::ResetServer()
{
    w_assert3(state == SelectingVictimState);

    deadlockGraph.ClearGraph();
    collectedIds.ClearAll();
    state = IdleState;

    numGtids = 0;
    GtidTableIter iter(gtidTable);
    GtidTableElem* tableElem;
    while ((tableElem = iter.next()))  {
	gtidTable.remove(tableElem);
	delete tableElem;
    }

    if (checkRequested)  {
	W_DO( BroadcastRequestWaitForList() );
    }

    checkRequested = false;

    return RCOK;
}


GtidTableElem* CentralizedGlobalDeadlockServer::GetGtidTableElem(const gtid_t& gtid)
{
    GtidTableElem* gtidTableElem = gtidTable.lookup(gtid);
    uint4_t i=0;

    if (gtidTableElem == 0)  {
	gtidTableElem = new GtidTableElem(gtid, numGtids);
	w_assert1(gtidTableElem);
	gtidTable.push(gtidTableElem);
	w_assert3(gtidTable.lookup(gtid));
    }

    if (numGtids >= idToGtidSize)  {
	uint4_t newIdToGtidSize = 2 * idToGtidSize;
	gtid_t** newIdToGtid = new gtid_t*[newIdToGtidSize];
	w_assert1(newIdToGtid);
	for (i = 0; i < idToGtidSize; i++)
	    newIdToGtid[i] = idToGtid[i];
	
	for (i = idToGtidSize; i < newIdToGtidSize; i++)
	    newIdToGtid[i] = 0;
	
	delete [] idToGtid;
	idToGtid = newIdToGtid;
	idToGtidSize = newIdToGtidSize;
    }

    idToGtid[gtidTableElem->id] = &gtidTableElem->gtid;
    numGtids++;
    
    return gtidTableElem;
}


const gtid_t& CentralizedGlobalDeadlockServer::IdToGtid(uint4_t id)
{
    w_assert3(id < idToGtidSize);
    return *idToGtid[id];
}


/***************************************************************************
 *                                                                         *
 * CentralizedGlobalDeadlockServer::ReceiverThread class                   *
 *                                                                         *
 ***************************************************************************/

NORET
CentralizedGlobalDeadlockServer::ReceiverThread::ReceiverThread()
:   
    smthread_t(t_regular, false, false, "DeadlockServer")
{
}

void
CentralizedGlobalDeadlockServer::ReceiverThread::set(CentralizedGlobalDeadlockServer* Server)
{
    deadlockServer = Server;
    w_assert3(deadlockServer);
}


NORET
CentralizedGlobalDeadlockServer::ReceiverThread::~ReceiverThread()
{
}


rc_t CentralizedGlobalDeadlockServer::ReceiverThread::Start()
{
    W_DO( this->fork() );
    return RCOK;
}


void CentralizedGlobalDeadlockServer::ReceiverThread::run()
{
    while (!deadlockServer->Done())  {
	W_COERCE( deadlockServer->RcvAndDispatchMsg() );
    }
}


void CentralizedGlobalDeadlockServer::ReceiverThread::retire()
{
    W_COERCE( deadlockServer->SendQuit() );
    this->wait();
}
#endif /* USE_COORD */

