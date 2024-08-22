// Copyright (c) 2015-present, Qihoo, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#ifndef PIKA_PKHASH_H_
#define PIKA_PKHASH_H_

#include "include/acl.h"
#include "include/pika_command.h"
#include "include/pika_db.h"
#include "storage/storage.h"

// class PKHDelCmd : public Cmd {
//  public:
//   PKHDelCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   //   void DoThroughDB() override;
//   //   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHDelCmd(*this); }

//  private:
//   std::string key_;
//   std::vector<std::string> fields_;
//   int32_t deleted_ = 0;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// // EHSET hash field value
// class PKHSetCmd : public Cmd {
//  public:
//   PKHSetCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHSetCmd(*this); }

//  private:
//   std::string key_, field_, value_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// // EHSETNX hash field value
// class PKHSetnxCmd : public Cmd {
//  public:
//   PKHSetnxCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHSetnxCmd(*this); }

//  private:
//   std::string key_, field_, value_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// // EHSETXX hash field value
// class PKHSetxxCmd : public Cmd {
//  public:
//   PKHSetxxCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHSetxxCmd(*this); }

//  private:
//   std::string key_, field_, value_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// // EHSETEX hash field value seconds
// class PKHSetexCmd : public Cmd {
//  public:
//   PKHSetexCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHSetexCmd(*this); }

//  private:
//   std::string key_, field_, value_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

class PKHExpireCmd : public Cmd {
 public:
  PKHExpireCmd(const std::string& name, int arity, uint32_t flag)
      : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
  std::vector<std::string> current_key() const override {
    std::vector<std::string> res;
    res.push_back(key_);
    return res;
  }
  void Do() override;
  void Split(const HintKeys& hint_keys) override {};
  void Merge() override {};
  Cmd* Clone() override { return new PKHExpireCmd(*this); }

 private:
  std::string key_;
  int64_t ttl_ = 0;
  int64_t numfields_ = 0;
  std::vector<std::string> fields_;

  rocksdb::Status s_;

  // std::string pattern_ = "*";
  // int64_t limit_ = 10;
  void DoInitial() override;
  void Clear() override {
    // pattern_ = "*";
    // limit_ = 10;
  }
};

class PKHExpireatCmd : public Cmd {
 public:
  PKHExpireatCmd(const std::string& name, int arity, uint32_t flag)
      : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
  std::vector<std::string> current_key() const override {
    std::vector<std::string> res;
    res.push_back(key_);
    return res;
  }
  void Do() override;
  void Split(const HintKeys& hint_keys) override {};
  void Merge() override {};
  Cmd* Clone() override { return new PKHExpireatCmd(*this); }

 private:
  std::string key_;
  int64_t ttl_ = 0;
  int64_t numfields_ = 0;
  std::vector<std::string> fields_;

  rocksdb::Status s_;

  // std::string pattern_ = "*";
  // int64_t limit_ = 10;
  void DoInitial() override;
  void Clear() override {
    // pattern_ = "*";
    // limit_ = 10;
  }
};
class PKHExpiretimeCmd : public Cmd {
 public:
  PKHExpiretimeCmd(const std::string& name, int arity, uint32_t flag)
      : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
  std::vector<std::string> current_key() const override {
    std::vector<std::string> res;
    res.push_back(key_);
    return res;
  }
  void Do() override;
  void Split(const HintKeys& hint_keys) override {};
  void Merge() override {};
  Cmd* Clone() override { return new PKHExpiretimeCmd(*this); }

 private:
  std::string key_;
  int64_t ttl_ = 0;
  int64_t numfields_ = 0;
  std::vector<std::string> fields_;

  rocksdb::Status s_;

  // std::string pattern_ = "*";
  // int64_t limit_ = 10;
  void DoInitial() override;
  void Clear() override {
    // pattern_ = "*";
    // limit_ = 10;
  }
};
// class PKHPExpireCmd : public Cmd {
//  public:
//   PKHPExpireCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHPExpireCmd(*this); }

//  private:
//   std::string key_;
//   int64_t ttl_ = 0;
//   int64_t numfields_ = 0;
//   std::vector<std::string> fields_;

//   rocksdb::Status s_;

//   // std::string pattern_ = "*";
//   // int64_t limit_ = 10;
//   void DoInitial() override;
//   void Clear() override {
//     // pattern_ = "*";
//     // limit_ = 10;
//   }
// };

