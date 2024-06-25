// Copyright (c) 2015-present, Qihoo, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#ifndef PIKA_STATISTIC_H_
#define PIKA_STATISTIC_H_

#include <atomic>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class QpsStatistic {
 public:
  QpsStatistic();
  QpsStatistic(const QpsStatistic& other);
  ~QpsStatistic() = default;
  void IncreaseQueryNum(bool is_write);
  void ResetLastSecQuerynum();

  std::atomic<uint64_t> querynum;
  std::atomic<uint64_t> write_querynum;

  std::atomic<uint64_t> last_querynum;
  std::atomic<uint64_t> last_write_querynum;

  std::atomic<uint64_t> last_sec_querynum;
  std::atomic<uint64_t> last_sec_write_querynum;

  std::atomic<uint64_t> last_time_us;
};

struct ServerStatistic {
  ServerStatistic() = default;
  ~ServerStatistic() = default;

  std::atomic<uint64_t> accumulative_connections;
  std::unordered_map<std::string, std::atomic<uint64_t>> exec_count_db;
  QpsStatistic qps;
};

struct Statistic {
  Statistic();

  QpsStatistic DBStat(const std::string& db_name);
  std::unordered_map<std::string, QpsStatistic> AllDBStat();

  void UpdateDBQps(const std::string& db_name, const std::string& command, bool is_write);
  void ResetDBLastSecQuerynum();

  // statistic shows accumulated data of all tables
  ServerStatistic server_stat;

  // statistic shows accumulated data of every single table
  std::shared_mutex db_stat_rw;
  std::unordered_map<std::string, QpsStatistic> db_stat;
};

struct DiskStatistic {
  std::atomic<uint64_t> db_size_ = 0;
  std::atomic<uint64_t> log_size_ = 0;
};



struct CmdStatistics{
  std::map<std::string, CmdStatistic> cmdStatistics;
  // 慢命令的统计时间阈值
  uint64_t slow_threshold_once;
  uint64_t slow_threshold_N;
  uint64_t slow_threshold_window_mean;
  uint64_t slow_threshold_window_N;

   
};

struct  CmdStatistic{
  //  std::map<std::string, >
  // 统计的时候，就计算。
  // 在下次执行的时候，就判断是否为慢命令。如果是就将其加入慢队列的命令中。
  std::string cmd;
  uint64_t cmd_count;
  uint64_t fist_time;
  uint64_t last_time;
  uint64_t window_time;

  uint64_t slow_once;
  uint64_t slow_N;
  uint64_t slow_window_N;
  uint64_t slow_window_mean;


  // 0000 0000
  // slow_window_mean slow_window_N slow_N slow_once
  // 总共是四个条件，如果有一个条件达到就将 threshold 对应的低位置1
  // 然后如果 threshold >= 12（ （0000 1111) ->  15） 12/15 = 0.8 就将其加入慢命令列表。
  // 如果后续不满足就将其剔除。
  /*
  1. 如果最新的一次slow_once 没有超出阈值，则 threshold 减1 剩 14 。
  2. 如果slow_N不满足， threshold 减 2 剩 13
  3. slow_window_mean 不满足  threshold 减 4 剩 11
  * 可以保证，不会窗口时间内，反复将一个命令调整成慢命令或者快命令。
  */
  uint8_t threshold;

};


#endif  // PIKA_STATISTIC_H_
