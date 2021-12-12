#ifndef __REDIS_CLUSTER_H
#define __REDIS_CLUSTER_H

/*-----------------------------------------------------------------------------
 * Redis cluster data structures, defines, exported API.
 *----------------------------------------------------------------------------*/

#define REDIS_CLUSTER_SLOTS 16384
#define REDIS_CLUSTER_OK 0          /* Everything looks ok */
#define REDIS_CLUSTER_FAIL 1        /* The cluster can't work */
#define REDIS_CLUSTER_NAMELEN 40    /* sha1 hex length */
#define REDIS_CLUSTER_PORT_INCR 10000 /* Cluster port = baseport + PORT_INCR */

/* The following defines are amount of time, sometimes expressed as
 * multiplicators of the node timeout value (when ending with MULT). */
#define REDIS_CLUSTER_DEFAULT_NODE_TIMEOUT 15000
#define REDIS_CLUSTER_DEFAULT_SLAVE_VALIDITY 10 /* Slave max data age factor. */
#define REDIS_CLUSTER_DEFAULT_REQUIRE_FULL_COVERAGE 1
#define REDIS_CLUSTER_FAIL_REPORT_VALIDITY_MULT 2 /* Fail report validity. */
#define REDIS_CLUSTER_FAIL_UNDO_TIME_MULT 2 /* Undo fail if master is back. */
#define REDIS_CLUSTER_FAIL_UNDO_TIME_ADD 10 /* Some additional time. */
#define REDIS_CLUSTER_FAILOVER_DELAY 5 /* Seconds */
#define REDIS_CLUSTER_DEFAULT_MIGRATION_BARRIER 1
#define REDIS_CLUSTER_MF_TIMEOUT 5000 /* Milliseconds to do a manual failover. */
#define REDIS_CLUSTER_MF_PAUSE_MULT 2 /* Master pause manual failover mult. */
#define REDIS_CLUSTER_SLAVE_MIGRATION_DELAY 5000 /* Delay for slave migration */

/* Redirection errors returned by getNodeByQuery(). */
#define REDIS_CLUSTER_REDIR_NONE 0          /* Node can serve the request. */
#define REDIS_CLUSTER_REDIR_CROSS_SLOT 1    /* -CROSSSLOT request. */
#define REDIS_CLUSTER_REDIR_UNSTABLE 2      /* -TRYAGAIN redirection required */
#define REDIS_CLUSTER_REDIR_ASK 3           /* -ASK redirection required. */
#define REDIS_CLUSTER_REDIR_MOVED 4         /* -MOVED redirection required. */
#define REDIS_CLUSTER_REDIR_DOWN_STATE 5    /* -CLUSTERDOWN, global state. */
#define REDIS_CLUSTER_REDIR_DOWN_UNBOUND 6  /* -CLUSTERDOWN, unbound slot. */

struct clusterNode;

/* 连接节点所需的有关信息 */
typedef struct clusterLink {
    mstime_t ctime;             /* 连接创建的时间 */
    int fd;                     /* 套接字描述符 */
    sds sndbuf;                 /* 输出缓冲区 */
    sds rcvbuf;                 /* 输入缓冲区 */
    struct clusterNode *node;   /* 与这个连接相关联的节点，如果没有就为NULL */
} clusterLink;

/* Cluster node flags and macros. */
#define REDIS_NODE_MASTER 1     /* The node is a master */
#define REDIS_NODE_SLAVE 2      /* The node is a slave */
#define REDIS_NODE_PFAIL 4      /* Failure? Need acknowledge */
#define REDIS_NODE_FAIL 8       /* The node is believed to be malfunctioning */
#define REDIS_NODE_MYSELF 16    /* This node is myself */
#define REDIS_NODE_HANDSHAKE 32 /* We have still to exchange the first ping */
#define REDIS_NODE_NOADDR   64  /* We don't know the address of this node */
#define REDIS_NODE_MEET 128     /* Send a MEET message to this node */
#define REDIS_NODE_MIGRATE_TO 256 /* Master elegible for replica migration. */
#define REDIS_NODE_NULL_NAME "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"

