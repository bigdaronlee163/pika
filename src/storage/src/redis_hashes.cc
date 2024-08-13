//  Copyright (c) 2017-present, Qihoo, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#include "src/redis.h"

#include <memory>

#include <fmt/core.h>
#include <glog/logging.h>

#include "pstd/include/pika_codis_slot.h"
#include "src/base_data_key_format.h"
#include "src/base_data_value_format.h"
#include "src/base_filter.h"
#include "src/scope_record_lock.h"
#include "src/scope_snapshot.h"
#include "storage/util.h"

namespace storage {
Status Redis::ScanHashesKeyNum(KeyInfo* key_info) {
  uint64_t keys = 0;
  uint64_t expires = 0;
  uint64_t ttl_sum = 0;
  uint64_t invaild_keys = 0;

  rocksdb::ReadOptions iterator_options;
  const rocksdb::Snapshot* snapshot;
  ScopeSnapshot ss(db_, &snapshot);
  iterator_options.snapshot = snapshot;
  iterator_options.fill_cache = false;

  int64_t curtime;
  rocksdb::Env::Default()->GetCurrentTime(&curtime);

  rocksdb::Iterator* iter = db_->NewIterator(iterator_options, handles_[kMetaCF]);
  for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    if (!ExpectedMetaValue(DataType::kHashes, iter->value().ToString())) {
      continue;
    }
    ParsedHashesMetaValue parsed_hashes_meta_value(iter->value());
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      invaild_keys++;
    } else {
      keys++;
      if (!parsed_hashes_meta_value.IsPermanentSurvival()) {
        expires++;
        ttl_sum += parsed_hashes_meta_value.Etime() - curtime;
      }
    }
  }
  delete iter;

  key_info->keys = keys;
  key_info->expires = expires;
  key_info->avg_ttl = (expires != 0) ? ttl_sum / expires : 0;
  key_info->invaild_keys = invaild_keys;
  return Status::OK();
}
/*
-  key ：要操作的哈希键。
-  fields ：要删除的字段列表。
-  ret ：指向一个整数的指针，用于返回实际删除的字段数量。
*/
Status Redis::HDel(const Slice& key, const std::vector<std::string>& fields, int32_t* ret) {
  /*
  -  statistic ：用于统计删除操作的数量。
  -  filtered_fields ：存储去重后的字段列表。
  -  field_set ：用于快速查找和去重字段。
  */
  uint32_t statistic = 0;
  std::vector<std::string> filtered_fields;
  std::unordered_set<std::string> field_set;
  for (const auto& iter : fields) {
    const std::string& field = iter;
    // 正常的迭代，没有像后面反向迭代。
    if (field_set.find(field) == field_set.end()) {
      field_set.insert(field);  // set去除重复的字段。
      filtered_fields.push_back(iter);
    }
  }
  /*
  初始化 RocksDB 批处理
  -  batch ：用于批量写入操作。
  -  read_options ：设置读取选项。
  -  snapshot ：用于快照读取。
  -  meta_value ：存储元数据的值。
  -  del_cnt ：计数器，用于统计删除的字段数量。
  -  version ：哈希的版本号。
  */
  rocksdb::WriteBatch batch;
  // 把go-redis过了一遍之后，发生代码看着更加亲切了。
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;

  std::string meta_value;
  int32_t del_cnt = 0;
  uint64_t version = 0;
  // - 使用作用域锁定和快照，以确保在操作期间数据的一致性。
  // 删除需要加锁操作。
  ScopeRecordLock l(lock_mgr_, key);
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;

  BaseMetaKey base_meta_key(key);
  // 没有额外的配置，都是使用的默认参数。
  // hash使用自己单独的列簇：kMetaCF
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  // 如果可以获取到，但是key的类型不是hash。
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    // 如果过期了，就证明这个key是不存在的。
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {  // 如果没有过期，就返回类型不对的错误。
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  //
  if (s.ok()) {
    // - 解析元数据，如果数据过期或计数为零，则直接返回。
    // meta_value 里面包含了 key 的版本信息。
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    // count代表了key关联的元素的个数。
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      *ret = 0;  //
      //
      return Status::OK();
    } else {
      // 如果没有过期，则进行进一步处理：删除key对应的中的字段。
      std::string data_value;
      version = parsed_hashes_meta_value.Version();
      for (const auto& field : filtered_fields) {
        HashesDataKey hashes_data_key(key, version, field);
        s = db_->Get(read_options, handles_[kHashesDataCF], hashes_data_key.Encode(), &data_value);
        if (s.ok()) {
          del_cnt++;
          statistic++;
          // 删除也是㝍。
          batch.Delete(handles_[kHashesDataCF], hashes_data_key.Encode());
        } else if (s.IsNotFound()) {
          continue;
        } else {
          return s;
        }
      }
      *ret = del_cnt;
      // key中的field删除。【更新key中的元素的数量。】
      if (!parsed_hashes_meta_value.CheckModifyCount(-del_cnt)) {
        // 这是为什么呢？【避免超出最大size的限制】
        return Status::InvalidArgument("hash size overflow");
      }
      parsed_hashes_meta_value.ModifyCount(-del_cnt);
      // 这里才真正删除。
      batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
    }
  } else if (s.IsNotFound()) {
    *ret = 0;
    return Status::OK();
  } else {
    return s;
  }
  // 这里写入，才是真的删除。
  s = db_->Write(default_write_options_, &batch);
  UpdateSpecificKeyStatistics(DataType::kHashes, key.ToString(), statistic);
  return s;
}

Status Redis::HExists(const Slice& key, const Slice& field) {
  std::string value;
  return HGet(key, field, &value);
}
/*


*/
Status Redis::HGet(const Slice& key, const Slice& field, std::string* value) {
  std::string meta_value;
  uint64_t version = 0;
  rocksdb::ReadOptions read_options;
  // 在 RocksDB 中，快照（ Snapshot
  // ）是一种用于读取数据的一致性视图。使用快照可以确保在读取数据时，数据的一致性和完整性。
  // 快照和事务相关，可以实现具体的事务。
  const rocksdb::Snapshot* snapshot;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      // 预期的类型和实际的类型。
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      version = parsed_hashes_meta_value.Version();
      /*
      1. HashesDataKey  是一个封装类，它将  key 、 version  和  field
      组合在一起，形成一个唯一的标识符。这样做可以使代码更加清晰，避免在多个地方直接操作原始数据。
      */
      HashesDataKey data_key(key, version, field);
      s = db_->Get(read_options, handles_[kHashesDataCF], data_key.Encode(), value);
      if (s.ok()) {
        // value是一个指针。 通过将value包装，调用其方法。
        ParsedBaseDataValue parsed_internal_value(value);
        if (parsed_internal_value.IsStale()) {
          return Status::NotFound("Stale");
        }
        parsed_internal_value.StripSuffix();
      }
    }
  }
  // 这里有用到。s = Status::NotFound();
  return s;
}

