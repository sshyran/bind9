/* $Id: socket.h,v 1.3 1998/11/10 01:56:44 explorer Exp $ */

#ifndef ISC_SOCKET_H
#define ISC_SOCKET_H 1

/*****
 ***** Module Info
 *****/

/*
 * Sockets
 *
 * Provides TCP and UDP sockets for network I/O.  The sockets are event
 * sources in the task system.
 *
 * When I/O completes, a completion event for the socket is posted to the
 * event queue of the task which requested the I/O.
 *
 * MP:
 *	The module ensures appropriate synchronization of data structures it
 *	creates and manipulates.
 *
 *	Clients of this module must not be holding a socket's task's lock when
 *	making a call that affects that socket.  Failure to follow this rule
 *	can result in deadlock.
 *
 *	The caller must ensure that isc_socketmgr_destroy() is called only
 *	once for a given manager.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	<TBS>
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */

/***
 *** Imports
 ***/

#include <isc/boolean.h>
#include <isc/result.h>
#include <isc/event.h>

#include <isc/task.h>
#include <isc/region.h>
#include <isc/memcluster.h>

#include <netinet/in.h>

/***
 *** Types
 ***/

typedef struct isc_socket *isc_socket_t;
typedef struct isc_socketmgr *isc_socketmgr_t;

/*
 * XXX Export this as isc/sockaddr.h
 */
typedef struct isc_sockaddr {
	/*
	 * XXX  Must be big enough for all sockaddr types we care about.
	 */
	union {
		struct sockaddr_in sin;
	} type;
} *isc_sockaddr_t;

typedef struct isc_socketevent {
	struct isc_event	common;		/* Sender is the socket. */
	isc_result_t		result;		/* OK, EOF, whatever else */
	unsigned int		n;		/* bytes read or written */
	struct isc_region	region;		/* the region info */
	struct isc_sockaddr	address;	/* source address */
	int			addrlength;	/* length of address */
} *isc_socketevent_t;

typedef struct isc_socket_newconev {
	struct isc_event	common;
	isc_socket_t		newsocket;
} *isc_socket_newconnev_t;

#define ISC_SOCKEVENT_ANYEVENT  (0)
#define ISC_SOCKEVENT_RECVDONE	(ISC_EVENTCLASS_SOCKET + 1)
#define ISC_SOCKEVENT_SENDDONE	(ISC_EVENTCLASS_SOCKET + 2)
#define ISC_SOCKEVENT_NEWCONN	(ISC_EVENTCLASS_SOCKET + 3)
#define ISC_SOCKEVENT_CONNECTED	(ISC_EVENTCLASS_SOCKET + 4)
#define ISC_SOCKEVENT_RECVMARK	(ISC_EVENTCLASS_SOCKET + 5)
#define ISC_SOCKEVENT_SENDMARK	(ISC_EVENTCLASS_SOCKET + 6)

/*
 * Internal events.
 */
#define ISC_SOCKEVENT_INTIO	(ISC_EVENTCLASS_SOCKET + 257)
#define ISC_SOCKEVENT_INTCONN	(ISC_EVENTCLASS_SOCKET + 258)


typedef enum {
	isc_socket_udp,
	isc_socket_tcp
} isc_sockettype_t;

typedef enum {
	isc_sockshut_reading,
	isc_sockshut_writing,
	isc_sockshut_all
} isc_socketshutdown_t;

/***
 *** Socket and Socket Manager Functions
 ***
 *** Note: all Ensures conditions apply only if the result is success for
 *** those functions which return an isc_result.
 ***/

isc_result_t
isc_socket_create(isc_socketmgr_t manager,
		  isc_sockettype_t type,
		  isc_socket_t *socketp);
/*
 * Create a new 'type' socket managed by 'manager'.
 *
 * Requires:
 *
 *	'manager' is a valid manager
 *
 *	'socketp' is a valid pointer, and *socketp == NULL
 *
 * Ensures:
 *
 *	'*socketp' is attached to the newly created socket
 *
 * Returns:
 *
 *	Success
 *	No memory
 *	Unexpected error
 */


void 
isc_socket_shutdown(isc_socket_t socket, isc_task_t task,
		    isc_socketshutdown_t how);
