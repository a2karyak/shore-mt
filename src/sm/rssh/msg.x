# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

struct ssh_cmd_t {
    char line<>;
};

struct ssh_reply_t {
    int result;
    int got_partial;
    char output<>;
};

program RSSHPROG {
    version RSSHVERS {
	ssh_reply_t perform(ssh_cmd_t) = 100;
    } = 1;
} = 0x20000100;

#ifdef RPC_HDR
%#ifdef RPC_SVC
%#ifdef __cplusplus
%extern "C" ssh_reply_t* perform_1(ssh_cmd_t*, svc_req*);
%#endif /*__cplusplus*/
%#endif /*RPC_SVC*/
%#ifdef RPC_CLNT
%#ifdef __cplusplus
%extern "C" ssh_reply_t* perform_1(ssh_cmd_t*, CLIENT*);
%#endif /*__cplusplus*/
%#endif /*RPC_CLNT*/
#endif /*RPC_HDR*/
