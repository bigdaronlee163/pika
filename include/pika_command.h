// Copyright (c) 2015-present, Qihoo, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#ifndef PIKA_COMMAND_H_
#define PIKA_COMMAND_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "rocksdb/status.h"

#include "net/include/net_conn.h"
#include "net/include/redis_conn.h"
#include "pstd/include/pstd_string.h"

#include "net/src/dispatch_thread.h"

class SyncMasterDB;
class SyncSlaveDB;
class DB;
// Constant for command name
// Admin
const std::string kCmdNameSlaveof = "slaveof";
const std::string kCmdNameDbSlaveof = "dbslaveof";
const std::string kCmdNameAuth = "auth";
const std::string kCmdNameBgsave = "bgsave";
const std::string kCmdNameCompact = "compact";
const std::string kCmdNameCompactRange = "compactrange";
const std::string kCmdNamePurgelogsto = "purgelogsto";
const std::string kCmdNamePing = "ping";
const std::string kCmdNameSelect = "select";
const std::string kCmdNameFlushall = "flushall";
const std::string kCmdNameFlushdb = "flushdb";
const std::string kCmdNameClient = "client";
const std::string kCmdNameShutdown = "shutdown";
const std::string kCmdNameInfo = "info";
const std::string kCmdNameConfig = "config";
const std::string kCmdNameMonitor = "monitor";
const std::string kCmdNameDbsize = "dbsize";
const std::string kCmdNameTime = "time";
const std::string kCmdNameDelbackup = "delbackup";
const std::string kCmdNameEcho = "echo";
const std::string kCmdNameScandb = "scandb";
const std::string kCmdNameSlowlog = "slowlog";
const std::string kCmdNamePadding = "padding";
const std::string kCmdNamePKPatternMatchDel = "pkpatternmatchdel";
const std::string kCmdDummy = "dummy";
const std::string kCmdNameQuit = "quit";
const std::string kCmdNameHello = "hello";
const std::string kCmdNameCommand = "command";
const std::string kCmdNameDiskRecovery = "diskrecovery";
const std::string kCmdNameClearReplicationID = "clearreplicationid";
const std::string kCmdNameDisableWal = "disablewal";
const std::string kCmdNameLastSave = "lastsave";
const std::string kCmdNameCache = "cache";
const std::string kCmdNameClearCache = "clearcache";

// Migrate slot
const std::string kCmdNameSlotsMgrtSlot = "slotsmgrtslot";
const std::string kCmdNameSlotsMgrtTagSlot = "slotsmgrttagslot";
const std::string kCmdNameSlotsMgrtOne = "slotsmgrtone";
const std::string kCmdNameSlotsMgrtTagOne = "slotsmgrttagone";
const std::string kCmdNameSlotsInfo = "slotsinfo";
const std::string kCmdNameSlotsHashKey = "slotshashkey";
const std::string kCmdNameSlotsReload = "slotsreload";
const std::string kCmdNameSlotsReloadOff = "slotsreloadoff";
const std::string kCmdNameSlotsDel = "slotsdel";
const std::string kCmdNameSlotsScan = "slotsscan";
const std::string kCmdNameSlotsCleanup = "slotscleanup";
const std::string kCmdNameSlotsCleanupOff = "slotscleanupoff";
const std::string kCmdNameSlotsMgrtTagSlotAsync = "slotsmgrttagslot-async";
const std::string kCmdNameSlotsMgrtSlotAsync = "slotsmgrtslot-async";
const std::string kCmdNameSlotsMgrtExecWrapper = "slotsmgrt-exec-wrapper";
const std::string kCmdNameSlotsMgrtAsyncStatus = "slotsmgrt-async-status";
const std::string kCmdNameSlotsMgrtAsyncCancel = "slotsmgrt-async-cancel";

