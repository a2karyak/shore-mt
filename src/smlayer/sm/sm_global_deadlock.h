/*<std-header orig-src='shore' incl-file-exclusion='SM_GLOBAL_DEADLOCK_H'>

 $Id: sm_global_deadlock.h,v 1.33 2001/04/18 17:22:59 bolo Exp $

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

#ifndef SM_GLOBAL_DEADLOCK_H
#define SM_GLOBAL_DEADLOCK_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

#ifndef GLOBAL_DEADLOCK_H
#include <global_deadlock.h>
#endif
#ifndef MAPPINGS_H
#include <mappings.h>
#endif
#ifndef BITMAPVECTOR_H
#include <bitmapvector.h>
#endif


class BlockedElem  {
    public:
	lock_request_t*		    req;
	bool			    collected;
	w_link_t		    _link;

	NORET			    BlockedElem(lock_request_t* theReq)
		: req(theReq), collected(false)
		{};
	NORET			    ~BlockedElem()
		{
		    if (_link.member_of() != NULL)
			_link.detach();
		};
	static uint4_t		    link_offset()
		{
		    return offsetof(BlockedElem, _link);
		};
	W_FASTNEW_CLASS_DECL(BlockedElem);
};


class WaitForElem  {
    public:
	gtid_t			    waitGtid;
	gtid_t			    forGtid;
	w_link_t		    _link;

	NORET			    WaitForElem(
	    const gtid_t&		theWaitGtid,
	    const gtid_t&		theForGtid)
		:  waitGtid(theWaitGtid), forGtid(theForGtid)
		{};
	NORET                       ~WaitForElem()
		{
		    if (_link.member_of() != NULL)
		    _link.detach();
		};
	static uint4_t                link_offset()
		{
		    return offsetof(WaitForElem, _link);
		};
	W_FASTNEW_CLASS_DECL(WaitForElem);
};


class WaitPtrForPtrElem  {
    public:
	const gtid_t*		    waitGtid;
	const gtid_t*		    forGtid;
	w_link_t		    _link;

	NORET			    WaitPtrForPtrElem(
	    const gtid_t*		theWaitGtid,
	    const gtid_t*		theForGtid)
		:  waitGtid(theWaitGtid), forGtid(theForGtid)
		{};
	NORET                       ~WaitPtrForPtrElem()
		{
		    if (_link.member_of() != NULL)
		    _link.detach();
		};
	static uint4_t                link_offset()
		{
		    return offsetof(WaitPtrForPtrElem, _link);
		};
	W_FASTNEW_CLASS_DECL(WaitPtrForPtrElem);
};


class IdElem  {
    public:
	uint4_t			    id;
	w_link_t		    _link;

	NORET			    IdElem(uint4_t i)
					: id(i)
					{};
	NORET			    ~IdElem()
					{
					    if (_link.member_of() != NULL)
						_link.detach();
					};
	static uint4_t		    link_offset()
					{
					    return offsetof(IdElem, _link);
					};
	W_FASTNEW_CLASS_DECL(IdElem);
};



typedef w_list_t<BlockedElem>		BlockedList;
typedef w_list_i<BlockedElem>		BlockedListIter;
typedef w_list_t<WaitForElem>		WaitForList;
typedef w_list_i<WaitForElem>		WaitForListIter;
typedef w_list_t<WaitPtrForPtrElem>	WaitPtrForPtrList;
typedef w_list_i<WaitPtrForPtrElem>	WaitPtrForPtrListIter;
typedef w_list_t<IdElem>		IdList;
typedef w_list_i<IdElem>		IdListIter;



enum DeadlockMsgType    {
    msgInvalidType,
    msgVictimizerEndpoint,
    msgRequestClientId,
    msgAssignClientId,
    msgRequestDeadlockCheck,
    msgRequestWaitFors,
    msgWaitForList,
    msgSelectVictim,
    msgVictimSelected,
    msgKillGtid,
    msgQuit,
    msgClientEndpointDied,
    msgVictimizerEndpointDied,
    msgServerEndpointDied
};


struct DeadlockMsgHeader  {
    uint1_t			    msgType;
    uint1_t			    complete;
    uint2_t			    count;
    uint4_t			    clientId;
    enum 			    { noId = 0xFFFFFFFF };

    void			    hton();
    void			    ntoh();

    NORET			    DeadlockMsgHeader(
	DeadlockMsgType			type = msgInvalidType,
	uint4_t				id = noId,
	bool				isComplete = true,
	uint2_t				cnt = 0)
				    :
					msgType(type),
					complete(isComplete),
					count(cnt),
					clientId(id)
				    { }
			
};


struct DeadlockMsg  {
    enum			    { MaxNumGtidsPerMsg = 32 };
    enum			    { noId = DeadlockMsgHeader::noId };
    
    DeadlockMsgHeader		    header;
    gtid_t			    gtids[MaxNumGtidsPerMsg];

    void			    hton();
    void			    ntoh();

    static const char* msgNameStrings[];
    NORET                           DeadlockMsg(
	DeadlockMsgType                 type = msgInvalidType,
	uint4_t                         id = noId,
	bool                            isComplete = true,
	uint2_t                         cnt = 0)
				    :
                                        header(type, id, isComplete, cnt)
                                    { }
    void			    AddGtid(const gtid_t& gtid)
				    {
					w_assert1(header.count < MaxNumGtidsPerMsg);
					gtids[header.count++] = gtid;
				    }
    uint2_t			    GtidCount() const
				    {
					return header.count;
				    }
    void			    ResetCount()
				    {
					header.count = 0;
				    }
    uint2_t			    MaxGtidCount() const
				    {
					return MaxNumGtidsPerMsg;
				    }
    bool			    IsComplete() const
				    {
					return header.complete != 0;
				    }
    void			    SetComplete(bool status)
				    {
					header.complete = status;
				    }
    uint4_t			    ClientId() const
				    {
					return header.clientId;
				    }
    void			    SetClientId(uint4_t id)
				    {
					header.clientId = id;
				    }
    DeadlockMsgType		    MsgType() const
				    {
					return DeadlockMsgType(header.msgType);
				    }
};

ostream& operator<<(ostream& o, const DeadlockMsg& msg);


class CentralizedGlobalDeadlockClient;

class DeadlockClientCommunicator
{
    enum { noId = DeadlockMsgHeader::noId };
    public:
        friend CentralizedGlobalDeadlockClient;
        
	NORET			    DeadlockClientCommunicator(
	    const server_handle_t&	theServerHandle,
	    name_ep_map*		ep_map,
	    CommSystem&			commSystem);
	NORET			    ~DeadlockClientCommunicator();
	bool			    Done() const
					{
					    return done;
					};

    private:
	rc_t			    SendRequestClientId();
	rc_t			    ReceivedAssignClientId(uint4_t clientId);
	rc_t			    SendRequestDeadlockCheck();
	rc_t			    SendWaitForList(WaitForList& waitForList);
	rc_t			    ReceivedRequestWaitForList();
	rc_t			    ReceivedKillGtid(const gtid_t& gtid);
	rc_t			    SendQuit();
	rc_t			    SendClientEndpointDied();
	rc_t			    ReceivedServerEndpointDied(Endpoint& theServerEndpoint);
	void			    SetDeadlockClient(CentralizedGlobalDeadlockClient * client);
	
	CentralizedGlobalDeadlockClient*	deadlockClient;

	const server_handle_t	    serverHandle;
	name_ep_map*		    endpointMap;

	Endpoint		    serverEndpoint;
	Endpoint		    myEndpoint;

	uint4_t			    myId;

	Buffer			    sendBuffer;
	Buffer			    rcvBuffer;
	Buffer			    serverEndpointDiedBuffer;
	DeadlockMsg*		    sendMsg;
	DeadlockMsg*		    rcvMsg;
	CommSystem&		    comm;
	bool			    done;
	bool			    serverEndpointValid;

	rc_t			    SendMsg();
	rc_t			    SendMsg(DeadlockMsg &msg, Buffer &buf);
	rc_t			    RcvAndDispatchMsg();
};


class DeadlockVictimizerCallback
{
    public:
	virtual gtid_t operator()(GtidList& gtidList) = 0;
};


class PickFirstDeadlockVictimizerCallback : public DeadlockVictimizerCallback
{
    public:
	gtid_t operator()(GtidList& gtidList)
	    {
		w_assert3(!gtidList.is_empty());
		return gtidList.top()->gtid;
	    };
};


class PickLatestDtidDeadlockVictimizerCallback : public DeadlockVictimizerCallback
{
    public:
	gtid_t operator()(GtidList& gtidList);
};


class CentralizedGlobalDeadlockServer;
extern PickFirstDeadlockVictimizerCallback selectFirstVictimizer;
extern PickLatestDtidDeadlockVictimizerCallback latestDtidVictimizer;

class DeadlockServerCommunicator
{
    enum { noId = DeadlockMsgHeader::noId };
    public:
        friend CentralizedGlobalDeadlockServer;
        
	NORET			    DeadlockServerCommunicator(
	    const server_handle_t&	theServerHandle,
	    name_ep_map*		ep_map,
	    CommSystem&			theCommSystem,
	    DeadlockVictimizerCallback& theCallback = selectFirstVictimizer);
	NORET			    ~DeadlockServerCommunicator();
	bool			    Done() const
					{
					    return done;
					}
	
    private:
	rc_t			    ReceivedVictimizerEndpoint(Endpoint& ep);
	rc_t			    ReceivedRequestClientId(Endpoint& ep);
	rc_t			    SendAssignClientId(uint4_t clientId);
	rc_t			    ReceivedRequestDeadlockCheck();
	rc_t			    BroadcastRequestWaitForList();
	rc_t			    SendRequestWaitForList(uint4_t clientId);
	rc_t			    ReceivedWaitForList(uint4_t clientId, uint2_t count, const gtid_t* gtids, bool complete);
	rc_t			    SendSelectVictim(IdList& deadlockList);
	rc_t			    ReceivedVictimSelected(const gtid_t gtid);
	rc_t			    SendKillGtid(uint4_t clientId, const gtid_t &gtid);
	rc_t			    SendQuit();
	rc_t			    BroadcastServerEndpointDied();
	rc_t			    ReceivedClientEndpointDied(Endpoint& theClientEndpoint);
	rc_t			    ReceivedVictimizerEndpointDied(Endpoint& theVictimimzerEndpoint);
	void			    SetDeadlockServer(CentralizedGlobalDeadlockServer* server);
	CentralizedGlobalDeadlockServer*	deadlockServer;

	const server_handle_t	    serverHandle;
	name_ep_map*		    endpointMap;

	CommSystem&		    commSystem;
	Endpoint		    myEndpoint;
	Endpoint		    victimizerEndpoint;
	enum			    { InitialNumberOfEndpoints = 20 };
	uint4_t			    clientEndpointsSize;
	Endpoint*		    clientEndpoints;

	Buffer			    sendBuffer;
	Buffer			    rcvBuffer;
	Buffer			    clientEndpointDiedBuffer;
	Buffer			    victimizerEndpointDiedBuffer;
	DeadlockMsg*		    sendMsg;
	DeadlockMsg*		    rcvMsg;
	BitMapVector		    activeClientIds;
	DeadlockVictimizerCallback& selectVictim;
	bool			    remoteVictimizer;
	bool			    done;
	
	enum MsgDestination	    { toVictimizer, toClient, toServer };
	rc_t			    SendMsg(MsgDestination destination = toClient);
	rc_t			    RcvAndDispatchMsg();
	void			    SetClientEndpoint(uint4_t clientId, Endpoint& ep);
	void			    ResizeClientEndpointsArray(uint4_t newSize);
};


class DeadlockVictimizerCommunicator
{
    public:
	NORET			    DeadlockVictimizerCommunicator(
	    const server_handle_t&	theServerHandle,
	    name_ep_map*		ep_map,
	    CommSystem&			commSystem,
	    DeadlockVictimizerCallback& theCallback);
	~DeadlockVictimizerCommunicator();
	rc_t			    SendVictimizerEndpoint();
	rc_t			    ReceivedSelectVictim(uint2_t count, gtid_t* gtids, bool complete);
	rc_t			    SendVictimSelected(const gtid_t& gtid);
	rc_t			    ReceivedServerEndpointDied(Endpoint& theServerEndpoint);
    
    private:
	const server_handle_t	    serverHandle;
	name_ep_map*		    endpointMap;

	Endpoint		    serverEndpoint;
	Endpoint		    myEndpoint;

	Buffer			    sendBuffer;
	Buffer			    rcvBuffer;
	Buffer			    serverEndpointDiedBuffer;
	DeadlockMsg*		    sendMsg;
	DeadlockMsg*		    rcvMsg;
	GtidList		    gtidList;
	DeadlockVictimizerCallback& selectVictim;
	bool			    done;
	bool			    serverEndpointValid;

	rc_t			    SendMsg();
	rc_t			    RcvAndDispatchMsg();
};


class CentralizedGlobalDeadlockClient : public GlobalDeadlockClient  {
    private:
        class ReceiverThread : public smthread_t
        {
	    public:
		NORET ReceiverThread();
		NORET ~ReceiverThread();
		rc_t Start();
		void run();
		void retire();
		void set(CentralizedGlobalDeadlockClient* client);

	    private:
		CentralizedGlobalDeadlockClient* deadlockClient;
        };

    public:
	NORET			    CentralizedGlobalDeadlockClient(
	    int4_t			initialTimeout,
	    int4_t			subsequentTimeout,
	    DeadlockClientCommunicator* clientCommunicator);
	NORET			    ~CentralizedGlobalDeadlockClient();
	rc_t			    AssignClientId();
        rc_t    		    GlobalXctLockWait(
	    lock_request_t*		req,         
	    const char *		blockname);
        rc_t    		    UnblockGlobalXct(
	    const gtid_t&		gtid);
        rc_t    		    SendWaitForGraph();
        rc_t			    NewServerEndpoint();

	bool			    Done();
	rc_t			    RcvAndDispatchMsg();
	rc_t			    SendQuit();
	rc_t			    SendRequestClientId();

    private:
	int4_t			    initialTimeout;
	int4_t			    subsequentTimeout;
	DeadlockClientCommunicator* communicator;
	ReceiverThread		    thread;

	smutex_t		    blockedListMutex;
	BlockedList		    blockedList;
	bool			    isClientIdAssigned;

	rc_t			    AddToWaitForList(
	    WaitForList&		waitForList,
	    const lock_request_t*	req,
	    const gtid_t&		gtid);


	/* disallow copying and assignment */
	NORET					    CentralizedGlobalDeadlockClient(
	    const CentralizedGlobalDeadlockClient&);
	CentralizedGlobalDeadlockClient& 	    operator=(
	    const CentralizedGlobalDeadlockClient&);
};


