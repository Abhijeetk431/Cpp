/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
   // cout << "Finishing up the node" << endl;
   memberNode->inited=false;
   memberNode->inGroup=false;
   memberNode->nnb=0;
   memberNode->heartbeat=0;
   memberNode->pingCounter=TFAIL;
   memberNode->timeOutCounter=-1;
   initMemberListTable(memberNode);
   // cout << "Finishing complete" << endl;
   return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * @brief A method to print the membership list of node for debugging purposes
 * 
 */
// void MP1Node::printstack(){
//     string adr = memberNode->addr.getAddress();
//     cout << "This is a member at ";
//     cout << adr << endl;
//     cout << "It has: ";
//     cout << memberNode->memberList.size();
//     cout << " members in the Membership List." << endl;
//     int i = 0;
//     MemberListEntry a_member;
//     for (std::vector<MemberListEntry>::iterator a_member = memberNode->memberList.begin(); a_member != memberNode -> memberList.end(); ++a_member){
//         i++;
//         cout << "Member number ";
//         cout << i;
//         cout << ">";
//         cout << a_member->id;
//         cout << ":";
//         cout << a_member->port;
//         cout << " HB:";
//         cout << a_member->heartbeat;
//         cout << " TS:";
//         cout << a_member->timestamp << endl;
//     }

// }
/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
    // cout << "MsgRecv Callback ";
    MessageHdr* msg=(MessageHdr*) malloc(size * sizeof(char));
    memcpy(msg, data, sizeof(MessageHdr));
    if(msg->msgType == JOINREQ){
        // cout << "-------------------JOINREQ-------------------------" << endl;
        // printstack();
        int id;
        short port;
        long heartbeat;
        memcpy(&id, data+sizeof(MessageHdr), sizeof(int));
        memcpy(&port, data+sizeof(MessageHdr)+sizeof(int), sizeof(short));
        memcpy(&heartbeat, data+sizeof(MessageHdr)+sizeof(int)+sizeof(short), sizeof(long));
        addToMembershipList(id, port, heartbeat, memberNode->timeOutCounter);
        Address requestee_addr = makeAddress(id, port);
        sendJoinResponse(&requestee_addr);
        // cout << "------------------Changes to------------------------" << endl;
        // printstack();
        // cout << "-------------Responded a joinreq--------------------" << endl;
    }
    if(msg->msgType == JOINREP){
        // cout << "-------------------JOINREP---------------------------" << endl;
        // printstack();
        memberNode->inGroup=true;
        memberListDeserializer(data);
        // cout << "------------------Changes to------------------------" << endl;
        // printstack();
        // cout << "------------REsponded a joinrep---------------------" << endl;
    }
    if(msg->msgType == HEARTBEAT){
        // cout << "--------------------HEARTBEAT-------------------------" << endl;
        // printstack();
        int id;
        short port;
        long heartbeat;
        memcpy(&id, data+sizeof(MessageHdr), sizeof(int));
        memcpy(&port, data+sizeof(MessageHdr)+sizeof(int), sizeof(short));
        memcpy(&heartbeat, data+sizeof(MessageHdr)+sizeof(int)+sizeof(short),sizeof(long));
        if(!inMembershipList(id)){
            addToMembershipList(id, port, heartbeat, memberNode->timeOutCounter);
        }
        MemberListEntry* member = fetchNodeFromList(id);
        member->setheartbeat(heartbeat);
        member->settimestamp(memberNode->timeOutCounter);
        // cout << "--------------------Changes to------------------------" << endl;
        // printstack();
        // cout << "----------------Respomded to Heartbeat message---------" << endl;
    }
    return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	/*
	 * Your code goes here
	 */
    if(memberNode->pingCounter==0){
        memberNode->heartbeat++;
        for(std::vector<MemberListEntry>::iterator a_member= memberNode->memberList.begin(); a_member!=memberNode->memberList.end(); ++a_member){
            Address targetMember = makeAddress(a_member->id, a_member->port);
            if(!isSame(&targetMember)){
                sendHeartbeats(&targetMember);
            }
        }
        memberNode->pingCounter=TFAIL;
    }
    else{
        memberNode->pingCounter--;
    }
    for(std::vector<MemberListEntry>::iterator a_member= memberNode->memberList.begin(); a_member!=memberNode->memberList.end(); ++a_member){
        Address targetMember = makeAddress(a_member->id, a_member->port);
        if(!isSame(&targetMember)){
            if(memberNode->timeOutCounter - a_member->timestamp > TREMOVE){
                // cout << "########################FAILURE DETECTED###########################" << endl;
                memberNode->memberList.erase(a_member);
                #ifdef DEBUGLOG
                log->logNodeRemove(&memberNode->addr, &targetMember);
                #endif
                break;
            }
        }
    }
    memberNode->timeOutCounter++;
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * @brief Compares address and responds true if address is same
 * 
 * @param address 
 * @return true - If address is same as current Member node's address
 * @return false - If address is not same as current Member node's address
 */