#define nodeIsMaster(n) ((n)->flags & REDIS_NODE_MASTER)
#define nodeIsSlave(n) ((n)->flags & REDIS_NODE_SLAVE)
#define nodeInHandshake(n) ((n)->flags & REDIS_NODE_HANDSHAKE)
#define nodeHasAddr(n) (!((n)->flags & REDIS_NODE_NOADDR))
#define nodeWithoutAddr(n) ((n)->flags & REDIS_NODE_NOADDR)
#define nodeTimedOut(n) ((n)->flags & REDIS_NODE_PFAIL)
#define nodeFailed(n) ((n)->flags & REDIS_NODE_FAIL)

/* Reasons why a slave is not able to failover. */
#define REDIS_CLUSTER_CANT_FAILOVER_NONE 0
#define REDIS_CLUSTER_CANT_FAILOVER_DATA_AGE 1
#define REDIS_CLUSTER_CANT_FAILOVER_WAITING_DELAY 2
#define REDIS_CLUSTER_CANT_FAILOVER_EXPIRED 3
#define REDIS_CLUSTER_CANT_FAILOVER_WAITING_VOTES 4
#define REDIS_CLUSTER_CANT_FAILOVER_RELOG_PERIOD (60*5) /* seconds. */

/* 下线报告 */
typedef struct clusterNodeFailReport {
    struct clusterNode *node;  /* 报告目标节点已经下线的节点 */
    mstime_t time;             /* 最后一次从node节点收到下线报告的时间（程序使用这个时间戳来检查下线报告是否过期，与当前时间相差太多的下线报告会被删除） */
} clusterNodeFailReport;
/* 集群节点 */
typedef struct clusterNode {
    mstime_t ctime;                     /* 节点创建时间 */
    char name[REDIS_CLUSTER_NAMELEN];   /* 节点名字（由40个十六进制字符组成） */
    int flags;                          /* 节点标识（标记节点的角色和状态） */
    uint64_t configEpoch;               /* 当前的配置纪元（用于故障转移） */
    unsigned char slots[REDIS_CLUSTER_SLOTS/8]; /* 二进制槽数组；1:处理槽，2:不处理槽 */
    int numslots;   /* Number of slots handled by this node */
    int numslaves;  /* 正在复制这个主节点的从节点数量 */
    struct clusterNode **slaves; /* 正在复制这个主节点的子节点链表 */
    struct clusterNode *slaveof; /* 指向主节点 */



    mstime_t ping_sent;      /* Unix time we sent latest ping */
    mstime_t pong_received;  /* Unix time we received the pong */
    mstime_t fail_time;      /* Unix time when FAIL flag was set */
    mstime_t voted_time;     /* Last time we voted for a slave of this master */
    mstime_t repl_offset_time;  /* Unix time we received offset for this node */
    mstime_t orphaned_time;     /* Starting time of orphaned master condition */
    long long repl_offset;      /* Last known repl offset for this node. */
    char ip[REDIS_IP_STR_LEN];  /* 节点IP */
    int port;                   /* 节点端口 */
    clusterLink *link;          /* 保存连接节点所需的有关信息 */
    list *fail_reports;         /* 记录所有其它节点对该节点的下线报告链表 */
} clusterNode;
/* 当前节点所在的集群状态 */
typedef struct clusterState {
    clusterNode *myself;    /* 本节点 */
    uint64_t currentEpoch;  /* 集群当钱的配置纪元（用于故障转移） */
    int state;              /* 集群当前的状态（REDIS_CLUSTER_OK, REDIS_CLUSTER_FAIL, ... ）*/
    int size;               /* 集群中至少处理着一个槽的节点的数量 */
    dict *nodes;            /* 节点字典：key:节点名字，value:节点 */
    dict *nodes_black_list; /* Nodes we don't re-add for a few seconds. */
    clusterNode *migrating_slots_to[REDIS_CLUSTER_SLOTS]; /* 记录当前节点正在迁移至其它节点的槽 */
    clusterNode *importing_slots_from[REDIS_CLUSTER_SLOTS]; /* 记录当前节点正在从其它节点导入的槽 */
    clusterNode *slots[REDIS_CLUSTER_SLOTS]; /* 槽指派信息数组 */
    zskiplist *slots_to_keys; /* 跳表，用来保存槽和键之间的关系（分值:槽，节点:键） */
    /* The following fields are used to take the slave state on elections. */
    mstime_t failover_auth_time; /* Time of previous or next election. */
    int failover_auth_count;    /* Number of votes received so far. */
    int failover_auth_sent;     /* True if we already asked for votes. */
    int failover_auth_rank;     /* This slave rank for current auth request. */
    uint64_t failover_auth_epoch; /* Epoch of the current election. */
    int cant_failover_reason;   /* Why a slave is currently not able to
                                   failover. See the CANT_FAILOVER_* macros. */
    /* Manual failover state in common. */
    mstime_t mf_end;            /* Manual failover time limit (ms unixtime).
                                   It is zero if there is no MF in progress. */
    /* Manual failover state of master. */
    clusterNode *mf_slave;      /* Slave performing the manual failover. */
    /* Manual failover state of slave. */
    long long mf_master_offset; /* Master offset the slave needs to start MF
                                   or zero if stil not received. */
    int mf_can_start;           /* If non-zero signal that the manual failover
                                   can start requesting masters vote. */
    /* The followign fields are used by masters to take state on elections. */
    uint64_t lastVoteEpoch;     /* Epoch of the last vote granted. */
    int todo_before_sleep; /* Things to do in clusterBeforeSleep(). */
    long long stats_bus_messages_sent;  /* Num of msg sent via cluster bus. */
    long long stats_bus_messages_received; /* Num of msg rcvd via cluster bus.*/
} clusterState;

