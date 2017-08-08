//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#pragma once

#include <mutex>
#include <condition_variable>

#include "rocksdb/async/callables.h"
#include "rocksdb/cache.h"

namespace rocksdb {

class InternalIterator;
class TableReader;

namespace async {

// This class is a helper base which helps
// to convert the async operation into sync
class AsyncAbsorber {
public:

  AsyncAbsorber(const AsyncAbsorber&) = delete;
  AsyncAbsorber& operator=(const AsyncAbsorber&) = delete;
  // Wait until the callback is absorbed
  void Wait() const {
    std::unique_lock<std::mutex> l(m_);
    while (!signalled_) {
      cvar_.wait(l);
    }
  }
  void Reset() {
    signalled_ = false;
  }

protected:
  AsyncAbsorber() : signalled_(false) {
  }
  ~AsyncAbsorber() {
  }
  void Notify() {
    std::unique_lock<std::mutex> l(m_);
    signalled_ = true;
    l.unlock();
    cvar_.notify_one();
  }
private:
  bool                   signalled_;
  mutable std::mutex     m_;
  mutable std::condition_variable cvar_;
};

// This class allow to make
// an async TableCache::FindTable() call
// in a sync manner
class FindTableSyncer : public async::AsyncAbsorber {
public:

  FindTableSyncer() {}

  Cache::Handle* GetResult() const {
    return result_;
  }

  async::Callable<Status, const Status&, Cache::Handle*>
    GetCallback() {
    using namespace async;
    CallableFactory<FindTableSyncer, Status, const Status&, Cache::Handle*> f(this);
    return f.GetCallable<&FindTableSyncer::OnFindTable>();
  }

private:

  Status OnFindTable(const Status& s, Cache::Handle* handle) {
    if (s.ok()) {
      result_ = handle;
    }
    Notify();
    return s;
  }
  Cache::Handle*         result_;
};

class NewIteratorSyncer : public async::AsyncAbsorber {
public:
  NewIteratorSyncer() : result_(nullptr) {
  }
  InternalIterator* GetResult() const {
    return result_;
  }
  async::Callable<Status, const Status&, InternalIterator*, TableReader*>
    GetCallable() {
    async::CallableFactory<NewIteratorSyncer, Status, const Status&, InternalIterator*, TableReader*>
      f(this);
    return f.GetCallable<&NewIteratorSyncer::OnNewIterator>();
  }
private:
  Status OnNewIterator(const Status& s, InternalIterator* iter,
    TableReader* table_reader) {
    result_ = iter;
    Notify();
    return s;
  }
  InternalIterator* result_;
};

}
}