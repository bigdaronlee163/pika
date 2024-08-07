//  Copyright (c) 2017-present, Qihoo, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#ifndef SRC_BASE_META_VALUE_FORMAT_H_
#define SRC_BASE_META_VALUE_FORMAT_H_

#include <string>

#include "pstd/include/env.h"
#include "storage/storage_define.h"
#include "src/base_value_format.h"

namespace storage {

/*
*| type | value |  version | reserve | cdate | timestamp |
*|  1B  |       |    8B   |   16B    |   8B  |     8B    |
*/
// TODO(wangshaoyi): reformat encode, AppendTimestampAndVersion
class BaseMetaValue : public InternalValue {
 public:
  /*
   * Constructing MetaValue requires passing in a type value
   */
  explicit BaseMetaValue(DataType type, const Slice& user_value) : InternalValue(type, user_value) {}
  rocksdb::Slice Encode() override {
    size_t usize = user_value_.size();
    // kSuffixReserveLength 对应于上面的 reserve 。是16个字节。【比redis还是浪费空间一些。 但是pika用的事磁盘。】
    size_t needed = usize + kVersionLength + kSuffixReserveLength + 2 * kTimestampLength + kTypeLength;
    char* dst = ReAllocIfNeeded(needed);
    // 将type 复制到指定的dst地址开始的内存。并且增加dst. 然后再按着顺序copy。
    memcpy(dst, &type_, sizeof(type_));
    dst += sizeof(type_);
    char* start_pos = dst;

    memcpy(dst, user_value_.data(), user_value_.size());
    dst += user_value_.size();
    // EncodeFixed64 也区分大小端。 
    EncodeFixed64(dst, version_);
    dst += sizeof(version_);
    memcpy(dst, reserve_, sizeof(reserve_));
    dst += sizeof(reserve_);
    EncodeFixed64(dst, ctime_);
    dst += sizeof(ctime_);
    EncodeFixed64(dst, etime_);
    return {start_, needed};
  }

  uint64_t UpdateVersion() {
    int64_t unix_time = pstd::NowMicros() / 1000000;
    if (version_ >= unix_time) {
      version_++;
    } else {
      version_ = uint64_t(unix_time);
    }
    return version_;
  }
};

class ParsedBaseMetaValue : public ParsedInternalValue {
 public:
  // Use this constructor after rocksdb::DB::Get();
  /*
   - 该构造函数在从 RocksDB 中获取数据后使用。它接收一个指向字符串的指针，解析该字符串以提取元数据。 
   - 解析过程包括提取数据类型、用户值、版本、创建时间（ctime）、结束时间（etime）等字段。

    -  count_ ：表示与该元数据相关的元素数量。 
    -  version_ ：表示元数据的版本号。 
    -  user_value_ ：存储用户值的切片。 
    -  ctime_  和  etime_ ：分别表示创建时间和结束时间的时间戳。 
    -  reserve_ ：保留字段，用于存储额外的信息。 
  
  
  */
  explicit ParsedBaseMetaValue(std::string* internal_value_str) : ParsedInternalValue(internal_value_str) {
    // 在这里对value按照内存进行解析。
    if (internal_value_str->size() >= kBaseMetaValueSuffixLength) {
      size_t offset = 0;
      // 第一个字节。
      type_ = static_cast<DataType>(static_cast<uint8_t>((*internal_value_str)[0]));
      offset += kTypeLength;
      // 用户的value从[internal_value_str->data() + offset， internal_value_str->size() - kBaseMetaValueSuffixLength - offset]
      user_value_ = Slice(internal_value_str->data() + offset,
                             internal_value_str->size() - kBaseMetaValueSuffixLength - offset);
      offset += user_value_.size();
      version_ = DecodeFixed64(internal_value_str->data() + offset);
      offset += sizeof(version_);
      memcpy(reserve_, internal_value_str->data() + offset, sizeof(reserve_));
      offset += sizeof(reserve_);
      ctime_ = DecodeFixed64(internal_value_str->data() + offset);
      offset += sizeof(ctime_);
      etime_ = DecodeFixed64(internal_value_str->data() + offset);
    }
    count_ = DecodeFixed32(internal_value_str->data() + kTypeLength);
  }