bool MP1Node::isSame(Address * address){
    return (memcmp((char * ) & (memberNode -> addr.addr), (char * ) & (address -> addr), sizeof(memberNode -> addr.addr)) == 0);
}

/**
 * @brief A Method to check if node exists in Membership list
 * 
 * @param id - of the process to check if exists in membership list
 * @return true - if the member exists in membership list
 * @return false - if the member does not exist in the membership list
 */
bool MP1Node::inMembershipList(int id){
    return (this -> fetchNodeFromList(id) != NULL);
}

/**
 * @brief A method to fetch a particular member from membership list
 * 
 * @param id - of the member to fetch from list
 * @return MemberListEntry* 
 */

MemberListEntry * MP1Node::fetchNodeFromList(int id){
    MemberListEntry * node = NULL;
    for (std::vector < MemberListEntry > ::iterator a_member = memberNode -> memberList.begin(); a_member != memberNode -> memberList.end(); ++a_member) {
        if (a_member->id == id) {
            node = a_member.base();
            break;
        }
    }
    return node;
}

/**
 * @brief A method to create and return an Address Object
 * 
 * @param id - to be used to create the Address
 * @param port - to be used to create the Address
 * @return Address 
 */
Address MP1Node::makeAddress(int id, short port){
    Address memaddr;
    memset( & memaddr, 0, sizeof(Address));
    *(int * )( & memaddr.addr) = id;
    *(short * )( & memaddr.addr[4]) = port;
    return memaddr;
}

/**
 * @brief A method to add member to membership list
 * 
 * @param id - of the member to add
 * @param port - of the member to add
 * @param heartbeat - of the member to add
 * @param timestamp - of the member to add
 */
void MP1Node::addToMembershipList(int id, short port, long heartbeat, long timestamp){
    // If node is not in membership list then add new member in Membership List    
    if (!inMembershipList(id)) {        
        MemberListEntry* newMember = new MemberListEntry(id, port, heartbeat, timestamp);            
        memberNode->memberList.insert(memberNode->memberList.end(), *newMember);
        #ifdef DEBUGLOG
        Address newMemberAddr = makeAddress(id, port);
        log->logNodeAdd(&memberNode->addr, &newMemberAddr);
        #endif
        delete newMember;
    }
}

/**
 * @brief A Method to remove a member from Membership list
 * 
 * @param id - of the meber to remove from membership list
 * @param port - of the member to remove from the membership list
 */
void MP1Node::removeFromMembershipList(int id, short port){
    for (std::vector < MemberListEntry > ::iterator a_member = memberNode -> memberList.begin(); a_member != memberNode -> memberList.end(); ++a_member){
        if (a_member->id == id){
            memberNode -> memberList.erase(a_member);
            #ifdef DEBUGLOG
            Address delMemberAddr = makeAddress(id, port);
            log->logNodeRemove(&memberNode->addr, &delMemberAddr);
            #endif
            break;
        }
    }
}

/**
 * @brief A method to send a join request for new nodes
 * 
 * @param joinAddr - the join address
 */