/* clusterState todo_before_sleep flags. */
#define CLUSTER_TODO_HANDLE_FAILOVER (1<<0)
#define CLUSTER_TODO_UPDATE_STATE (1<<1)
#define CLUSTER_TODO_SAVE_CONFIG (1<<2)
#define CLUSTER_TODO_FSYNC_CONFIG (1<<3)

/* Redis cluster messages header */

/* Note that the PING, PONG and MEET messages are actually the same exact
 * kind of packet. PONG is the reply to ping, in the exact format as a PING,
 * while MEET is a special PING that forces the receiver to add the sender
 * as a node (if it is not already in the list). */
#define CLUSTERMSG_TYPE_PING 0          /* Ping */
#define CLUSTERMSG_TYPE_PONG 1          /* Pong (reply to Ping) */
#define CLUSTERMSG_TYPE_MEET 2          /* Meet "let's join" message */
#define CLUSTERMSG_TYPE_FAIL 3          /* Mark node xxx as failing */
#define CLUSTERMSG_TYPE_PUBLISH 4       /* Pub/Sub Publish propagation */
#define CLUSTERMSG_TYPE_FAILOVER_AUTH_REQUEST 5 /* May I failover? */
#define CLUSTERMSG_TYPE_FAILOVER_AUTH_ACK 6     /* Yes, you have my vote */
#define CLUSTERMSG_TYPE_UPDATE 7        /* Another node slots configuration */
#define CLUSTERMSG_TYPE_MFSTART 8       /* Pause clients for manual failover */

/* Initially we don't know our "name", but we'll find it once we connect
 * to the first node, using the getsockname() function. Then we'll use this
 * address for all the next messages. */
typedef struct {
    char nodename[REDIS_CLUSTER_NAMELEN]; /* 节点的名字 */
    uint32_t ping_sent;                   /* 最后一次向该节点发送PING消息的时间戳 */
    uint32_t pong_received;               /* 最后一次从该节点接收到PONG消息的时间戳 */
    char ip[REDIS_IP_STR_LEN];            /* 节点的IP地址 */
    uint16_t port;                        /* 节点的端口号 */
    uint16_t flags;                       /* 节点的标识值 */
    uint16_t notused1;                    /*  */
    uint32_t notused2;
} clusterMsgDataGossip;
/* FAIL消息 */
typedef struct {
    char nodename[REDIS_CLUSTER_NAMELEN]; /* 已下线节点的名字 */
} clusterMsgDataFail;
/* PUBLISH消息 */
typedef struct {
    uint32_t channel_len; /* channel参数的长度 */
    uint32_t message_len; /* message参数的长度 */
    /* （定义为8字节，只是为了对其其它消息结构，实际的长度由保存的内容决定） */
    /* bulk_data[0, channel_len - 1]：channel参数 */
    /* bulk_data[channel_len, channel_len+message_len - 1]：message参数 */
    unsigned char bulk_data[8]; /* 客户端通过PUBLISH发送给节点的channel参数和message参数 */
} clusterMsgDataPublish;