// Kv
const std::string kCmdNameSet = "set";
const std::string kCmdNameGet = "get";
const std::string kCmdNameDel = "del";
const std::string kCmdNameUnlink = "unlink";
const std::string kCmdNameIncr = "incr";
const std::string kCmdNameIncrby = "incrby";
const std::string kCmdNameIncrbyfloat = "incrbyfloat";
const std::string kCmdNameDecr = "decr";
const std::string kCmdNameDecrby = "decrby";
const std::string kCmdNameGetset = "getset";
const std::string kCmdNameAppend = "append";
const std::string kCmdNameMget = "mget";
const std::string kCmdNameKeys = "keys";
const std::string kCmdNameSetnx = "setnx";
const std::string kCmdNameSetex = "setex";
const std::string kCmdNamePsetex = "psetex";
const std::string kCmdNameDelvx = "delvx";
const std::string kCmdNameMset = "mset";
const std::string kCmdNameMsetnx = "msetnx";
const std::string kCmdNameGetrange = "getrange";
const std::string kCmdNameSetrange = "setrange";
const std::string kCmdNameStrlen = "strlen";
const std::string kCmdNameExists = "exists";
const std::string kCmdNameExpire = "expire";
const std::string kCmdNamePexpire = "pexpire";
const std::string kCmdNameExpireat = "expireat";
const std::string kCmdNamePexpireat = "pexpireat";
const std::string kCmdNameTtl = "ttl";
const std::string kCmdNamePttl = "pttl";
const std::string kCmdNamePersist = "persist";
const std::string kCmdNameType = "type";
const std::string kCmdNameScan = "scan";
const std::string kCmdNameScanx = "scanx";
const std::string kCmdNamePKSetexAt = "pksetexat";
const std::string kCmdNamePKScanRange = "pkscanrange";
const std::string kCmdNamePKRScanRange = "pkrscanrange";
const std::string kCmdNameDump = "dump";
const std::string kCmdNameRestore = "restore";

// Hash
const std::string kCmdNameHDel = "hdel";
const std::string kCmdNameHSet = "hset";
const std::string kCmdNameHGet = "hget";
const std::string kCmdNameHGetall = "hgetall";
const std::string kCmdNameHExists = "hexists";
const std::string kCmdNameHIncrby = "hincrby";
const std::string kCmdNameHIncrbyfloat = "hincrbyfloat";
const std::string kCmdNameHKeys = "hkeys";
const std::string kCmdNameHLen = "hlen";
const std::string kCmdNameHMget = "hmget";
const std::string kCmdNameHMset = "hmset";
const std::string kCmdNameHSetnx = "hsetnx";
const std::string kCmdNameHStrlen = "hstrlen";
const std::string kCmdNameHVals = "hvals";
const std::string kCmdNameHScan = "hscan";
const std::string kCmdNameHScanx = "hscanx";
const std::string kCmdNamePKHScanRange = "pkhscanrange";
const std::string kCmdNamePKHRScanRange = "pkhrscanrange";
const std::string kCmdNameHExpire = "hexpire";
const std::string kCmdNameHExpireat = "hexpireat";
const std::string kCmdNameHExpireTime = "hexpiretime";
const std::string kCmdNameHPExpire = "hpexpire";
const std::string kCmdNameHPExpireat = "hpexpireat";
const std::string kCmdNameHPExpireTime = "hpexpiretime";
const std::string kCmdNameHPersist = "hpersist";
const std::string kCmdNameHTTL = "httl";
const std::string kCmdNameHPTTL = "hpttl";

// List
const std::string kCmdNameLIndex = "lindex";
const std::string kCmdNameLInsert = "linsert";
const std::string kCmdNameLLen = "llen";
const std::string kCmdNameBLPop = "blpop";
const std::string kCmdNameLPop = "lpop";
const std::string kCmdNameLPush = "lpush";
const std::string kCmdNameLPushx = "lpushx";
const std::string kCmdNameLRange = "lrange";
const std::string kCmdNameLRem = "lrem";
const std::string kCmdNameLSet = "lset";
const std::string kCmdNameLTrim = "ltrim";
const std::string kCmdNameBRpop = "brpop";
const std::string kCmdNameRPop = "rpop";
const std::string kCmdNameRPopLPush = "rpoplpush";
const std::string kCmdNameRPush = "rpush";
const std::string kCmdNameRPushx = "rpushx";

// BitMap
const std::string kCmdNameBitSet = "setbit";
const std::string kCmdNameBitGet = "getbit";
const std::string kCmdNameBitPos = "bitpos";
const std::string kCmdNameBitOp = "bitop";
const std::string kCmdNameBitCount = "bitcount";