Status Redis::HGetall(const Slice& key, std::vector<FieldValue>* fvs) {
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;

  std::string meta_value;
  uint64_t version = 0;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;

  BaseMetaKey base_meta_key(key);
  // 通过CF和key来获取key的元信息，应该可以避免key冲突。
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      version = parsed_hashes_meta_value.Version();
      // 构建 hash key
      HashesDataKey hashes_data_key(key, version, "");
      // 编码成 寻找的 prefix
      Slice prefix = hashes_data_key.EncodeSeekKey();
      KeyStatisticsDurationGuard guard(this, DataType::kHashes, key.ToString());
      // 迭代器： 获取在hash cf中获取多个key
      auto iter = db_->NewIterator(read_options, handles_[kHashesDataCF]);
      for (iter->Seek(prefix); iter->Valid() && iter->key().starts_with(prefix); iter->Next()) {
        // 获取 db 中的 key value
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        ParsedBaseDataValue parsed_internal_value(iter->value());
        // fvs 入参，用于返回参数。
        // 从key中返回field  从 value中返回value
        fvs->push_back({parsed_hashes_data_key.field().ToString(), parsed_internal_value.UserValue().ToString()});
      }
      delete iter;
    }
  }
  return s;
}

/*
HGetallWithTTL  函数的主要作用是从 Redis 哈希中获取所有字段及其对应的值，
同时获取该哈希键的剩余生存时间（TTL）。
它通过使用 RocksDB 的快照和迭代器来实现这一点，
确保在读取数据时的一致性和完整性。函数的设计确保了对哈希表的有效访问和错误处理，
使得在 Redis 中操作哈希数据结构变得高效且可靠。
*/
Status Redis::HGetallWithTTL(const Slice& key, std::vector<FieldValue>* fvs, int64_t* ttl) {
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;

  std::string meta_value;
  uint64_t version = 0;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;
  BaseMetaKey base_meta_key(key);
  // 获取元信息。
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else if (parsed_hashes_meta_value.IsStale()) {
      return Status::NotFound("Stale");
    } else {
      // ttl
      // 获取具体的时间。【比redis中实现应该会简单一些。】
      /*

      - 解析元数据，检查是否有字段和是否过期。
      - 获取过期时间（ Etime ）并计算剩余的 TTL：
      - 如果 TTL 为 0，设置为 -1，表示没有设置过期时间。
      - 否则，计算当前时间与过期时间的差值。如果差值为负，设置为 -2，表示已过期。

      */
      *ttl = parsed_hashes_meta_value.Etime();
      if (*ttl == 0) {
        *ttl = -1;
      } else {
        int64_t curtime;
        rocksdb::Env::Default()->GetCurrentTime(&curtime);
        *ttl = *ttl - curtime >= 0 ? *ttl - curtime : -2;
      }
      /*
        - 创建一个  HashesDataKey  对象，用于生成前缀键。
        - 使用前缀键生成一个切片（ prefix ），并创建一个新的迭代器以遍历哈希表中的字段
      */
      version = parsed_hashes_meta_value.Version();
      HashesDataKey hashes_data_key(key, version, "");
      Slice prefix = hashes_data_key.EncodeSeekKey();
      KeyStatisticsDurationGuard guard(this, DataType::kHashes, key.ToString());
      // 5. **准备迭代器以获取字段值**
      auto iter = db_->NewIterator(read_options, handles_[kHashesDataCF]);
      /*
      - 使用迭代器从前缀开始查找，并遍历所有以该前缀开头的键。
      - 对每个有效的键，解析哈希字段和对应的值，并将它们存储到  fvs  中。
      */
      for (iter->Seek(prefix); iter->Valid() && iter->key().starts_with(prefix); iter->Next()) {
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        ParsedBaseDataValue parsed_internal_value(iter->value());
        fvs->push_back({parsed_hashes_data_key.field().ToString(), parsed_internal_value.UserValue().ToString()});
      }
      delete iter;
    }
  }
  return s;
}

/*
- **key**: 要操作的哈希键。
- **field**: 要增加值的字段。
- **value**: 增加的整数值。
- **ret**: 指向  int64_t  的指针，用于返回增加后的新值。
*/
Status Redis::HIncrby(const Slice& key, const Slice& field, int64_t value, int64_t* ret) {
  *ret = 0;
  rocksdb::WriteBatch batch;
  ScopeRecordLock l(lock_mgr_, key);

  uint64_t version = 0;
  uint32_t statistic = 0;
  std::string old_value;
  std::string meta_value;

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  char value_buf[32] = {0};
  char meta_value_buf[4] = {0};
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      //  - 如果字段不存在，创建新的字段并设置初始值。
      version = parsed_hashes_meta_value.UpdateVersion();
      parsed_hashes_meta_value.SetCount(1);
      parsed_hashes_meta_value.SetEtime(0);
      batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
      HashesDataKey hashes_data_key(key, version, field);
      Int64ToStr(value_buf, 32, value);
      BaseDataValue internal_value(value_buf);
      batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
      *ret = value;
    } else {
      //  - 如果字段存在，获取旧值并进行增量操作：
      version = parsed_hashes_meta_value.Version();
      HashesDataKey hashes_data_key(key, version, field);
      s = db_->Get(default_read_options_, handles_[kHashesDataCF], hashes_data_key.Encode(), &old_value);
      if (s.ok()) {
        ParsedBaseDataValue parsed_internal_value(&old_value);
        parsed_internal_value.StripSuffix();
        int64_t ival = 0;
        /*
          - 检查旧值是否为整数，如果不是，返回错误。
          - 检查增量操作是否会导致溢出。
          - 更新字段的值并写入数据库。
        */
        if (StrToInt64(old_value.data(), old_value.size(), &ival) == 0) {
          return Status::Corruption("hash value is not an integer");
        }
        if ((value >= 0 && LLONG_MAX - value < ival) || (value < 0 && LLONG_MIN - value > ival)) {
          return Status::InvalidArgument("Overflow");
        }
        *ret = ival + value;
        Int64ToStr(value_buf, 32, *ret);
        BaseDataValue internal_value(value_buf);
        batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
        statistic++;
      } else if (s.IsNotFound()) {
        // - 如果元数据不存在，创建新的哈希元数据并初始化字段的值。
        Int64ToStr(value_buf, 32, value);
        if (!parsed_hashes_meta_value.CheckModifyCount(1)) {
          return Status::InvalidArgument("hash size overflow");
        }
        BaseDataValue internal_value(value_buf);
        parsed_hashes_meta_value.ModifyCount(1);
        batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
        batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
        *ret = value;
      } else {
        return s;
      }
    }
  } else if (s.IsNotFound()) {
    // - 如果元数据不存在，创建新的哈希元数据并初始化字段的值。
    EncodeFixed32(meta_value_buf, 1);
    HashesMetaValue hashes_meta_value(DataType::kHashes, Slice(meta_value_buf, 4));
    version = hashes_meta_value.UpdateVersion();
    batch.Put(handles_[kMetaCF], base_meta_key.Encode(), hashes_meta_value.Encode());
    HashesDataKey hashes_data_key(key, version, field);

    Int64ToStr(value_buf, 32, value);
    BaseDataValue internal_value(value_buf);
    batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
    *ret = value;
  } else {
    return s;
  }
  s = db_->Write(default_write_options_, &batch);
  UpdateSpecificKeyStatistics(DataType::kHashes, key.ToString(), statistic);
  return s;
}