// class PKHPExpireatCmd : public Cmd {
//  public:
//   PKHPExpireatCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHPExpireatCmd(*this); }

//  private:
//   std::string key_;
//   int64_t ttl_ = 0;
//   int64_t numfields_ = 0;
//   std::vector<std::string> fields_;

//   rocksdb::Status s_;

//   // std::string pattern_ = "*";
//   // int64_t limit_ = 10;
//   void DoInitial() override;
//   void Clear() override {
//     // pattern_ = "*";
//     // limit_ = 10;
//   }
// };

// class PKHExpiretimeCmd : public Cmd {
//  public:
//   PKHExpiretimeCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHExpiretimeCmd(*this); }

//  private:
//   std::string key_;
//   int64_t ttl_ = 0;
//   int64_t numfields_ = 0;
//   std::vector<std::string> fields_;

//   rocksdb::Status s_;

//   // std::string pattern_ = "*";
//   // int64_t limit_ = 10;
//   void DoInitial() override;
//   void Clear() override {
//     // pattern_ = "*";
//     // limit_ = 10;
//   }
// };

class PKHPersistCmd : public Cmd {
 public:
  PKHPersistCmd(const std::string& name, int arity, uint32_t flag)
      : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
  std::vector<std::string> current_key() const override {
    std::vector<std::string> res;
    res.push_back(key_);
    return res;
  }
  void Do() override;
  void Split(const HintKeys& hint_keys) override {};
  void Merge() override {};
  Cmd* Clone() override { return new PKHPersistCmd(*this); }

 private:
  std::string key_;
  int64_t ttl_ = 0;
  int64_t numfields_ = 0;
  std::vector<std::string> fields_;

  rocksdb::Status s_;

  // std::string pattern_ = "*";
  // int64_t limit_ = 10;
  void DoInitial() override;
  void Clear() override {
    // pattern_ = "*";
    // limit_ = 10;
  }
};

class PKHTTLCmd : public Cmd {
 public:
  PKHTTLCmd(const std::string& name, int arity, uint32_t flag)
      : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
  std::vector<std::string> current_key() const override {
    std::vector<std::string> res;
    res.push_back(key_);
    return res;
  }
  void Do() override;
  void Split(const HintKeys& hint_keys) override {};
  void Merge() override {};
  Cmd* Clone() override { return new PKHTTLCmd(*this); }

 private:
  std::string key_;
  int64_t ttl_ = 0;
  int64_t numfields_ = 0;
  std::vector<std::string> fields_;

  rocksdb::Status s_;

  // std::string pattern_ = "*";
  // int64_t limit_ = 10;
  void DoInitial() override;
  void Clear() override {
    // pattern_ = "*";
    // limit_ = 10;
  }
};

// class PKHPTTLCmd : public Cmd {
//  public:
//   PKHPTTLCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHPTTLCmd(*this); }

//  private:
//   std::string key_;
//   int64_t ttl_ = 0;
//   int64_t numfields_ = 0;
//   std::vector<std::string> fields_;

//   rocksdb::Status s_;

//   // std::string pattern_ = "*";
//   // int64_t limit_ = 10;
//   void DoInitial() override;
//   void Clear() override {
//     // pattern_ = "*";
//     // limit_ = 10;
//   }
// };

class PKHGetCmd : public Cmd {
 public:
  PKHGetCmd(const std::string& name, int arity, uint32_t flag)
      : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
  std::vector<std::string> current_key() const override {
    std::vector<std::string> res;
    res.push_back(key_);
    return res;
  }
  void Do() override;
  void ReadCache() override;
  void DoThroughDB() override;
  void DoUpdateCache() override;
  void Split(const HintKeys& hint_keys) override {};
  void Merge() override {};
  Cmd* Clone() override { return new PKHGetCmd(*this); }

 private:
  std::string key_, field_;
  void DoInitial() override;
  rocksdb::Status s_;
};

class PKHSetCmd : public Cmd {
 public:
  PKHSetCmd(const std::string& name, int arity, uint32_t flag)
      : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
  std::vector<std::string> current_key() const override {
    std::vector<std::string> res;
    res.push_back(key_);
    return res;
  }
  void Do() override;
  void DoThroughDB() override;
  void DoUpdateCache() override;
  void Split(const HintKeys& hint_keys) override {};
  void Merge() override {};
  Cmd* Clone() override { return new PKHSetCmd(*this); }