// Zset
const std::string kCmdNameZAdd = "zadd";
const std::string kCmdNameZCard = "zcard";
const std::string kCmdNameZScan = "zscan";
const std::string kCmdNameZIncrby = "zincrby";
const std::string kCmdNameZRange = "zrange";
const std::string kCmdNameZRangebyscore = "zrangebyscore";
const std::string kCmdNameZCount = "zcount";
const std::string kCmdNameZRem = "zrem";
const std::string kCmdNameZUnionstore = "zunionstore";
const std::string kCmdNameZInterstore = "zinterstore";
const std::string kCmdNameZRank = "zrank";
const std::string kCmdNameZRevrank = "zrevrank";
const std::string kCmdNameZScore = "zscore";
const std::string kCmdNameZRevrange = "zrevrange";
const std::string kCmdNameZRevrangebyscore = "zrevrangebyscore";
const std::string kCmdNameZRangebylex = "zrangebylex";
const std::string kCmdNameZRevrangebylex = "zrevrangebylex";
const std::string kCmdNameZLexcount = "zlexcount";
const std::string kCmdNameZRemrangebyrank = "zremrangebyrank";
const std::string kCmdNameZRemrangebylex = "zremrangebylex";
const std::string kCmdNameZRemrangebyscore = "zremrangebyscore";
const std::string kCmdNameZPopmax = "zpopmax";
const std::string kCmdNameZPopmin = "zpopmin";

// Set
const std::string kCmdNameSAdd = "sadd";
const std::string kCmdNameSPop = "spop";
const std::string kCmdNameSCard = "scard";
const std::string kCmdNameSMembers = "smembers";
const std::string kCmdNameSScan = "sscan";
const std::string kCmdNameSRem = "srem";
const std::string kCmdNameSUnion = "sunion";
const std::string kCmdNameSUnionstore = "sunionstore";
const std::string kCmdNameSInter = "sinter";
const std::string kCmdNameSInterstore = "sinterstore";
const std::string kCmdNameSIsmember = "sismember";
const std::string kCmdNameSDiff = "sdiff";
const std::string kCmdNameSDiffstore = "sdiffstore";
const std::string kCmdNameSMove = "smove";
const std::string kCmdNameSRandmember = "srandmember";

// transation
const std::string kCmdNameMulti = "multi";
const std::string kCmdNameExec = "exec";
const std::string kCmdNameDiscard = "discard";
const std::string kCmdNameWatch = "watch";
const std::string kCmdNameUnWatch = "unwatch";

// HyperLogLog
const std::string kCmdNamePfAdd = "pfadd";
const std::string kCmdNamePfCount = "pfcount";
const std::string kCmdNamePfMerge = "pfmerge";

// GEO
const std::string kCmdNameGeoAdd = "geoadd";
const std::string kCmdNameGeoPos = "geopos";
const std::string kCmdNameGeoDist = "geodist";
const std::string kCmdNameGeoHash = "geohash";
const std::string kCmdNameGeoRadius = "georadius";
const std::string kCmdNameGeoRadiusByMember = "georadiusbymember";

// Pub/Sub
const std::string kCmdNamePublish = "publish";
const std::string kCmdNameSubscribe = "subscribe";
const std::string kCmdNameUnSubscribe = "unsubscribe";
const std::string kCmdNamePubSub = "pubsub";
const std::string kCmdNamePSubscribe = "psubscribe";
const std::string kCmdNamePUnSubscribe = "punsubscribe";

// ACL
const std::string KCmdNameAcl = "acl";

// Stream
const std::string kCmdNameXAdd = "xadd";
const std::string kCmdNameXDel = "xdel";
const std::string kCmdNameXRead = "xread";
const std::string kCmdNameXLen = "xlen";
const std::string kCmdNameXRange = "xrange";
const std::string kCmdNameXRevrange = "xrevrange";
const std::string kCmdNameXTrim = "xtrim";
const std::string kCmdNameXInfo = "xinfo";

const std::string kClusterPrefix = "pkcluster";


/*
 * If a type holds a key, a new data structure
 * that uses the key will use this error
 */
constexpr const char* ErrTypeMessage = "Invalid argument: WRONGTYPE";

