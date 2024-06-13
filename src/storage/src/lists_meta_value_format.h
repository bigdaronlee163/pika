//  Copyright (c) 2017-present, Qihoo, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#ifndef SRC_LISTS_META_VALUE_FORMAT_H_
#define SRC_LISTS_META_VALUE_FORMAT_H_

#include <string>

#include "src/base_value_format.h"
#include "storage/storage_define.h"

namespace storage {

const uint64_t InitalLeftIndex = 9223372036854775807;
const uint64_t InitalRightIndex = 9223372036854775808U;

/*
* 比 hash  zset  set 增加了 2 个字段，现在共8个字段。 
*| type | list_size | version | left index | right index | reserve |  cdate | timestamp |
*|  1B  |     8B    |    8B   |     8B     |      8B     |   16B   |    8B  |     8B    |

 list size 和 left index 和 right index的作用是什么？ 

  列表的大小（list size）是指列表中元素的数量。左索引（left index）和右索引（right index）是用于访问列表中元素的两个指针，它们分别表示列表中第一个元素和最后一个元素的位置。

左索引通常用0或-1表示，表示列表中的第一个元素。例如，如果列表中有5个元素，那么左索引为0，表示第一个元素。右索引通常用列表大小减1表示，表示列表中的最后一个元素。例如，如果列表中有5个元素，那么右索引为4，表示最后一个元素。
 
*/
class ListsMetaValue : public InternalValue {
 public:
  explicit ListsMetaValue(const rocksdb::Slice& user_value)
      : InternalValue(DataType::kLists, user_value), left_index_(InitalLeftIndex), right_index_(InitalRightIndex) {}
  // 这个里面都是编码的函数
  rocksdb::Slice Encode() override {
    size_t usize = user_value_.size();
    size_t needed = usize + kVersionLength + 2 * kListValueIndexLength +
                    kSuffixReserveLength + 2 * kTimestampLength + kTypeLength;
    char* dst = ReAllocIfNeeded(needed);
    memcpy(dst, &type_, sizeof(type_));
    dst += sizeof(type_);
    char* start_pos = dst;

    memcpy(dst, user_value_.data(), usize);
    dst += usize;
    EncodeFixed64(dst, version_);
    dst += kVersionLength;
    EncodeFixed64(dst, left_index_);
    dst += kListValueIndexLength;
    EncodeFixed64(dst, right_index_);
    dst += kListValueIndexLength;
    memcpy(dst, reserve_, sizeof(reserve_));
    dst += kSuffixReserveLength;
    EncodeFixed64(dst, ctime_);
    dst += kTimestampLength;
    EncodeFixed64(dst, etime_);
    return {start_, needed};
  }

  uint64_t UpdateVersion() {
    int64_t unix_time;
    rocksdb::Env::Default()->GetCurrentTime(&unix_time);
    if (version_ >= static_cast<uint64_t>(unix_time)) {
      version_++;
    } else {
      version_ = static_cast<uint64_t>(unix_time);
    }
    return version_;
  }

  uint64_t LeftIndex() { return left_index_; }

  void ModifyLeftIndex(uint64_t index) { left_index_ -= index; }

  uint64_t RightIndex() { return right_index_; }

  void ModifyRightIndex(uint64_t index) { right_index_ += index; }

 private:
  uint64_t left_index_ = 0;
  uint64_t right_index_ = 0;
};

class ParsedListsMetaValue : public ParsedInternalValue {
 public:
  // Use this constructor after rocksdb::DB::Get();
  // 这个里面都是解码的函数和方法。 也就是从rocksdb中获取到数据之后，用于解码具体的value。然后获取数据。
  // TODO(DDD): 怎么存储到rocskdb中的？ 在存入的时候，执行Encode函数。
  explicit ParsedListsMetaValue(std::string* internal_value_str)
      : ParsedInternalValue(internal_value_str) {
    assert(internal_value_str->size() >= kListsMetaValueSuffixLength);
    if (internal_value_str->size() >= kListsMetaValueSuffixLength) {
      size_t offset = 0;
      type_ = static_cast<DataType>(static_cast<uint8_t>((*internal_value_str)[0]));
      offset += kTypeLength;
      user_value_ = rocksdb::Slice(internal_value_str->data() + kTypeLength,
                                   internal_value_str->size() - kListsMetaValueSuffixLength - kTypeLength);
      offset += user_value_.size();
      version_ = DecodeFixed64(internal_value_str->data() + offset);
      offset += kVersionLength;
      left_index_ = DecodeFixed64(internal_value_str->data() + offset);
      offset += kListValueIndexLength;
      right_index_ = DecodeFixed64(internal_value_str->data() + offset);
      offset += kListValueIndexLength;
      memcpy(reserve_, internal_value_str->data() + offset, sizeof(reserve_));
      offset += kSuffixReserveLength;
      ctime_ = DecodeFixed64(internal_value_str->data() + offset);
      offset += kTimestampLength;
      etime_ = DecodeFixed64(internal_value_str->data() + offset);
      offset += kTimestampLength;
    }
    count_ = DecodeFixed64(internal_value_str->data() + kTypeLength);
  }