Status Redis::HIncrbyfloat(const Slice& key, const Slice& field, const Slice& by, std::string* new_value) {
  new_value->clear();
  rocksdb::WriteBatch batch;
  ScopeRecordLock l(lock_mgr_, key);

  uint64_t version = 0;
  uint32_t statistic = 0;
  std::string meta_value;
  std::string old_value_str;
  long double long_double_by;
  // 看参数是否正确。
  if (StrToLongDouble(by.data(), by.size(), &long_double_by) == -1) {
    return Status::Corruption("value is not a vaild float");
  }

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  char meta_value_buf[4] = {0};
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      version = parsed_hashes_meta_value.UpdateVersion();
      parsed_hashes_meta_value.SetCount(1);
      parsed_hashes_meta_value.SetEtime(0);
      batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
      HashesDataKey hashes_data_key(key, version, field);

      LongDoubleToStr(long_double_by, new_value);
      BaseDataValue inter_value(*new_value);
      batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), inter_value.Encode());
    } else {
      version = parsed_hashes_meta_value.Version();
      HashesDataKey hashes_data_key(key, version, field);
      s = db_->Get(default_read_options_, handles_[kHashesDataCF], hashes_data_key.Encode(), &old_value_str);
      if (s.ok()) {
        long double total;
        long double old_value;
        ParsedBaseDataValue parsed_internal_value(&old_value_str);
        parsed_internal_value.StripSuffix();
        if (StrToLongDouble(old_value_str.data(), old_value_str.size(), &old_value) == -1) {
          return Status::Corruption("value is not a vaild float");
        }

        total = old_value + long_double_by;
        if (LongDoubleToStr(total, new_value) == -1) {
          return Status::InvalidArgument("Overflow");
        }
        BaseDataValue internal_value(*new_value);
        batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
        statistic++;
      } else if (s.IsNotFound()) {
        LongDoubleToStr(long_double_by, new_value);
        if (!parsed_hashes_meta_value.CheckModifyCount(1)) {
          return Status::InvalidArgument("hash size overflow");
        }
        parsed_hashes_meta_value.ModifyCount(1);
        BaseDataValue internal_value(*new_value);
        batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
        batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
      } else {
        return s;
      }
    }
  } else if (s.IsNotFound()) {
    EncodeFixed32(meta_value_buf, 1);
    HashesMetaValue hashes_meta_value(DataType::kHashes, Slice(meta_value_buf, 4));
    version = hashes_meta_value.UpdateVersion();
    batch.Put(handles_[kMetaCF], base_meta_key.Encode(), hashes_meta_value.Encode());

    HashesDataKey hashes_data_key(key, version, field);
    LongDoubleToStr(long_double_by, new_value);
    BaseDataValue internal_value(*new_value);
    batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
  } else {
    return s;
  }
  s = db_->Write(default_write_options_, &batch);
  UpdateSpecificKeyStatistics(DataType::kHashes, key.ToString(), statistic);
  return s;
}
// 获取所有的key
Status Redis::HKeys(const Slice& key, std::vector<std::string>* fields) {
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;

  std::string meta_value;
  uint64_t version = 0;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      version = parsed_hashes_meta_value.Version();
      HashesDataKey hashes_data_key(key, version, "");
      Slice prefix = hashes_data_key.EncodeSeekKey();
      /*
      KeyStatisticsDurationGuard  是一个用于统计特定操作持续时间的
      RAII（资源获取即初始化）类。它的主要作用是帮助开发者在执行某个操作（如对特定键的读取或写入操作）时，自动跟踪和记录该操作的持续时间。
      */
      KeyStatisticsDurationGuard guard(this, DataType::kHashes, key.ToString());
      // 迭代器。
      auto iter = db_->NewIterator(read_options, handles_[kHashesDataCF]);
      for (iter->Seek(prefix); iter->Valid() && iter->key().starts_with(prefix); iter->Next()) {
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        fields->push_back(parsed_hashes_data_key.field().ToString());
      }
      delete iter;
    }
  }
  return s;
}

Status Redis::HLen(const Slice& key, int32_t* ret, std::string&& prefetch_meta) {
  *ret = 0;
  Status s;
  std::string meta_value(std::move(prefetch_meta));

  // meta_value is empty means no meta value get before,
  // we should get meta first
  if (meta_value.empty()) {
    BaseMetaKey base_meta_key(key);
    s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
    if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
      if (ExpectedStale(meta_value)) {
        s = Status::NotFound();
      } else {
        return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() + ", expect type: " +
                                       DataTypeStrings[static_cast<int>(DataType::kHashes)] + ", get type: " +
                                       DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
      }
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      *ret = 0;
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      *ret = parsed_hashes_meta_value.Count();
    }
  } else if (s.IsNotFound()) {
    *ret = 0;
  }
  return s;
}