class DeadlockGraph  {
    public:
	NORET			    DeadlockGraph(uint4_t initialNumberOfXcts = 32);
	NORET			    ~DeadlockGraph();
	void			    ClearGraph();
	void			    AddEdge(uint4_t waitId, uint4_t forId);
	void			    ComputeTransitiveClosure(IdList& cycleParticipantsList);
	void			    KillId(uint4_t id);
	bool			    QueryWaitsFor(uint4_t waitId, uint4_t forId);

    private:
	uint4_t			    maxId;
	uint4_t			    maxUsedId;
	BitMapVector**		    original;
	BitMapVector**		    closure;
	enum			    { BitsPerWord = sizeof(uint4_t) * CHAR_BIT };

	void			    Resize(uint4_t newSize);

    private:	/* disabled */
    	DeadlockGraph(const DeadlockGraph &);
	const DeadlockGraph &operator=(const DeadlockGraph &);
};


class GtidTableElem  {
    public:
	NORET			    GtidTableElem(gtid_t g, uint4_t i)
					: gtid(g), id(i)
					{};
	gtid_t			    gtid;
	uint4_t			    id;
	BitMapVector		    nodeIds;
	w_link_t		    _link;

	W_FASTNEW_CLASS_DECL(GtidTableElem);

    private: /* disabled */
    	GtidTableElem(const GtidTableElem &);
	const GtidTableElem &operator=(const GtidTableElem &);
};