using PikaCmdArgsType = net::RedisCmdArgsType;
static const int RAW_ARGS_LEN = 1024 * 1024;
// TODO(DDD): 
/*

读写（RW）：表示该命令可以读取和写入数据。
本地（Local）：表示该命令仅在本地数据库上执行，而不是在分布式数据库上执行。
暂停（Suspend）：表示该命令在执行过程中可以暂停，以便其他命令可以执行。
读取缓存（ReadCache）：表示该命令从缓存中读取数据，而不是从数据库中读取。
管理员需求（AdminRequire）：表示该命令需要管理员权限才能执行。
更新缓存（UpdateCache）：表示该命令在执行完成后更新缓存，以便其他命令可以使用。
Through DB：表示该命令通过数据库执行，而不是在本地执行。

*/
enum CmdFlagsMask {
  kCmdFlagsMaskRW = 1,
  kCmdFlagsMaskLocal = (1 << 1),
  kCmdFlagsMaskSuspend = (1 << 2),
  kCmdFlagsMaskReadCache = (1 << 3),
  kCmdFlagsMaskAdminRequire = (1 << 4),
  kCmdFlagsMaskUpdateCache = (1 << 5),
  kCmdFlagsMaskDoThrouhDB = (1 << 6),
};

enum CmdFlags {
  kCmdFlagsRead = 1,  // default rw
  kCmdFlagsWrite = (1 << 1),
  kCmdFlagsAdmin = (1 << 2),  // default type
  kCmdFlagsKv = (1 << 3),
  kCmdFlagsHash = (1 << 4),
  kCmdFlagsList = (1 << 5),
  kCmdFlagsSet = (1 << 6),
  kCmdFlagsZset = (1 << 7),
  kCmdFlagsBit = (1 << 8),
  kCmdFlagsHyperLogLog = (1 << 9),
  kCmdFlagsGeo = (1 << 10),
  kCmdFlagsPubSub = (1 << 11),
  kCmdFlagsLocal = (1 << 12),
  kCmdFlagsSuspend = (1 << 13),
  kCmdFlagsAdminRequire = (1 << 14),
  kCmdFlagsNoAuth = (1 << 15),  // command no auth can also be executed
  kCmdFlagsReadCache = (1 << 16),
  kCmdFlagsUpdateCache = (1 << 17),
  kCmdFlagsDoThroughDB = (1 << 18),
  kCmdFlagsOperateKey = (1 << 19),  // redis keySpace
  kCmdFlagsStream = (1 << 20),
  kCmdFlagsFast = (1 << 21),
  kCmdFlagsSlow = (1 << 22)
};

void inline RedisAppendContent(std::string& str, const std::string& value);
void inline RedisAppendLen(std::string& str, int64_t ori, const std::string& prefix);
void inline RedisAppendLenUint64(std::string& str, uint64_t ori, const std::string& prefix) {
  RedisAppendLen(str, static_cast<int64_t>(ori), prefix);
}

const std::string kNewLine = "\r\n";