Status Redis::HMGet(const Slice& key, const std::vector<std::string>& fields, std::vector<ValueStatus>* vss) {
  vss->clear();

  uint64_t version = 0;
  bool is_stale = false;
  std::string value;
  std::string meta_value;
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;
  // 通过 metakey 和 cf 获取元信息。
  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(read_options, handles_[kHashesDataCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    // 解析上面获取到的。
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if ((is_stale = parsed_hashes_meta_value.IsStale()) || parsed_hashes_meta_value.Count() == 0) {
      for (size_t idx = 0; idx < fields.size(); ++idx) {
        vss->push_back({std::string(), Status::NotFound()});
      }
      return Status::NotFound(is_stale ? "Stale" : "");
    } else {
      version = parsed_hashes_meta_value.Version();
      for (const auto& field : fields) {
        // 构建hash的key。包含三个内容，key  version field 保证和别的key不重复。
        HashesDataKey hashes_data_key(key, version, field);
        s = db_->Get(read_options, handles_[kHashesDataCF], hashes_data_key.Encode(), &value);
        if (s.ok()) {
          // 这里是get，获取到了value之后，需要解析。
          ParsedBaseDataValue parsed_internal_value(&value);
          parsed_internal_value.StripSuffix();
          vss->push_back({value, Status::OK()});
        } else if (s.IsNotFound()) {
          vss->push_back({std::string(), Status::NotFound()});
        } else {
          vss->clear();
          return s;
        }
      }
    }
    return Status::OK();
  } else if (s.IsNotFound()) {
    for (size_t idx = 0; idx < fields.size(); ++idx) {
      vss->push_back({std::string(), Status::NotFound()});
    }
  }
  return s;
}
// 同时set多个field 和 value .
// set 不支持 同时设置多个。
Status Redis::HMSet(const Slice& key, const std::vector<FieldValue>& fvs) {
  uint32_t statistic = 0;
  std::unordered_set<std::string> fields;
  std::vector<FieldValue> filtered_fvs;
  // 根据field来去重。FieldValue是一个结构体，用于参数field 和 value 。
  for (auto iter = fvs.rbegin(); iter != fvs.rend(); ++iter) {
    std::string field = iter->field;
    if (fields.find(field) == fields.end()) {
      fields.insert(field);           // 只存储field
      filtered_fvs.push_back(*iter);  // 存储 field 和 value
    }
  }

  rocksdb::WriteBatch batch;
  ScopeRecordLock l(lock_mgr_, key);

  uint64_t version = 0;
  std::string meta_value;

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  char meta_value_buf[4] = {0};
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  // key是存在的。
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    // key 存在但是过期了，或者key没有对应具体的元素。  写入过程，类似于 key 不存在的情况。
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      version = parsed_hashes_meta_value.InitialMetaValue();
      // 最多支持 INT32_MAX 个 filed value
      if (!parsed_hashes_meta_value.check_set_count(static_cast<int32_t>(filtered_fvs.size()))) {
        return Status::InvalidArgument("hash size overflow");
      }
      // 设置元素的个数。
      parsed_hashes_meta_value.SetCount(static_cast<int32_t>(filtered_fvs.size()));
      // 先存储 meta_value
      batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
      // 再依次存储 field value
      for (const auto& fv : filtered_fvs) {
        HashesDataKey hashes_data_key(key, version, fv.field);
        // set的时候，需要往里面㝍。需要构建一个新的 baseData
        BaseDataValue inter_value(fv.value);
        // 都存放到 batch 中，等下一起写入。
        batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), inter_value.Encode());
      }

    } else {
      // key 是存在的，但是需要更新或者新增 field value。
      int32_t count = 0;
      std::string data_value;
      // 更新版本。（旧版本会被剔除。）
      version = parsed_hashes_meta_value.Version();
      for (const auto& fv : filtered_fvs) {
        // 查询 field 是否存在。
        HashesDataKey hashes_data_key(key, version, fv.field);
        BaseDataValue inter_value(fv.value);
        s = db_->Get(default_read_options_, handles_[kHashesDataCF], hashes_data_key.Encode(), &data_value);
        // key + field 分为新创建的和更新的。
        if (s.ok()) {
          statistic++;
          batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), inter_value.Encode());
        } else if (s.IsNotFound()) {
          count++;
          batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), inter_value.Encode());
        } else {
          return s;
        }
      }
      if (!parsed_hashes_meta_value.CheckModifyCount(count)) {
        return Status::InvalidArgument("hash size overflow");
      }
      // 更新元信息。
      parsed_hashes_meta_value.ModifyCount(count);
      batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
    }
    // key是不存在的。
  } else if (s.IsNotFound()) {
    // char * 等价于 char []
    EncodeFixed32(meta_value_buf, filtered_fvs.size());
    // 构建 meta_value
    HashesMetaValue hashes_meta_value(DataType::kHashes, Slice(meta_value_buf, 4));
    // 更新或者创建版本信息。
    version = hashes_meta_value.UpdateVersion();
    // 创建 meta_value
    batch.Put(handles_[kMetaCF], base_meta_key.Encode(), hashes_meta_value.Encode());
    // 依次存储 field value
    for (const auto& fv : filtered_fvs) {
      HashesDataKey hashes_data_key(key, version, fv.field);
      BaseDataValue inter_value(fv.value);
      batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), inter_value.Encode());
    }
  }
  // 批量写入。
  s = db_->Write(default_write_options_, &batch);
  UpdateSpecificKeyStatistics(DataType::kHashes, key.ToString(), statistic);
  return s;
}
// 只能每次设置一个。
Status Redis::HSet(const Slice& key, const Slice& field, const Slice& value, int32_t* res) {
  rocksdb::WriteBatch batch;
  ScopeRecordLock l(lock_mgr_, key);

  uint64_t version = 0;
  uint32_t statistic = 0;
  std::string meta_value;
  // hash 使用BaseMetaKey
  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  char meta_value_buf[4] = {0};
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  // key 存在
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    // key 存在，但是过期或者里面没有元素。
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      version = parsed_hashes_meta_value.InitialMetaValue();
      parsed_hashes_meta_value.SetCount(1);
      batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
      HashesDataKey data_key(key, version, field);
      BaseDataValue internal_value(value);

      // internal_value.SetEtime(111);  // DDD

      batch.Put(handles_[kHashesDataCF], data_key.Encode(), internal_value.Encode());
      *res = 1;
    } else {
      // key存在，且里面还有元素。
      version = parsed_hashes_meta_value.Version();
      std::string data_value;
      HashesDataKey hashes_data_key(key, version, field);
      s = db_->Get(default_read_options_, handles_[kHashesDataCF], hashes_data_key.Encode(), &data_value);
      if (s.ok()) {
        *res = 0;
        if (data_value == value.ToString()) {
          return Status::OK();
        } else {
          // 使用value，构建出 db 需要的 key  value
          // key 使用  hashes_data_key(key, version, field); 构成。
          BaseDataValue internal_value(value);

          // internal_value.SetEtime(111);  // DDD

          batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
          statistic++;
        }
      } else if (s.IsNotFound()) {
        if (!parsed_hashes_meta_value.CheckModifyCount(1)) {
          return Status::InvalidArgument("hash size overflow");
        }
        parsed_hashes_meta_value.ModifyCount(1);
        BaseDataValue internal_value(value);

        // internal_value.SetEtime(111);  // DDD

        batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
        batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
        *res = 1;
      } else {
        return s;
      }
    }
  } else if (s.IsNotFound()) {
    EncodeFixed32(meta_value_buf, 1);
    HashesMetaValue hashes_meta_value(DataType::kHashes, Slice(meta_value_buf, 4));
    version = hashes_meta_value.UpdateVersion();
    batch.Put(handles_[kMetaCF], base_meta_key.Encode(), hashes_meta_value.Encode());
    HashesDataKey data_key(key, version, field);
    BaseDataValue internal_value(value);

    // internal_value.SetEtime(111);  // DDD

    batch.Put(handles_[kHashesDataCF], data_key.Encode(), internal_value.Encode());
    *res = 1;
  } else {
    return s;
  }
  s = db_->Write(default_write_options_, &batch);

  std::string* temp_value;
  HashesDataKey data_key(key, version, field);
  s = db_->Get(default_read_options_, handles_[kHashesDataCF], data_key.Encode(), temp_value);
  if (s.ok()) {
    // value是一个指针。 通过将value包装，调用其方法。
    ParsedBaseDataValue parsed_internal_value(temp_value);
    if (parsed_internal_value.IsStale()) {
      return Status::NotFound("Stale");
    }
    parsed_internal_value.StripSuffix();
  }

  UpdateSpecificKeyStatistics(DataType::kHashes, key.ToString(), statistic);
  return s;
}