typedef w_hash_t<GtidTableElem, const gtid_t> GtidTable;
typedef w_hash_i<GtidTableElem, const gtid_t> GtidTableIter;

w_base_t::uint4_t w_hash(const gtid_t &g);


class CentralizedGlobalDeadlockServer  {
    private:
	friend DeadlockServerCommunicator;
        class ReceiverThread : public smthread_t
        {
	    public:
		NORET ReceiverThread();
		NORET ~ReceiverThread();
		rc_t Start();
                void run();
                void retire();
		void set(CentralizedGlobalDeadlockServer* client);
	    private:
		CentralizedGlobalDeadlockServer* deadlockServer;
        };

    public:
	NORET			    CentralizedGlobalDeadlockServer(DeadlockServerCommunicator* serverCommunicator);
	NORET			    ~CentralizedGlobalDeadlockServer();

	bool			    Done();
	rc_t			    RcvAndDispatchMsg();
	rc_t			    SendQuit();

    private:
	rc_t			    AddClient(uint4_t id);
	rc_t			    RemoveClient(uint4_t id);
	rc_t			    KillGtid(uint4_t clientId, const gtid_t& gtid);
	rc_t			    CheckDeadlockRequested();
	rc_t			    BroadcastRequestWaitForList();
	rc_t			    SendRequestWaitForList(uint4_t id);
	rc_t			    AddWaitFors(uint4_t clientId, WaitPtrForPtrList& waitForList, bool complete);
	rc_t			    SelectVictim(IdList& deadlockedList);
	rc_t			    VictimSelected(const gtid_t& gtid);
	rc_t			    CheckDeadlock();
	rc_t			    ResetServer();
	GtidTableElem*		    GetGtidTableElem(const gtid_t& gtid);
	const gtid_t&		    IdToGtid(uint4_t id);

	DeadlockGraph		    deadlockGraph;
	DeadlockServerCommunicator* communicator;
	ReceiverThread		    thread;
	enum State		    {IdleState, CollectingState, SelectingVictimState};
	State			    state;
	bool			    checkRequested;
	BitMapVector		    activeIds;
	BitMapVector		    collectedIds;
	uint4_t			    numGtids;
	GtidTable		    gtidTable;
	gtid_t**                    idToGtid;
	uint4_t			    idToGtidSize;
};

/*<std-footer incl-file-exclusion='SM_GLOBAL_DEADLOCK_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