  // Use this constructor in rocksdb::CompactionFilter::Filter();
  // 在合并法索的时候，解码，然后判断这条数据是否已经过期，就可以在压缩的时候，将数据丢掉。
  explicit ParsedListsMetaValue(const rocksdb::Slice& internal_value_slice)
      : ParsedInternalValue(internal_value_slice) {
    assert(internal_value_slice.size() >= kListsMetaValueSuffixLength);
    if (internal_value_slice.size() >= kListsMetaValueSuffixLength) {
      size_t offset = 0;
      type_ = static_cast<DataType>(static_cast<uint8_t>(internal_value_slice[0]));
      offset += kTypeLength;
      user_value_ = rocksdb::Slice(internal_value_slice.data() + kTypeLength,
                                   internal_value_slice.size() - kListsMetaValueSuffixLength - kTypeLength);
      offset += user_value_.size();
      version_ = DecodeFixed64(internal_value_slice.data() + offset);
      offset += kVersionLength;
      left_index_ = DecodeFixed64(internal_value_slice.data() + offset);
      offset += kListValueIndexLength;
      right_index_ = DecodeFixed64(internal_value_slice.data() + offset);
      offset += kListValueIndexLength;
      memcpy(reserve_, internal_value_slice.data() + offset, sizeof(reserve_));
      offset += kSuffixReserveLength;
      ctime_ = DecodeFixed64(internal_value_slice.data() + offset);
      offset += kTimestampLength;
      etime_ = DecodeFixed64(internal_value_slice.data() + offset);
      offset += kTimestampLength;
    }
    count_ = DecodeFixed64(internal_value_slice.data() + kTypeLength);
  }

  void StripSuffix() override {
    if (value_) {
      value_->erase(value_->size() - kListsMetaValueSuffixLength, kListsMetaValueSuffixLength);
    }
  }
  /* 
  
  这段代码中的 `SetVersionToValue` 函数是用来设置版本号的，它会将版本号编码到数据中。
  在这里， `version_`  是一个成员变量，而  `value_`  是一个指向数据的指针。
  在构造函数中设置了  `version_`  的值，但在  `SetVersionToValue`  
  函数中需要将该值编码到数据中，因此需要再次设置一遍。
  这样确保在每次调用  `SetVersionToValue`  函数时，都会使用最新的版本号来进行编码。

  在 UpdateVersion 有用到。 
  */
  void SetVersionToValue() override {
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kListsMetaValueSuffixLength;
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

  void SetIndexToValue() {
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kListsMetaValueSuffixLength + kVersionLength;
      EncodeFixed64(dst, left_index_);
      dst += sizeof(left_index_);
      EncodeFixed64(dst, right_index_);
    }
  }

  uint64_t InitialMetaValue() {
    this->SetCount(0);
    this->set_left_index(InitalLeftIndex);
    this->set_right_index(InitalRightIndex);
    this->SetEtime(0);
    this->SetCtime(0);
    return this->UpdateVersion();
  }

  bool IsValid() override {
    return !IsStale() && Count() != 0;
  }

  uint64_t Count() { return count_; }

  void SetCount(uint64_t count) {
    count_ = count;
    if (value_) {
      char* dst = const_cast<char*>(value_->data());
      EncodeFixed64(dst + kTypeLength, count_);
    }
  }

  void ModifyCount(uint64_t delta) {
    count_ += delta;
    if (value_) {
      char* dst = const_cast<char*>(value_->data());
      EncodeFixed64(dst + kTypeLength, count_);
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

  uint64_t LeftIndex() { return left_index_; }

  void set_left_index(uint64_t index) {
    left_index_ = index;
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kListsMetaValueSuffixLength + kVersionLength;
      EncodeFixed64(dst, left_index_);
    }
  }

  void ModifyLeftIndex(uint64_t index) {
    left_index_ -= index;
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kListsMetaValueSuffixLength + kVersionLength;
      EncodeFixed64(dst, left_index_);
    }
  }

  uint64_t RightIndex() { return right_index_; }

  void set_right_index(uint64_t index) {
    right_index_ = index;
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kListsMetaValueSuffixLength + kVersionLength + kListValueIndexLength;
      EncodeFixed64(dst, right_index_);
    }
  }

  void ModifyRightIndex(uint64_t index) {
    right_index_ += index;
    if (value_) {
      char* dst = const_cast<char*>(value_->data()) + value_->size() - kListsMetaValueSuffixLength + kVersionLength + kListValueIndexLength;
      EncodeFixed64(dst, right_index_);
    }
  }

private:
  const size_t kListsMetaValueSuffixLength = kVersionLength + 2 * kListValueIndexLength + kSuffixReserveLength + 2 * kTimestampLength;

 private:
  uint64_t count_ = 0;
  uint64_t left_index_ = 0;
  uint64_t right_index_ = 0;
};

}  //  namespace storage
#endif  //  SRC_LISTS_META_VALUE_FORMAT_H_