Status Redis::HSetnx(const Slice& key, const Slice& field, const Slice& value, int32_t* ret) {
  rocksdb::WriteBatch batch;
  ScopeRecordLock l(lock_mgr_, key);

  uint64_t version = 0;
  std::string meta_value;

  BaseMetaKey base_meta_key(key);
  BaseDataValue internal_value(value);
  Status s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  char meta_value_buf[4] = {0};
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      version = parsed_hashes_meta_value.InitialMetaValue();
      parsed_hashes_meta_value.SetCount(1);
      batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
      HashesDataKey hashes_data_key(key, version, field);
      batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
      *ret = 1;
    } else {
      version = parsed_hashes_meta_value.Version();
      HashesDataKey hashes_data_key(key, version, field);
      std::string data_value;
      s = db_->Get(default_read_options_, handles_[kHashesDataCF], hashes_data_key.Encode(), &data_value);
      if (s.ok()) {
        *ret = 0;
      } else if (s.IsNotFound()) {
        if (!parsed_hashes_meta_value.CheckModifyCount(1)) {
          return Status::InvalidArgument("hash size overflow");
        }
        parsed_hashes_meta_value.ModifyCount(1);
        batch.Put(handles_[kMetaCF], base_meta_key.Encode(), meta_value);
        batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
        *ret = 1;
      } else {
        return s;
      }
    }
  } else if (s.IsNotFound()) {
    EncodeFixed32(meta_value_buf, 1);
    HashesMetaValue hashes_meta_value(DataType::kHashes, Slice(meta_value_buf, 4));
    version = hashes_meta_value.UpdateVersion();
    batch.Put(handles_[kMetaCF], base_meta_key.Encode(), hashes_meta_value.Encode());
    HashesDataKey hashes_data_key(key, version, field);
    batch.Put(handles_[kHashesDataCF], hashes_data_key.Encode(), internal_value.Encode());
    *ret = 1;
  } else {
    return s;
  }
  return db_->Write(default_write_options_, &batch);
}

// TODO 整个流程先走通。
/*
- `-2` if no such field exists in the provided hash key, or the provided key does not exist.
- `0` if the specified NX | XX | GT | LT condition has not been met.
- `1` if the expiration time was set/updated.
- `2` when `HEXPIRE`/`HPEXPIRE` is called with 0 seconds/milliseconds or when `HEXPIREAT`/`HPEXPIREAT` is called with a
past Unix time in seconds/milliseconds.

*/
Status Redis::HExpire(const Slice& key, int32_t ttl, int32_t numfields, const std::vector<std::string>& fields,
                      std::vector<int32_t>* rets) {
  if (ttl <= 0) {
    // 非法情况，不对rets赋值。
    // *ret = 2;
    return Status::InvalidArgument("invalid expire time, must be >= 0");
  }

  rocksdb::WriteBatch batch;
  ScopeRecordLock l(lock_mgr_, key);

  bool is_stale = false;
  int32_t version = 0;
  std::string meta_value;

  // rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;
  ScopeSnapshot ss(db_, &snapshot);

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);

  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    // not found 有两种结果，过期或者count为0.
    if (parsed_hashes_meta_value.IsStale()) {
      // *ret = -2;
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      // *ret = -2;
      return Status::NotFound();
    } else {
      version = parsed_hashes_meta_value.Version();

      for (const auto& field : fields) {
        HashesDataKey data_key(key, version, field);
        std::string data_value;
        s = db_->Get(default_read_options_, handles_[kHashesDataCF], data_key.Encode(), &data_value);
        if (s.ok()) {
          ParsedBaseDataValue parsed_internal_value(&data_value);
          // 存在一个过期，就直接返回。
          if (parsed_internal_value.IsStale()) {
            // *ret = 0;
            rets->push_back(-2);
            // return Status::NotFound("Stale");
          } else {
            rets->push_back(1);
            // 修改过期时间。
            // 怎么保证这个修改生效。 
            parsed_internal_value.SetRelativeTimestamp(ttl);
            batch.Put(handles_[kHashesDataCF], data_key.Encode(), data_value);
          }
        }

        s = db_->Write(default_write_options_, &batch);

        s = db_->Get(default_read_options_, handles_[kHashesDataCF], data_key.Encode(), &data_value);
        if (s.ok()) {
          ParsedBaseDataValue parsed_internal_value(&data_value);
          // 存在一个过期，就直接返回。
          if (parsed_internal_value.IsStale()) {
            // for test
          }
        }
      }
      s = db_->Write(default_write_options_, &batch);

      return s;
    }
  } else if (s.IsNotFound()) {
    return Status::NotFound(is_stale ? "Stale" : "111");
  }
  return s;
}



  // Status HExpireat(const Slice& key, const Slice& field, int32_t timestamp){
  // }

  // Status HExpireTime(const Slice& key, const Slice& field){

  // }

  // Status HPExpire(const Slice& key, const Slice& field, int32_t ttl){
  // }

  // Status HPExpireat(const Slice& key, const Slice& field, int32_t timestamp){
  // }

  // Status HPExpireTime(const Slice& key, const Slice& field){
  // }


  // Status HPersist(const Slice& key, const Slice& field){
  // }

  // Status HTTL(const Slice& key, const Slice& field){
  // }

  // Status HPTTL(const Slice& key, const Slice& field){
  // }




Status Redis::HVals(const Slice& key, std::vector<std::string>* values) {
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;

  std::string meta_value;
  uint64_t version = 0;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      version = parsed_hashes_meta_value.Version();
      HashesDataKey hashes_data_key(key, version, "");
      Slice prefix = hashes_data_key.EncodeSeekKey();
      KeyStatisticsDurationGuard guard(this, DataType::kHashes, key.ToString());
      auto iter = db_->NewIterator(read_options, handles_[kHashesDataCF]);
      for (iter->Seek(prefix); iter->Valid() && iter->key().starts_with(prefix); iter->Next()) {
        ParsedBaseDataValue parsed_internal_value(iter->value());
        values->push_back(parsed_internal_value.UserValue().ToString());
      }
      delete iter;
    }
  }
  return s;
}
/*
HStrlen  函数的主要功能是获取指定哈希键中某个字段的值的长度。
它通过调用  HGet  函数来获取字段值，并根据获取的结果返回相应的长度或错误状态。
这个函数在处理字符串数据时非常有用，特别是在需要知道字段长度的情况下。
*/
Status Redis::HStrlen(const Slice& key, const Slice& field, int32_t* len) {
  std::string value;
  Status s = HGet(key, field, &value);
  if (s.ok()) {
    *len = static_cast<int32_t>(value.size());
  } else {
    *len = 0;
  }
  return s;
}