void MP1Node::sendJoinRequest(Address * joinAddr){
    size_t msgsize = sizeof(MessageHdr) + sizeof(joinAddr->addr) + sizeof(long) + 1;
    MessageHdr * msg = (MessageHdr * ) malloc(msgsize * sizeof(char)); 
    // Create JOINREQ message: format of data is {struct Address myaddr}   
    msg->msgType = JOINREQ;
    memcpy((char*)(msg + 1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
    memcpy((char*)(msg + 1) + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));
    // Send JOINREQ message to introducer member
    emulNet->ENsend(&memberNode->addr, joinAddr, (char *)msg, msgsize);
    free(msg);
}

/**
 * @brief A method to send response for join requests
 * 
 * @param destinationAddr - target address which would be the join requester
 */
void MP1Node::sendJoinResponse(Address * destinationAddr){
    size_t memberListEntrySize = sizeof(int) + sizeof(short) + sizeof(long) + sizeof(long);
    size_t msgsize = sizeof(MessageHdr) + sizeof(int) + (memberNode -> memberList.size() * memberListEntrySize);
    MessageHdr * msg = (MessageHdr * ) malloc(msgsize * sizeof(char));
    msg->msgType = JOINREP;
    membershiplistSerializer(msg);
    emulNet->ENsend(&memberNode->addr, destinationAddr, (char*)msg, msgsize);
    free(msg);}
    
/**
 * @brief A method to send Heartbeats
 * 
 * @param destinationAddr - address to send heartbeats to
 */
void MP1Node::sendHeartbeats(Address *destinationAddr){
    size_t msgsize = sizeof(MessageHdr) + sizeof(destinationAddr->addr) + sizeof(long) + 1;
    MessageHdr *msg = (MessageHdr *) malloc(msgsize * sizeof(char));
    msg->msgType = HEARTBEAT;
    memcpy((char*)(msg + 1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
    memcpy((char*)(msg + 1) + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));
    emulNet->ENsend(&memberNode->addr, destinationAddr, (char *)msg, msgsize);
    free(msg);
}

/**
 * @brief A Method to serialize a membership list to transmit as message
 * 
 * @param msg - The msg to be serialized
 */
void MP1Node::membershiplistSerializer(MessageHdr * msg){
    int numberOfItems = memberNode->memberList.size();
    memcpy((char *)(msg + 1), &numberOfItems, sizeof(int));
    int displace = sizeof(int);
    for (std::vector < MemberListEntry > ::iterator a_member = memberNode -> memberList.begin(); a_member != memberNode -> memberList.end(); ++a_member){
        memcpy((char * )(msg + 1) + displace, &a_member->id, sizeof(int));
        displace += sizeof(int);
        memcpy((char * )(msg + 1) + displace, &a_member-> port, sizeof(short));
        displace += sizeof(short);
        memcpy((char * )(msg + 1) + displace, &a_member-> heartbeat, sizeof(long));
        displace += sizeof(long);
        memcpy((char * )(msg + 1) + displace, &a_member-> timestamp, sizeof(long));
        displace += sizeof(long);
    }

}

/**
 * @brief A method to deserialize a membership list after receiving it in msg
 * 
 * @param data - data recieved in msg
 */
void MP1Node::memberListDeserializer(char * data){
    int itemCount;
    memcpy(&itemCount, data + sizeof(MessageHdr), sizeof(int));
    int offset = sizeof(int);
    for(int i=0; i<itemCount; i++) {
        int id;
        short port;
        long heartbeat;
        long timestamp;
        memcpy(&id, data + sizeof(MessageHdr) + offset, sizeof(int));
        offset += sizeof(int);
        memcpy(&port, data + sizeof(MessageHdr) + offset, sizeof(short));
        offset += sizeof(short);
        memcpy(&heartbeat, data + sizeof(MessageHdr) + offset, sizeof(long));
        offset += sizeof(long);
        memcpy(&timestamp, data + sizeof(MessageHdr) + offset, sizeof(long));
        offset += sizeof(long);
        // Create and insert new entry
        addToMembershipList(id, port, heartbeat, timestamp);
    }
}


/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}