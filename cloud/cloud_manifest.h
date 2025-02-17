//  Copyright (c) 2016-present, Rockset, Inc.  All rights reserved.

#pragma once
#include <rocksdb/status.h>

#include <vector>

#include "db/log_reader.h"
#include "db/log_writer.h"
#include "port/port_posix.h"

namespace ROCKSDB_NAMESPACE {


// Cloud manifest holds the information about mapping between original file
// names and their suffixes.
// Each database runtime starts at a certain file number and ends at a different
// (bigger) file number. We call this runtime an epoch. All file numbers
// generated during that period have the same epoch ID (i.e. suffix). For
// example, if a database runtime creates file numbers from 10 to 16 with epoch
// [e1], CloudManifest will store this as epoch (10-16, [e1]) and will enable us
// to retrieve this file mapping. When RocksDB asks for 10.sst, we will
// internally map this to 10-[e1].sst.
// Here's an example:
// * Database starts, epoch ID [e1]
// * Files 1, 2 and 3 are created, all with [e1] suffix
// * Database restarts, epoch ID [e2]
// * No files are created
// * Database restarts epoch ID [e3]
// * File 4 is created
// * Database restarts
// In this case, we should expect to see files 1-[e1], 2-[e1], 3-[e1] and
// 4-[e2]. Files with same file number, but different suffix should be
// eliminated.
// CloudManifest is thread safe after Finalize() is called.
class CloudManifest {
 public:
  static Status LoadFromLog(std::unique_ptr<SequentialFileReader> log,
                            std::unique_ptr<CloudManifest>* manifest);
  // Creates CloudManifest for an empty database
  static Status CreateForEmptyDatabase(
      std::string currentEpoch, std::unique_ptr<CloudManifest>* manifest);

  std::unique_ptr<CloudManifest> clone() const;

  Status WriteToLog(std::unique_ptr<WritableFileWriter> log);

  // Add an epoch that starts with startFileNumber and is identified by epochId.
  // GetEpoch(startFileNumber) == epochId
  // Invalid call if finalized_ is false
  void AddEpoch(uint64_t startFileNumber, std::string epochId);

  std::string GetEpoch(uint64_t fileNumber);

  std::string GetCurrentEpoch();
  std::string ToString(bool include_past_epochs=false);

 private:
  CloudManifest(std::vector<std::pair<uint64_t, std::string>> pastEpochs,
                std::string currentEpoch)
      : pastEpochs_(std::move(pastEpochs)),
        currentEpoch_(std::move(currentEpoch)) {}

  mutable port::RWMutex mutex_;

  // sorted
  // a set of (fileNumber, epochId) where fileNumber is the last file number
  // (exclusive) of an epoch
  std::vector<std::pair<uint64_t, std::string>> pastEpochs_;
  std::string currentEpoch_;

  static constexpr uint32_t kCurrentFormatVersion = 1;
};

}  // namespace ROCKSDB_NAMESPACE