class CmdRes {
 public:
  // 记录redis协议常用的字段。
  enum CmdRet {
    kNone = 0,
    kOk,
    kPong,
    kSyntaxErr,
    kInvalidInt,
    kInvalidBitInt,
    kInvalidBitOffsetInt,
    kInvalidBitPosArgument,
    kWrongBitOpNotNum,
    kInvalidFloat,
    kOverFlow,
    kNotFound,
    kOutOfRange,
    kInvalidPwd,
    kNoneBgsave,
    kPurgeExist,
    kInvalidParameter,
    kWrongNum,
    kInvalidIndex,
    kInvalidDbType,
    kInvalidDB,
    kInconsistentHashTag,
    kErrOther,
    kCacheMiss,
    KIncrByOverFlow,
    kInvalidTransaction,
    kTxnQueued,
    kTxnAbort,
    kMultiKey
  };
  // 构造函数。
  CmdRes() = default;
  // 判断是否为不同的情况。
  bool none() const { return ret_ == kNone && message_.empty(); }
  bool ok() const { return ret_ == kOk || ret_ == kNone; }
  CmdRet ret() const { return ret_; }
  void clear() {
    message_.clear();
    ret_ = kNone;
  }
  bool CacheMiss() const { return ret_ == kCacheMiss; }
  std::string raw_message() const { return message_; }
  // 返回具体的消息内容。 格式： 类型+具体的信息。
  std::string message() const {
    std::string result;
    switch (ret_) {
      case kNone:
        return message_;
      case kOk:
        return "+OK\r\n";
      case kPong:
        return "+PONG\r\n";
      case kSyntaxErr:
        return "-ERR syntax error\r\n";
      case kInvalidInt:
        return "-ERR value is not an integer or out of range\r\n";
      case kInvalidBitInt:
        return "-ERR bit is not an integer or out of range\r\n";
      case kInvalidBitOffsetInt:
        return "-ERR bit offset is not an integer or out of range\r\n";
      case kWrongBitOpNotNum:
        return "-ERR BITOP NOT must be called with a single source key.\r\n";

      case kInvalidBitPosArgument:
        return "-ERR The bit argument must be 1 or 0.\r\n";
      case kInvalidFloat:
        return "-ERR value is not a valid float\r\n";
      case kOverFlow:
        return "-ERR increment or decrement would overflow\r\n";
      case kNotFound:
        return "-ERR no such key\r\n";
      case kOutOfRange:
        return "-ERR index out of range\r\n";
      case kInvalidPwd:
        return "-ERR invalid password\r\n";
      case kNoneBgsave:
        return "-ERR No BGSave Works now\r\n";
      case kPurgeExist:
        return "-ERR binlog already in purging...\r\n";
      case kInvalidParameter:
        return "-ERR Invalid Argument\r\n";
      case kWrongNum:
        result = "-ERR wrong number of arguments for '";
        result.append(message_);
        result.append("' command\r\n");
        break;
      case kInvalidIndex:
        result = "-ERR invalid DB index for '";
        result.append(message_);
        result.append("'\r\n");
        break;
      case kInvalidDbType:
        result = "-ERR invalid DB for '";
        result.append(message_);
        result.append("'\r\n");
        break;
      case kInconsistentHashTag:
        return "-ERR parameters hashtag is inconsistent\r\n";
      case kInvalidDB:
        result = "-ERR invalid DB for '";
        result.append(message_);
        result.append("'\r\n");
        break;
      case kInvalidTransaction:
        return "-ERR WATCH inside MULTI is not allowed\r\n";
      case kTxnQueued:
        result = "+QUEUED";
        result.append("\r\n");
        break;
      case kTxnAbort:
        result = "-EXECABORT ";
        result.append(message_);
        result.append(kNewLine);
        break;
      case kErrOther:
        result = "-ERR ";
        result.append(message_);
        result.append(kNewLine);
        break;
      case KIncrByOverFlow:
        result = "-ERR increment would produce NaN or Infinity";
        result.append(message_);
        result.append(kNewLine);
        break;
      case kMultiKey:
        result = "-WRONGTYPE Operation against a key holding the wrong kind of value";
        result.append(kNewLine);
        break;
      default:
        break;
    }
    return result;
  }

  // Inline functions for Create Redis protocol
  // 头文件中的函数，自动inline吗？ 
  // 总结来说，虽然你没有显式地使用  inline  关键字，
  // 但在头文件中定义的这些小型函数通常会被编译器视为内联函数，
  // 从而可能会被自动内联。为了确保这一点，
  // 可以在函数定义前加上  inline  关键字，尽管这不是必需的。
  void AppendStringLen(int64_t ori) { RedisAppendLen(message_, ori, "$"); }
  void AppendStringLenUint64(uint64_t ori) { RedisAppendLenUint64(message_, ori, "$"); }
  void AppendArrayLen(int64_t ori) { RedisAppendLen(message_, ori, "*"); }
  void AppendArrayLenUint64(uint64_t ori) { RedisAppendLenUint64(message_, ori, "*"); }
  void AppendInteger(int64_t ori) { RedisAppendLen(message_, ori, ":"); }
  void AppendContent(const std::string& value) { RedisAppendContent(message_, value); }
  void AppendString(const std::string& value) {
    AppendStringLenUint64(value.size());
    AppendContent(value);
  }
  void AppendStringRaw(const std::string& value) { message_.append(value); }