 private:
  // 每个命令的参数组成不同。
  std::string key_, field_, value_;
  void DoInitial() override;
  rocksdb::Status s_;
};

// class PKHGetallCmd : public Cmd {
//  public:
//   PKHGetallCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void ReadCache() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHGetallCmd(*this); }

//  private:
//   std::string key_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHExistsCmd : public Cmd {
//  public:
//   PKHExistsCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void ReadCache() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHExistsCmd(*this); }

//  private:
//   std::string key_, field_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHIncrbyCmd : public Cmd {
//  public:
//   PKHIncrbyCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHIncrbyCmd(*this); }

//  private:
//   std::string key_, field_;
//   int64_t by_ = 0;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHIncrbyfloatCmd : public Cmd {
//  public:
//   PKHIncrbyfloatCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHIncrbyfloatCmd(*this); }

//  private:
//   std::string key_, field_, by_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHKeysCmd : public Cmd {
//  public:
//   PKHKeysCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void ReadCache() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHKeysCmd(*this); }

//  private:
//   std::string key_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHLenCmd : public Cmd {
//  public:
//   PKHLenCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void ReadCache() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHLenCmd(*this); }

//  private:
//   std::string key_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHMgetCmd : public Cmd {
//  public:
//   PKHMgetCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void ReadCache() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHMgetCmd(*this); }

//  private:
//   std::string key_;
//   std::vector<std::string> fields_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHMsetCmd : public Cmd {
//  public:
//   PKHMsetCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHMsetCmd(*this); }

//  private:
//   std::string key_;
//   std::vector<storage::FieldValue> fvs_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHMsetnxCmd : public Cmd {
//  public:
//   PKHMsetnxCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHMsetnxCmd(*this); }

//  private:
//   std::string key_;
//   std::vector<storage::FieldValue> fvs_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHStrlenCmd : public Cmd {
//  public:
//   PKHStrlenCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void ReadCache() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHStrlenCmd(*this); }

//  private:
//   std::string key_, field_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHValsCmd : public Cmd {
//  public:
//   PKHValsCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)) {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void ReadCache() override;
//   void DoThroughDB() override;
//   void DoUpdateCache() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHValsCmd(*this); }

//  private:
//   std::string key_, field_;
//   void DoInitial() override;
//   rocksdb::Status s_;
// };

// class PKHScanCmd : public Cmd {
//  public:
//   PKHScanCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)), pattern_("*") {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHScanCmd(*this); }

//  private:
//   std::string key_;
//   std::string pattern_;
//   int64_t cursor_;
//   int64_t count_{10};
//   void DoInitial() override;
//   void Clear() override {
//     pattern_ = "*";
//     count_ = 10;
//   }
// };

// class PKHScanxCmd : public Cmd {
//  public:
//   PKHScanxCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)), pattern_("*") {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKHScanxCmd(*this); }

//  private:
//   std::string key_;
//   std::string start_field_;
//   std::string pattern_;
//   int64_t count_{10};
//   void DoInitial() override;
//   void Clear() override {
//     pattern_ = "*";
//     count_ = 10;
//   }
// };

// class PKEHScanRangeCmd : public Cmd {
//  public:
//   PKEHScanRangeCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)), pattern_("*") {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKEHScanRangeCmd(*this); }

//  private:
//   std::string key_;
//   std::string field_start_;
//   std::string field_end_;
//   std::string pattern_;
//   int64_t limit_ = 10;
//   void DoInitial() override;
//   void Clear() override {
//     pattern_ = "*";
//     limit_ = 10;
//   }
// };

// class PKEHRScanRangeCmd : public Cmd {
//  public:
//   PKEHRScanRangeCmd(const std::string& name, int arity, uint32_t flag)
//       : Cmd(name, arity, flag, static_cast<uint32_t>(AclCategory::PKHASH)), pattern_("*") {}
//   std::vector<std::string> current_key() const override {
//     std::vector<std::string> res;
//     res.push_back(key_);
//     return res;
//   }
//   void Do() override;
//   void Split(const HintKeys& hint_keys) override {};
//   void Merge() override {};
//   Cmd* Clone() override { return new PKEHRScanRangeCmd(*this); }

//  private:
//   std::string key_;
//   std::string field_start_;
//   std::string field_end_;
//   std::string pattern_ = "*";
//   int64_t limit_ = 10;
//   void DoInitial() override;
//   void Clear() override {
//     pattern_ = "*";
//     limit_ = 10;
//   }
// };
#endif