/*
 * Shutdown 'socket' according to 'how'.
 *
 * Note: if 'task' is NULL, then the shutdown applies to all tasks using the
 * socket.
 *
 * Requires:
 *
 *	'socket' is a valid socket.
 *
 *	'task' is NULL or is a valid task.
 *
 * Ensures:
 *
 *	If 'how' is 'isc_sockshut_reading' or 'isc_sockshut_all' then
 *
 *		Any pending read completion events for the task are
 *		removed from the task's event queue.
 *
 *		No further read completion events will be delivered to the
 *		task.
 *
 *		No further read requests may be made.
 *
 *	If 'how' is 'isc_sockshut_writing' or 'isc_sockshut_all' then
 *
 *		Any pending write completion events for the task are
 *		removed from the task's event queue.
 *
 *		No further write completion events will be delivered to the
 *		task.
 *
 *		No further write requests may be made.
 *
 *		If 'socket' is a TCP socket, then when the last currently
 *		pending write completes, TCP FIN will sent to the remote peer.
 */

void
isc_socket_attach(isc_socket_t socket, isc_socket_t *socketp);
/*
 * Attach *socketp to socket.
 *
 * Requires:
 *
 *	'socket' is a valid socket.
 *
 *	'socketp' points to a NULL socket.
 *
 * Ensures:
 *
 *	*socketp is attached to socket.
 */

void 
isc_socket_detach(isc_socket_t *socketp);
/*
 * Detach *socketp from its socket.
 *
 * Notes:
 *
 * 	Detaching the last reference may cause any still-pending I/O to be
 *	cancelled.
 * 
 * Requires:
 *
 *	'socketp' points to a valid socket.
 *
 * Ensures:
 *
 *	*socketp is NULL.
 *
 *	If '*socketp' is the last reference to the socket,
 *	then:
 *
 *		The socket will be shutdown (both reading and writing)
 *		for all tasks.
 *
 *		All resources used by the socket have been freed
 */

isc_result_t
isc_socket_bind(isc_socket_t socket, struct isc_sockaddr *addressp,
		int length);
/*
 * Bind 'socket' to '*addressp'.
 *
 * Requires:
 *
 *	'socket' is a valid socket
 *
 *	'addressp' points to a valid isc_sockaddr.
 *
 *	'length' is approprate for the isc_sockaddr type.
 *
 * Returns:
 *
 *	Success
 *	Address not available
 *	Address in use
 *	Permission denied
 *	Unexpected error
 */

isc_result_t
isc_socket_listen(isc_socket_t socket, int backlog,
		  isc_task_t task, isc_taskaction_t action, void *arg);
/*
 * Listen on 'socket'.  Every time a new connection request arrives,
 * a NEWCONN event with action 'action' and arg 'arg' will be posted
 * to the event queue for 'task'.
 *
 * Notes:
 *
 *	'backlog' is as in the UNIX system call listen().
 *
 * Requires:
 *
 *	'socket' is a valid TCP socket.
 *
 *	'task' is a valid task
 *
 *	'action' is a valid action
 *
 * Returns:
 *
 *	Success
 *	Unexpected error
 */

void
isc_socket_hold(isc_socket_t socket);
/*
 * Put a TCP listener socket on hold.  No NEWCONN events will be posted
 * 
 * Notes:
 *
 *	While 'on hold', new connection requests will be queued or dropped
 *	by the operating system.
 *
 * Requires:
 *
 *	'socket' is a valid TCP socket
 *
 *	Some task is listening on the socket.
 *
 */

void
isc_socket_unhold(isc_socket_t socket);
/*
 * Restore normal NEWCONN event posting.
 * 
 * Requires:
 *
 *	'socket' is a valid TCP socket
 *
 *	Some task is listening on the socket.
 *
 *	'socket' is holding.
 *
 */

isc_result_t
isc_socket_accept(isc_socket_t s1, isc_socket_t *s2p);
/*
 * Accept a connection from 's1', creating a new socket for the connection
 * and attaching '*s2p' to it.
 *
 * Requires:
 *
 *	'socket' is a valid TCP socket.
 *
 *	s2p is a valid pointer, and *s2p == NULL;
 *
 * Ensures:
 *
 *	*s2p is attached to the newly created socket.
 *
 * Returns:
 *
 *	Success
 *	No memory
 *	No pending connection requests
 *	Unexpected error
 */

isc_result_t
isc_socket_connect(isc_socket_t socket, struct isc_sockaddr *addressp,
		   int length, isc_task_t task, isc_taskaction_t action,
		   void *arg);