  void AppendStringVector(const std::vector<std::string>& strArray) {
    if (strArray.empty()) {
      AppendArrayLen(0);
      return;
    }
    AppendArrayLen(strArray.size());
    for (const auto& item : strArray) {
      AppendString(item);
    }
  }
  // 设置响应结果。
  void SetRes(CmdRet _ret, const std::string& content = "") {
    ret_ = _ret;
    if (!content.empty()) {
      message_ = content;
    }
  }

 private:
  // 记录具体响应的内容。
  std::string message_;
  // 记录响应的类型。
  CmdRet ret_ = kNone;
};

/**
 * Current used by:
 * blpop,brpop
 */
struct UnblockTaskArgs {
  std::string key;
  std::shared_ptr<DB> db;
  net::DispatchThread* dispatchThread{ nullptr };
  UnblockTaskArgs(std::string key_, std::shared_ptr<DB> db_, net::DispatchThread* dispatchThread_)
      : key(std::move(key_)), db(db_), dispatchThread(dispatchThread_) {}
};

class PikaClientConn;
/*

在PikaClientConn的通用处理流程中，
对于不同Cmd的操作都是调用其基类处理函数Initial和Execute，
Initial和Execute函数内部会调用纯虚函数DoInitial和Do，通过多态查找派生类的真正实现。

任何具体的命令继承Cmd之后，需要实现DoInitial和Do 两个纯虚函数。在之后的通用处理流程中Cmd会做相应的调用。Cmd对外主要暴露Initial 和Execute 两个接口。

1，Initial清除前一次调用的残留数据，同时调用DoInitial虚函数。

2，Execute判断pika运行模式，主要调用InternalProcessCommand。

  2.1，对于操作DB 和Binlog 这两个动作加锁，确保DB 和Binlog 是一致的。

  2.2，调用DoCommand，其内部主要调用Do 虚函数。

  2.3，调用DoBinlog，将命令处理后写入Binlog。 
*/
class Cmd : public std::enable_shared_from_this<Cmd> {
 public:
  friend class PikaClientConn;
  // TODO(DDD): 
  /*
  这些成员分别表示命令的三个阶段：未开始、二进制日志阶段和执行阶段。
  */
  enum CmdStage { kNone, kBinlogStage, kExecuteStage };
  struct HintKeys {
    HintKeys() = default;

    bool empty() const { return keys.empty() && hints.empty(); }
    std::vector<std::string> keys;
    std::vector<int> hints;
  };
  struct ProcessArg {
    ProcessArg() = default;
    ProcessArg(std::shared_ptr<DB> _db, std::shared_ptr<SyncMasterDB> _sync_db, HintKeys _hint_keys)
        : db(std::move(_db)), sync_db(std::move(_sync_db)), hint_keys(std::move(_hint_keys)) {}
    std::shared_ptr<DB> db;
    std::shared_ptr<SyncMasterDB> sync_db;
    HintKeys hint_keys;
  };
  struct CommandStatistics {
    CommandStatistics() = default;
    CommandStatistics(const CommandStatistics& other) {
      cmd_time_consuming.store(other.cmd_time_consuming.load());
      cmd_count.store(other.cmd_count.load());
    }
    std::atomic<int32_t> cmd_count = {0};
    std::atomic<int32_t> cmd_time_consuming = {0};
  };
  CommandStatistics state;
  Cmd(std::string name, int arity, uint32_t flag, uint32_t aclCategory = 0);
  virtual ~Cmd() = default;

  virtual std::vector<std::string> current_key() const;
  virtual void Execute();
  virtual void Do() {};
  virtual void DoThroughDB() {}
  virtual void DoUpdateCache() {}
  virtual void ReadCache() {}
  virtual Cmd* Clone() = 0;
  // used for execute multikey command into different slots
  virtual void Split(const HintKeys& hint_keys) = 0;
  virtual void Merge() = 0;

  int8_t SubCmdIndex(const std::string& cmdName);  // if the command no subCommand，return -1；

  void Initial(const PikaCmdArgsType& argv, const std::string& db_name);
  uint32_t flag() const;
  bool hasFlag(uint32_t flag) const;
  bool is_read() const;
  bool is_write() const;
  bool isCacheRead() const;