  // Use this constructor in rocksdb::CompactionFilter::Filter();
  /*
  - 该构造函数在 RocksDB 的压缩过滤器中使用，接收一个  Slice  对象（一个轻量级的只读字符串视图），以类似的方式解析元数据。 
  */
  explicit ParsedBaseMetaValue(const Slice& internal_value_slice) : ParsedInternalValue(internal_value_slice) {
    if (internal_value_slice.size() >= kBaseMetaValueSuffixLength) {
      size_t offset = 0;
      type_ = static_cast<DataType>(static_cast<uint8_t>(internal_value_slice[0]));
      offset += kTypeLength;
      user_value_ = Slice(internal_value_slice.data() + offset,
                          internal_value_slice.size() - kBaseMetaValueSuffixLength - offset);
      offset += user_value_.size();
      version_ = DecodeFixed64(internal_value_slice.data() + offset);
      offset += sizeof(uint64_t);
      memcpy(reserve_, internal_value_slice.data() + offset, sizeof(reserve_));
      offset += sizeof(reserve_);
      ctime_ = DecodeFixed64(internal_value_slice.data() + offset);
      offset += sizeof(ctime_);
      etime_ = DecodeFixed64(internal_value_slice.data() + offset);
    }
    count_ = DecodeFixed32(internal_value_slice.data() + kTypeLength);
  }

  void StripSuffix() override {
    if (value_) {
      value_->erase(value_->size() - kBaseMetaValueSuffixLength, kBaseMetaValueSuffixLength);
    }
  }

  void SetVersionToValue() override {
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kBaseMetaValueSuffixLength;
      EncodeFixed64(dst, version_);
    }
  }

  void SetCtimeToValue() override {
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - 2 * kTimestampLength;
      EncodeFixed64(dst, ctime_);
    }
  }

  void SetEtimeToValue() override {
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kTimestampLength;
      EncodeFixed64(dst, etime_);
    }
  }

  uint64_t InitialMetaValue() {
    this->SetCount(0);
    this->SetEtime(0);
    this->SetCtime(0);
    return this->UpdateVersion();
  }

  bool IsValid() override {
    return !IsStale() && Count() != 0;
  }

  bool check_set_count(size_t count) {
    if (count > INT32_MAX) {
      return false;
    }
    return true;
  }

  int32_t Count() { return count_; }

  void SetCount(int32_t count) {
    count_ = count;
    if (value_) {
      char* dst = const_cast<char*>(value_->data());
      EncodeFixed32(dst + kTypeLength, count_);
    }
  }

  bool CheckModifyCount(int32_t delta) {
    int64_t count = count_;
    count += delta;
    if (count < 0 || count > INT32_MAX) {
      return false;
    }
    return true;
  }

  void ModifyCount(int32_t delta) {
    count_ += delta;
    if (value_) {
      char* dst = const_cast<char*>(value_->data());
      EncodeFixed32(dst + kTypeLength, count_);
    }
  }

  uint64_t UpdateVersion() {
    int64_t unix_time;
    rocksdb::Env::Default()->GetCurrentTime(&unix_time);
    if (version_ >= static_cast<uint64_t>(unix_time)) {
      version_++;
    } else {
      version_ = static_cast<uint64_t>(unix_time);
    }
    SetVersionToValue();
    return version_;
  }

 private:
  /*
  *| type | value |  version | reserve | cdate | timestamp |
  *|  1B  |       |    8B   |   16B    |   8B  |     8B    |
  */
 // 这里计算后面的四个字节的长度。
  static const size_t kBaseMetaValueSuffixLength = kVersionLength + kSuffixReserveLength + 2 * kTimestampLength;
  // 这边增加了一个count_ 的字段。 【这个字段就是计算数据结构中元素的个数。】
  //  `count_` 字段是一个整型变量，用来存储某种计数或数量的值。
  // 它可以用来跟踪某个对象、事件或状态发生的次数。通过增加 `count_` 字段，
  // 可以方便地记录和追踪某个特定属性的数量，从而在程序中进行相应的逻辑处理或统计分析。
  int32_t count_ = 0;
};

// hash set zset 使用这里的value的定义。
using HashesMetaValue = BaseMetaValue;
using ParsedHashesMetaValue = ParsedBaseMetaValue;
using SetsMetaValue = BaseMetaValue;
using ParsedSetsMetaValue = ParsedBaseMetaValue;
using ZSetsMetaValue = BaseMetaValue;
using ParsedZSetsMetaValue = ParsedBaseMetaValue;

}  //  namespace storage
#endif  // SRC_BASE_META_VALUE_FORMAT_H_