typedef struct {
    uint64_t configEpoch; /* Config epoch of the specified instance. */
    char nodename[REDIS_CLUSTER_NAMELEN]; /* Name of the slots owner. */
    unsigned char slots[REDIS_CLUSTER_SLOTS/8]; /* Slots bitmap. */
} clusterMsgDataUpdate;
/* 消息类型集合 */
union clusterMsgData {
    /* PING, MEET and PONG */
    struct {
        /* Array of N clusterMsgDataGossip structures */
        clusterMsgDataGossip gossip[1];
    } ping;

    /* FAIL */
    struct {
        clusterMsgDataFail about;
    } fail;

    /* PUBLISH */
    struct {
        clusterMsgDataPublish msg;
    } publish;

    /* UPDATE */
    struct {
        clusterMsgDataUpdate nodecfg;
    } update;
};

#define CLUSTER_PROTO_VER 0 /* Cluster bus protocol version. */
/* 消息头 */
typedef struct {
    char sig[4];        /* Siganture "RCmb" (Redis Cluster message bus). */
    uint32_t totlen;    /* 消息的长度（包括这个消息头的长度和消息正文的长度） */ 
    uint16_t ver;       /* Protocol version, currently set to 0. */
    uint16_t notused0;  /* 2 bytes not used. */
    uint16_t type;      /* 消息的类型 */
    uint16_t count;     /* 消息正文包含的节点信息数量（只在发送MEET，PING，PONG这三种Gossip协议消息时使用） */
    uint64_t currentEpoch;  /* 发送者所处的配置纪元 */
    uint64_t configEpoch;   /* 配置纪元 */
                            /* 如果发送者是一个主节点，那么这里记录的是发送者的配置纪元 */
                            /* 如果发送者是一个从节点，那么这里记录的是发送者正在复制的主节点的配置纪元 */
    uint64_t offset;    /* Master replication offset if node is a master or
                           processed replication offset if node is a slave. */
    char sender[REDIS_CLUSTER_NAMELEN];           /* 发送者的名字 */
    unsigned char myslots[REDIS_CLUSTER_SLOTS/8]; /* 发送者目前的槽指派信息 */
    char slaveof[REDIS_CLUSTER_NAMELEN]; /* 从节点：这里记录的是发送者正在复制的主节点的名字；主节点：记录的事REDIS_NODE_NULL_NAME； */
    char notused1[32];  /* 32 bytes reserved for future usage. */
    uint16_t port;      /* 发送者的TCP端口号 */
    uint16_t flags;     /* 发送者的标识值 */
    unsigned char state;       /* 发送者所处集群的状态 */
    unsigned char mflags[3];   /* Message flags: CLUSTERMSG_FLAG[012]_... */
    union clusterMsgData data; /* 消息内容 */
} clusterMsg;

#define CLUSTERMSG_MIN_LEN (sizeof(clusterMsg)-sizeof(union clusterMsgData))

/* Message flags better specify the packet content or are used to
 * provide some information about the node state. */
#define CLUSTERMSG_FLAG0_PAUSED (1<<0) /* Master paused for manual failover. */
#define CLUSTERMSG_FLAG0_FORCEACK (1<<1) /* Give ACK to AUTH_REQUEST even if
                                            master is up. */

/* ---------------------- API exported outside cluster.c -------------------- */
clusterNode *getNodeByQuery(redisClient *c, struct redisCommand *cmd, robj **argv, int argc, int *hashslot, int *ask);
int clusterRedirectBlockedClientIfNeeded(redisClient *c);
void clusterRedirectClient(redisClient *c, clusterNode *n, int hashslot, int error_code);

#endif /* __REDIS_CLUSTER_H */