Status Redis::HScan(const Slice& key, int64_t cursor, const std::string& pattern, int64_t count,
                    std::vector<FieldValue>* field_values, int64_t* next_cursor) {
  *next_cursor = 0;
  field_values->clear();
  if (cursor < 0) {
    *next_cursor = 0;
    return Status::OK();
  }

  int64_t rest = count;
  int64_t step_length = count;
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;

  std::string meta_value;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      *next_cursor = 0;
      return Status::NotFound();
    } else {
      std::string sub_field;
      std::string start_point;
      uint64_t version = parsed_hashes_meta_value.Version();
      s = GetScanStartPoint(DataType::kHashes, key, pattern, cursor, &start_point);
      if (s.IsNotFound()) {
        cursor = 0;
        if (isTailWildcard(pattern)) {
          start_point = pattern.substr(0, pattern.size() - 1);
        }
      }
      if (isTailWildcard(pattern)) {
        sub_field = pattern.substr(0, pattern.size() - 1);
      }

      HashesDataKey hashes_data_prefix(key, version, sub_field);
      HashesDataKey hashes_start_data_key(key, version, start_point);
      std::string prefix = hashes_data_prefix.EncodeSeekKey().ToString();
      KeyStatisticsDurationGuard guard(this, DataType::kHashes, key.ToString());
      rocksdb::Iterator* iter = db_->NewIterator(read_options, handles_[kHashesDataCF]);
      for (iter->Seek(hashes_start_data_key.Encode()); iter->Valid() && rest > 0 && iter->key().starts_with(prefix);
           iter->Next()) {
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        std::string field = parsed_hashes_data_key.field().ToString();
        if (StringMatch(pattern.data(), pattern.size(), field.data(), field.size(), 0) != 0) {
          ParsedBaseDataValue parsed_internal_value(iter->value());
          field_values->emplace_back(field, parsed_internal_value.UserValue().ToString());
        }
        rest--;
      }

      if (iter->Valid() && (iter->key().compare(prefix) <= 0 || iter->key().starts_with(prefix))) {
        *next_cursor = cursor + step_length;
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        std::string next_field = parsed_hashes_data_key.field().ToString();
        StoreScanNextPoint(DataType::kHashes, key, pattern, *next_cursor, next_field);
      } else {
        *next_cursor = 0;
      }
      delete iter;
    }
  } else {
    *next_cursor = 0;
    return s;
  }
  return Status::OK();
}

Status Redis::HScanx(const Slice& key, const std::string& start_field, const std::string& pattern, int64_t count,
                     std::vector<FieldValue>* field_values, std::string* next_field) {
  next_field->clear();
  field_values->clear();

  int64_t rest = count;
  std::string meta_value;
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      *next_field = "";
      return Status::NotFound();
    } else {
      uint64_t version = parsed_hashes_meta_value.Version();
      HashesDataKey hashes_data_prefix(key, version, Slice());
      HashesDataKey hashes_start_data_key(key, version, start_field);
      std::string prefix = hashes_data_prefix.EncodeSeekKey().ToString();
      KeyStatisticsDurationGuard guard(this, DataType::kHashes, key.ToString());
      rocksdb::Iterator* iter = db_->NewIterator(read_options, handles_[kHashesDataCF]);
      for (iter->Seek(hashes_start_data_key.Encode()); iter->Valid() && rest > 0 && iter->key().starts_with(prefix);
           iter->Next()) {
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        std::string field = parsed_hashes_data_key.field().ToString();
        if (StringMatch(pattern.data(), pattern.size(), field.data(), field.size(), 0) != 0) {
          ParsedBaseDataValue parsed_value(iter->value());
          field_values->emplace_back(field, parsed_value.UserValue().ToString());
        }
        rest--;
      }

      if (iter->Valid() && iter->key().starts_with(prefix)) {
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        *next_field = parsed_hashes_data_key.field().ToString();
      } else {
        *next_field = "";
      }
      delete iter;
    }
  } else {
    *next_field = "";
    return s;
  }
  return Status::OK();
}
/*
PKHScanRange  函数是一个用于在 Redis 哈希数据结构中扫描特定字段范围的函数。
它允许用户根据提供的起始字段、结束字段和匹配模式来获取字段及其对应的值。
PKHScanRange  函数是一个用于在 Redis 哈希数据结构中扫描特定字段范围的函数。
它允许用户根据提供的起始字段、结束字段和匹配模式来获取字段及其对应的值。
- **key**: 要操作的哈希键。
- **field_start**: 开始扫描的字段（可以为空）。
- **field_end**: 结束扫描的字段（可以为空）。
- **pattern**: 匹配模式，用于过滤字段。
- **limit**: 返回的字段数量限制。
- **field_values**: 用于存储匹配的字段及其值的向量。
- **next_field**: 用于返回下一个字段的指针。
*/
Status Redis::PKHScanRange(const Slice& key, const Slice& field_start, const std::string& field_end,
                           const Slice& pattern, int32_t limit, std::vector<FieldValue>* field_values,
                           std::string* next_field) {
  next_field->clear();
  field_values->clear();

  int64_t remain = limit;
  std::string meta_value;
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;

  bool start_no_limit = field_start.compare("") == 0;
  bool end_no_limit = field_end.empty();
  /*
  2. **检查范围有效性**：
   - 检查  field_start  和  field_end  的有效性。如果  field_start  大于  field_end ，则返回无效参数的状态。
  */
  if (!start_no_limit && !end_no_limit && (field_start.compare(field_end) > 0)) {
    return Status::InvalidArgument("error in given range");
  }

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      uint64_t version = parsed_hashes_meta_value.Version();
      HashesDataKey hashes_data_prefix(key, version, Slice());
      HashesDataKey hashes_start_data_key(key, version, field_start);
      /*
      5. **准备扫描**：
      - 获取哈希表的版本号。
      - 创建用于扫描的前缀键和起始数据键。
      - 使用  KeyStatisticsDurationGuard  监控操作的持续时间。
      */
      std::string prefix = hashes_data_prefix.EncodeSeekKey().ToString();
      KeyStatisticsDurationGuard guard(this, DataType::kHashes, key.ToString());
      rocksdb::Iterator* iter = db_->NewIterator(read_options, handles_[kHashesDataCF]);
      /*
      6. **创建迭代器并扫描字段**：
        - 创建一个新的迭代器以遍历哈希表中的字段。
        - 使用  Seek  方法开始扫描：
          - 如果  field_start  为空，则从哈希表的前缀开始。
          - 否则，从指定的  field_start  开始。
        - 在扫描过程中，检查每个字段是否满足以下条件：
          - 字段是否在  field_end  之前。
          - 字段是否匹配提供的模式。
          - 如果匹配，存储字段及其对应的值到  field_values  向量中。
          - 减少  remain  计数，直到达到限制
      */
      for (iter->Seek(start_no_limit ? prefix : hashes_start_data_key.Encode());
           iter->Valid() && remain > 0 && iter->key().starts_with(prefix); iter->Next()) {
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        std::string field = parsed_hashes_data_key.field().ToString();
        if (!end_no_limit && field.compare(field_end) > 0) {
          break;
        }
        if (StringMatch(pattern.data(), pattern.size(), field.data(), field.size(), 0) != 0) {
          ParsedBaseDataValue parsed_internal_value(iter->value());
          field_values->push_back({field, parsed_internal_value.UserValue().ToString()});
        }
        remain--;
      }

      if (iter->Valid() && iter->key().starts_with(prefix)) {
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        if (end_no_limit || parsed_hashes_data_key.field().compare(field_end) <= 0) {
          *next_field = parsed_hashes_data_key.field().ToString();
        }
      }
      delete iter;
    }
  } else {
    return s;
  }
  return Status::OK();
}