  bool IsLocal() const;
  bool IsSuspend() const;
  bool IsAdminRequire() const;
  bool HasSubCommand() const;                   // The command is there a sub command
  std::vector<std::string> SubCommand() const;  // Get command is there a sub command
  bool IsNeedUpdateCache() const;
  bool IsNeedReadCache() const;
  bool IsNeedCacheDo() const;
  bool HashtagIsConsistent(const std::string& lhs, const std::string& rhs) const;
  uint64_t GetDoDuration() const { return do_duration_; };
  std::shared_ptr<DB> GetDB() const { return db_; };
  uint32_t AclCategory() const;
  void AddAclCategory(uint32_t aclCategory);
  void SetDbName(const std::string& db_name) { db_name_ = db_name; }
  std::string GetDBName() { return db_name_; }

  std::string name() const;
  CmdRes& res();
  std::string db_name() const;
  PikaCmdArgsType& argv();
  virtual std::string ToRedisProtocol();

  void SetConn(const std::shared_ptr<net::NetConn>& conn);
  std::shared_ptr<net::NetConn> GetConn();

  void SetResp(const std::shared_ptr<std::string>& resp);
  std::shared_ptr<std::string> GetResp();

  void SetStage(CmdStage stage);
  void SetCmdId(uint32_t cmdId){cmdId_ = cmdId;}
  
  /*
  https://whoiami.github.io/PIKA_DATA_PATH

  DoBinlog的作用主要是将命令写入Binlog。 
  通过 ConsensusProposeLog => InternalAppendBinlog => 
  (std::shared_ptr<Binlog>)Logger()->Put(binlog)
   一系列的函数调用，最终调用class Binlog的Put接口将，binlog 字符串写入Binlog 文件当中。

   Binlog文件是由一个一个Blocks组成的，
   这样组织主要防止binlog文件的某一个点损坏造成整个文件不可读。
   每一个binlog 字符串先序列化成BinlogItem 结构，如黄色板块所示，组成BinlogItem之后，
   再加上8个bytes（Length，Time，Type）组成完整的可以落盘的数据。

  */
  virtual void DoBinlog();

  uint32_t GetCmdId() const { return cmdId_; };
  bool CheckArg(uint64_t num) const;

  bool IsCacheMissedInRtc() const;
  void SetCacheMissedInRtc(bool value);

 protected:
  // enable copy, used default copy
  // Cmd(const Cmd&);
  void ProcessCommand(const HintKeys& hint_key = HintKeys());
  void InternalProcessCommand(const HintKeys& hint_key);
  void DoCommand(const HintKeys& hint_key);
  bool DoReadCommandInCache();
  void LogCommand() const;

  std::string name_;
  int arity_ = -2;
  uint32_t flag_ = 0;

  std::vector<std::string> subCmdName_;  // sub command name, may be empty

 protected:
  // 承接cmd的result。
  CmdRes res_;
  PikaCmdArgsType argv_;
  std::string db_name_;
  rocksdb::Status s_;
  std::shared_ptr<DB> db_;
  std::shared_ptr<SyncMasterDB> sync_db_;
  std::weak_ptr<net::NetConn> conn_;
  std::weak_ptr<std::string> resp_;
  CmdStage stage_ = kNone;
  uint64_t do_duration_ = 0;
  uint32_t cmdId_ = 0;
  uint32_t aclCategory_ = 0;
  bool cache_missed_in_rtc_{false};

 private:
  virtual void DoInitial() = 0;
  virtual void Clear(){};

  Cmd& operator=(const Cmd&);
};

using CmdTable = std::unordered_map<std::string, std::unique_ptr<Cmd>>;

// Method for Cmd Table
void InitCmdTable(CmdTable* cmd_table);
Cmd* GetCmdFromDB(const std::string& opt, const CmdTable& cmd_table);

void RedisAppendContent(std::string& str, const std::string& value) {
  str.append(value.data(), value.size());
  str.append(kNewLine);
}

void RedisAppendLen(std::string& str, int64_t ori, const std::string& prefix) {
  char buf[32];
  pstd::ll2string(buf, 32, static_cast<long long>(ori));
  str.append(prefix);
  str.append(buf);
  str.append(kNewLine);
}

#endif
