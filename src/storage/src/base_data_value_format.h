//  Copyright (c) 2023-present, Qihoo, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#ifndef SRC_BASE_DATA_VALUE_FORMAT_H_
#define SRC_BASE_DATA_VALUE_FORMAT_H_

#include <string>

#include "rocksdb/env.h"
#include "rocksdb/slice.h"

#include "base_value_format.h"
#include "src/coding.h"
#include "src/mutex.h"
#include "storage/storage_define.h"

namespace storage {
/*
 * hash/set/zset/list data value format
 * | value | reserve | ctime | etime |
 * |       |   16B   |   8B  |   8B  |
 */
class BaseDataValue : public InternalValue {
 public:
  /*
   * The header of the Value field is initially initialized to knulltype
   */
  explicit BaseDataValue(const rocksdb::Slice& user_value) : InternalValue(DataType::kNones, user_value) {}
  virtual ~BaseDataValue() {}
  // Encode 方法，是没有将 etime_ 编码进去的。
  // 所以是不行的？
  // 直接改变这个还是新建一个类型？(不管是新建一个类型和新增编码类型，实质上都是要加上一个etime 用于判断是否过期。)
  virtual rocksdb::Slice Encode() {
    size_t usize = user_value_.size();
    // 需要的内存的空间，还需要加上 reserver ctime的长度。
    size_t needed = usize + kSuffixReserveLength + kTimestampLength * 2;
    char* dst = ReAllocIfNeeded(needed);
    char* start_pos = dst;

    memcpy(dst, user_value_.data(), user_value_.size());
    dst += user_value_.size();
    memcpy(dst, reserve_, kSuffixReserveLength);
    dst += kSuffixReserveLength;
    EncodeFixed64(dst, ctime_);
    dst += kTimestampLength;
    EncodeFixed64(dst, etime_);
    // 构造一个slice，起始地址加长度。
    return rocksdb::Slice(start_pos, needed);
  }

 private:
  // 后面用来移除 后缀，获取单纯的用户数据。
  const size_t kDefaultValueSuffixLength = kSuffixReserveLength + kTimestampLength * 2;
};

class ParsedBaseDataValue : public ParsedInternalValue {
 public:
  // Use this constructor after rocksdb::DB::Get(), since we use this in
  // the implement of user interfaces and may need to modify the
  // original value suffix, so the value_ must point to the string
  // 通过这个构造函数，获取具体的内容。
  explicit ParsedBaseDataValue(std::string* value) : ParsedInternalValue(value) {
    // 只有大于 辅助信息，才有用户的数据。
    if (value_->size() >= kBaseDataValueSuffixLength) {
      // 解析具体的内容。
      user_value_ = rocksdb::Slice(value_->data(), value_->size() - kBaseDataValueSuffixLength);
      // 直接复制内存。
      memcpy(reserve_, value_->data() + user_value_.size(), kSuffixReserveLength);
      // 解析。
      ctime_ = DecodeFixed64(value_->data() + user_value_.size() + kSuffixReserveLength);
      etime_ = DecodeFixed64(value_->data() + user_value_.size() + kSuffixReserveLength + kTimestampLength);
    }
  }

  // Use this constructor in rocksdb::CompactionFilter::Filter(),
  // since we use this in Compaction process, all we need to do is parsing
  // the rocksdb::Slice, so don't need to modify the original value, value_ can be
  // set to nullptr
  explicit ParsedBaseDataValue(const rocksdb::Slice& value) : ParsedInternalValue(value) {
    if (value.size() >= kBaseDataValueSuffixLength) {
      user_value_ = rocksdb::Slice(value.data(), value.size() - kBaseDataValueSuffixLength);
      memcpy(reserve_, value.data() + user_value_.size(), kSuffixReserveLength);
      ctime_ = DecodeFixed64(value.data() + user_value_.size() + kSuffixReserveLength);
      etime_ = DecodeFixed64(value_->data() + user_value_.size() + kSuffixReserveLength + kTimestampLength);
    }
  }

  virtual ~ParsedBaseDataValue() = default;
  // base
  void SetEtimeToValue() override {
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kTimestampLength ;
      EncodeFixed64(dst, etime_);
    }
  }
  // 通过rocksdb返回的内容，解析具体的内容。
  void SetCtimeToValue() override {
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kTimestampLength - kTimestampLength;
      EncodeFixed64(dst, ctime_);
    }
  }

  void SetReserveToValue() {
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kBaseDataValueSuffixLength;
      memcpy(dst, reserve_, kSuffixReserveLength);
    }
  }
  // 移除 reserve 和  ctime 后缀的信息内容。
  virtual void StripSuffix() override {
    if (value_) {
      value_->erase(value_->size() - kBaseDataValueSuffixLength, kBaseDataValueSuffixLength);
    }
  }

 protected:
  virtual void SetVersionToValue() override {};

 private:
  const size_t kBaseDataValueSuffixLength = kSuffixReserveLength + kTimestampLength * 2;
};

}  //  namespace storage
#endif  // SRC_BASE_VALUE_FORMAT_H_