/*
 PKHRScanRange  函数的主要作用是在 Redis 哈希中逆向扫描特定字段范围，返回匹配的字段及其值。它通过使用 RocksDB
的迭代器来实现高效的字段遍历，同时支持起始和结束字段的限制以及模式匹配。
- **key**: 要操作的哈希键。
- **field_start**: 开始扫描的字段（可以为空）。
- **field_end**: 结束扫描的字段（可以为空）。
- **pattern**: 匹配模式，用于过滤字段。
- **limit**: 返回的字段数量限制。
- **field_values**: 用于存储匹配的字段及其值的向量。
- **next_field**: 用于返回下一个字段的指针。
*/
Status Redis::PKHRScanRange(const Slice& key, const Slice& field_start, const std::string& field_end,
                            const Slice& pattern, int32_t limit, std::vector<FieldValue>* field_values,
                            std::string* next_field) {
  next_field->clear();
  field_values->clear();

  int64_t remain = limit;
  std::string meta_value;
  rocksdb::ReadOptions read_options;
  const rocksdb::Snapshot* snapshot;
  ScopeSnapshot ss(db_, &snapshot);
  read_options.snapshot = snapshot;

  bool start_no_limit = field_start.compare("") == 0;
  bool end_no_limit = field_end.empty();

  if (!start_no_limit && !end_no_limit && (field_start.compare(field_end) < 0)) {
    return Status::InvalidArgument("error in given range");
  }

  BaseMetaKey base_meta_key(key);
  Status s = db_->Get(read_options, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
  if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
    if (ExpectedStale(meta_value)) {
      s = Status::NotFound();
    } else {
      return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() +
                                     ", expect type: " + DataTypeStrings[static_cast<int>(DataType::kHashes)] +
                                     ", get type: " + DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale() || parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      uint64_t version = parsed_hashes_meta_value.Version();
      int32_t start_key_version = start_no_limit ? version + 1 : version;
      std::string start_key_field = start_no_limit ? "" : field_start.ToString();
      HashesDataKey hashes_data_prefix(key, version, Slice());
      HashesDataKey hashes_start_data_key(key, start_key_version, start_key_field);
      std::string prefix = hashes_data_prefix.EncodeSeekKey().ToString();
      KeyStatisticsDurationGuard guard(this, DataType::kHashes, key.ToString());
      rocksdb::Iterator* iter = db_->NewIterator(read_options, handles_[kHashesDataCF]);
      for (iter->SeekForPrev(hashes_start_data_key.Encode().ToString());
           iter->Valid() && remain > 0 && iter->key().starts_with(prefix); iter->Prev()) {
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        std::string field = parsed_hashes_data_key.field().ToString();
        if (!end_no_limit && field.compare(field_end) < 0) {
          break;
        }
        if (StringMatch(pattern.data(), pattern.size(), field.data(), field.size(), 0) != 0) {
          ParsedBaseDataValue parsed_value(iter->value());
          field_values->push_back({field, parsed_value.UserValue().ToString()});
        }
        remain--;
      }

      if (iter->Valid() && iter->key().starts_with(prefix)) {
        ParsedHashesDataKey parsed_hashes_data_key(iter->key());
        if (end_no_limit || parsed_hashes_data_key.field().compare(field_end) >= 0) {
          *next_field = parsed_hashes_data_key.field().ToString();
        }
      }
      delete iter;
    }
  } else {
    return s;
  }
  return Status::OK();
}
/*
这个函数 Status Redis::HashesExpire(const Slice& key, int64_t ttl, std::string&& prefetch_meta) 是一个用于设置 Redis
哈希键过期时间的函数。以下是对该函数的详细解释：

函数参数
const Slice& key: 这是要设置过期时间的哈希键。
int64_t ttl: 这是要设置的过期时间（以秒为单位）。如果 ttl 为正数，则表示设置一个相对的过期时间；如果 ttl
为零或负数，则表示删除过期时间。 std::string&& prefetch_meta: 这是一个预取的元数据字符串，用于减少数据库查询次数。

*/
Status Redis::HashesExpire(const Slice& key, int64_t ttl, std::string&& prefetch_meta) {
  std::string meta_value(std::move(prefetch_meta));
  ScopeRecordLock l(lock_mgr_, key);
  BaseMetaKey base_meta_key(key);
  Status s;

  // meta_value is empty means no meta value get before,
  // we should get meta first
  if (meta_value.empty()) {
    s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
    if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
      if (ExpectedStale(meta_value)) {
        s = Status::NotFound();
      } else {
        return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() + ", expect type: " +
                                       DataTypeStrings[static_cast<int>(DataType::kHashes)] + ", get type: " +
                                       DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
      }
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    }
    // 如果 ttl 大于 0，设置相对过期时间：
    if (ttl > 0) {
      parsed_hashes_meta_value.SetRelativeTimestamp(ttl);
      s = db_->Put(default_write_options_, handles_[kMetaCF], base_meta_key.Encode(), meta_value);
    } else {
      parsed_hashes_meta_value.InitialMetaValue();
      s = db_->Put(default_write_options_, handles_[kMetaCF], base_meta_key.Encode(), meta_value);
    }
  }
  return s;
}
/*


*/
Status Redis::HashesDel(const Slice& key, std::string&& prefetch_meta) {
  std::string meta_value(std::move(prefetch_meta));
  ScopeRecordLock l(lock_mgr_, key);
  BaseMetaKey base_meta_key(key);
  Status s;

  // meta_value is empty means no meta value get before,
  // we should get meta first
  if (meta_value.empty()) {
    s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
    if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
      if (ExpectedStale(meta_value)) {
        s = Status::NotFound();
      } else {
        return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() + ", expect type: " +
                                       DataTypeStrings[static_cast<int>(DataType::kHashes)] + ", get type: " +
                                       DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
      }
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      uint32_t statistic = parsed_hashes_meta_value.Count();
      parsed_hashes_meta_value.InitialMetaValue();
      s = db_->Put(default_write_options_, handles_[kMetaCF], base_meta_key.Encode(), meta_value);
      UpdateSpecificKeyStatistics(DataType::kHashes, key.ToString(), statistic);
    }
  }
  return s;
}
/*

这个函数 Status Redis::HashesPersist(const Slice& key, std::string&& prefetch_meta) 的目的是移除 Redis
哈希键的过期时间，使其变为永久存在。以下是对该函数的详细解释：

函数参数
const Slice& key: 这是要移除过期时间的哈希键。
std::string&& prefetch_meta: 这是一个预取的元数据字符串，用于减少数据库查询次数。
函数逻辑

*/
Status Redis::HashesExpireat(const Slice& key, int64_t timestamp, std::string&& prefetch_meta) {
  std::string meta_value(std::move(prefetch_meta));
  ScopeRecordLock l(lock_mgr_, key);
  BaseMetaKey base_meta_key(key);
  Status s;

  // meta_value is empty means no meta value get before,
  // we should get meta first
  if (meta_value.empty()) {
    s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
    if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
      if (ExpectedStale(meta_value)) {
        s = Status::NotFound();
      } else {
        return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() + ", expect type: " +
                                       DataTypeStrings[static_cast<int>(DataType::kHashes)] + ", get type: " +
                                       DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
      }
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      if (timestamp > 0) {
        parsed_hashes_meta_value.SetEtime(static_cast<uint64_t>(timestamp));
      } else {
        parsed_hashes_meta_value.InitialMetaValue();
      }
      s = db_->Put(default_write_options_, handles_[kMetaCF], base_meta_key.Encode(), meta_value);
    }
  }
  return s;
}
/*


*/
Status Redis::HashesPersist(const Slice& key, std::string&& prefetch_meta) {
  std::string meta_value(std::move(prefetch_meta));
  ScopeRecordLock l(lock_mgr_, key);
  BaseMetaKey base_meta_key(key);
  Status s;

  // meta_value is empty means no meta value get before,
  // we should get meta first
  if (meta_value.empty()) {
    s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
    if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
      if (ExpectedStale(meta_value)) {
        s = Status::NotFound();
      } else {
        return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() + ", expect type: " +
                                       DataTypeStrings[static_cast<int>(DataType::kHashes)] + ", get type: " +
                                       DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
      }
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      return Status::NotFound();
    } else {
      uint64_t timestamp = parsed_hashes_meta_value.Etime();
      if (timestamp == 0) {
        return Status::NotFound("Not have an associated timeout");
      } else {
        // 不设置具体的过期时间。持久化。
        parsed_hashes_meta_value.SetEtime(0);
        s = db_->Put(default_write_options_, handles_[kMetaCF], base_meta_key.Encode(), meta_value);
      }
    }
  }
  return s;
}
/*

这个函数 Status Redis::HashesTTL(const Slice& key, int64_t* timestamp, std::string&& prefetch_meta) 的目的是获取 Redis
哈希键的剩余生存时间（TTL）。以下是对该函数的详细解释：

函数参数
const Slice& key: 这是要查询过期时间的哈希键。
int64_t* timestamp: 这是一个指向 int64_t 类型的指针，用于存储查询到的过期时间。
std::string&& prefetch_meta: 这是一个预取的元数据字符串，用于减少数据库查询次数。
函数逻辑
*/
Status Redis::HashesTTL(const Slice& key, int64_t* timestamp, std::string&& prefetch_meta) {
  std::string meta_value(std::move(prefetch_meta));
  Status s;
  BaseMetaKey base_meta_key(key);

  // meta_value is empty means no meta value get before,
  // we should get meta first
  if (meta_value.empty()) {
    s = db_->Get(default_read_options_, handles_[kMetaCF], base_meta_key.Encode(), &meta_value);
    if (s.ok() && !ExpectedMetaValue(DataType::kHashes, meta_value)) {
      if (ExpectedStale(meta_value)) {
        s = Status::NotFound();
      } else {
        return Status::InvalidArgument("WRONGTYPE, key: " + key.ToString() + ", expect type: " +
                                       DataTypeStrings[static_cast<int>(DataType::kHashes)] + ", get type: " +
                                       DataTypeStrings[static_cast<int>(GetMetaValueType(meta_value))]);
      }
    }
  }
  if (s.ok()) {
    ParsedHashesMetaValue parsed_hashes_meta_value(&meta_value);
    if (parsed_hashes_meta_value.IsStale()) {
      *timestamp = -2;
      return Status::NotFound("Stale");
    } else if (parsed_hashes_meta_value.Count() == 0) {
      *timestamp = -2;
      return Status::NotFound();
    } else {
      *timestamp = parsed_hashes_meta_value.Etime();
      if (*timestamp == 0) {
        *timestamp = -1;
      } else {
        int64_t curtime;
        rocksdb::Env::Default()->GetCurrentTime(&curtime);
        *timestamp = *timestamp - curtime >= 0 ? *timestamp - curtime : -2;
      }
    }
  } else if (s.IsNotFound()) {
    *timestamp = -2;
  }
  return s;
}