/*
 * Connect 'socket' to peer with address *saddr.  When the connection
 * succeeds, or when an error occurs, a CONNECTED event with action 'action'
 * and arg 'arg' will be posted to the event queue for 'task'.
 *
 * Requires:
 *
 *	'socket' is a valid TCP socket
 *
 *	'addressp' points to a valid isc_sockaddr
 *
 *	'length' is approprate for the isc_sockaddr type
 *
 *	'task' is a valid task
 *
 *	'action' is a valid action
 *
 * Returns:
 *
 *	Success
 *	No memory
 *	Address not available
 *	Address in use
 *	Host unreachable
 *	Network unreachable
 *	Connection refused
 *	Unexpected error
 */

isc_result_t
isc_socket_getpeername(isc_socket_t socket, struct isc_sockaddr *addressp,
		       int *lengthp);
/*
 * Get the name of the peer connected to 'socket'.
 *
 * Requires:
 *
 *	'socket' is a valid TCP socket.
 *
 *	'addressp' points to '*lengthp' bytes.
 *
 * Returns:
 *
 *	Success
 *	Address buffer too small
 */

isc_result_t
isc_socket_getsockname(isc_socket_t socket, struct isc_sockaddr *addressp,
		       int *lengthp);
/*
 * Get the name of 'socket'.
 *
 * Requires:
 *
 *	'socket' is a valid socket.
 *
 *	'addressp' points to '*lengthp' bytes.
 *
 * Returns:
 *
 *	Success
 *	Address buffer too small
 */

isc_result_t
isc_socket_recv(isc_socket_t socket, isc_region_t region,
		isc_boolean_t partial,
		isc_task_t task, isc_taskaction_t action, void *arg);
/*
 * Receive from 'socket', storing the results in region.
 *
 * Notes:
 *
 *	Let 'length' refer to the length of 'region'.
 *
 *	If 'partial' is true, then at most 'length' bytes will be read.
 *	Otherwise the read will not complete until exactly 'length' bytes
 *	have been read.
 *
 *	The read will complete when the desired number of bytes have been
 *	read, if end-of-input occurs, or if an error occurs.  A read done
 *	event with the given 'action' and 'arg' will be posted to the
 *	event queue of 'task'.
 *
 *	The caller may neither read from nor write to 'region' until it
 *	has received the read completion event.
 *
 * Requires:
 *
 *	'socket' is a valid socket
 *
 *	'region' is a valid region
 *
 *	'task' is a valid task
 *
 *	action != NULL and is a valid action
 *
 * Returns:
 *
 *	Success
 *	No memory
 *	Unexpected error
 */

isc_result_t
isc_socket_send(isc_socket_t socket, isc_region_t region,
		isc_task_t task, isc_taskaction_t action, void *arg);
/*
 * Send the contents of 'region' to the socket's peer.
 *
 * Notes:
 *
 *	Shutting down the requestor's task *may* result in any
 *	still pending writes being dropped.
 *
 *	If 'action' is NULL, then no completion event will be posted.
 *
 *	The caller may neither read from nor write to 'region' until it
 *	has received the write completion event, or all references to the
 *	socket have been detached.
 *
 * Requires:
 *
 *	'socket' is a valid socket
 *
 *	'region' is a valid region
 *
 *	'task' is a valid task
 *
 *	action == NULL or is a valid action
 *
 * Returns:
 *
 *	Success
 *	No memory
 *	Unexpected error
 */

/* XXX this is some of how to do a read
 *	generate new net_request
 *	generate new read-result net_event
 *	attach to requestor
 *	lock socket
 *	queue request
 *	unlock socket
 */

isc_result_t
isc_socketmgr_create(isc_memctx_t mctx, isc_socketmgr_t *managerp);
/*
 * Create a socket manager.
 *
 * Notes:
 *
 *	All memory will be allocated in memory context 'mctx'.
 *
 * Requires:
 *
 *	'mctx' is a valid memory context.
 *
 *	'managerp' points to a NULL isc_socketmgr_t.
 *
 * Ensures:
 *
 *	'*managerp' is a valid isc_socketmgr_t.
 *
 * Returns:
 *
 *	Success
 *	No memory
 *	Unexpected error
 */

void
isc_socketmgr_destroy(isc_socketmgr_t *);
/*
 * Destroy a socket manager.
 *
 * Notes:
 *	
 *	This routine blocks until there are no sockets left in the manager,
 *	so if the caller holds any socket references using the manager, it
 *	must detach them before calling isc_socketmgr_destroy() or it will
 *	block forever.
 *
 * Requires:
 *
 *	'*managerp' is a valid isc_socketmgr_t.
 *
 * Ensures:
 *
 *	*managerp == NULL
 *
 *	All resources used by the manager have been freed.
 */

#endif /* ISC_SOCKET_H */