/*
这个函数 void Redis::ScanHashes() 的目的是扫描和打印 Redis 哈希键的元数据和字段数据。以

*/
void Redis::ScanHashes() {
  /*
  rocksdb::ReadOptions iterator_options;：初始化 RocksDB 读取选项。
  const rocksdb::Snapshot* snapshot;：声明一个快照指针。
  ScopeSnapshot ss(db_, &snapshot);：创建一个作用域快照对象，以确保在函数执行期间使用一致的数据库视图。
  iterator_options.snapshot = snapshot;：将快照设置为读取选项的一部分。
  iterator_options.fill_cache = false;：禁用读取缓存，以避免影响数据库性能。
  auto current_time = static_cast<int32_t>(time(nullptr));：获取当前时间（以秒为单位）。
  */
  rocksdb::ReadOptions iterator_options;
  const rocksdb::Snapshot* snapshot;
  ScopeSnapshot ss(db_, &snapshot);
  iterator_options.snapshot = snapshot;
  iterator_options.fill_cache = false;
  auto current_time = static_cast<int32_t>(time(nullptr));

  LOG(INFO) << "***************" << "rocksdb instance: " << index_ << " Hashes Meta Data***************";
  auto meta_iter = db_->NewIterator(iterator_options, handles_[kMetaCF]);
  for (meta_iter->SeekToFirst(); meta_iter->Valid(); meta_iter->Next()) {
    if (!ExpectedMetaValue(DataType::kHashes, meta_iter->value().ToString())) {
      continue;
    }
    // 这里包含了value的过期时间，可以设计具体value的过期时间。
    ParsedHashesMetaValue parsed_hashes_meta_value(meta_iter->value());
    int32_t survival_time = 0;
    if (parsed_hashes_meta_value.Etime() != 0) {
      survival_time =
          parsed_hashes_meta_value.Etime() > current_time ? parsed_hashes_meta_value.Etime() - current_time : -1;
    }
    ParsedBaseMetaKey parsed_meta_key(meta_iter->key());

    LOG(INFO) << fmt::format("[key : {:<30}] [count : {:<10}] [timestamp : {:<10}] [version : {}] [survival_time : {}]",
                             parsed_meta_key.Key().ToString(), parsed_hashes_meta_value.Count(),
                             parsed_hashes_meta_value.Etime(), parsed_hashes_meta_value.Version(), survival_time);
  }
  delete meta_iter;

  LOG(INFO) << "***************Hashes Field Data***************";
  auto field_iter = db_->NewIterator(iterator_options, handles_[kHashesDataCF]);
  for (field_iter->SeekToFirst(); field_iter->Valid(); field_iter->Next()) {
    ParsedHashesDataKey parsed_hashes_data_key(field_iter->key());
    ParsedBaseDataValue parsed_internal_value(field_iter->value());

    LOG(INFO) << fmt::format("[key : {:<30}] [field : {:<20}] [value : {:<20}] [version : {}]",
                             parsed_hashes_data_key.Key().ToString(), parsed_hashes_data_key.field().ToString(),
                             parsed_internal_value.UserValue().ToString(), parsed_hashes_data_key.Version());
  }
  delete field_iter;
}

}  //  namespace storage
