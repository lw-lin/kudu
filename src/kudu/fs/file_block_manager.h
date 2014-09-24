// Copyright (c) 2014, Cloudera, inc.

#ifndef KUDU_FS_FILE_BLOCK_MANAGER_H
#define KUDU_FS_FILE_BLOCK_MANAGER_H

#include <string>
#include <tr1/memory>
#include <tr1/unordered_set>
#include <vector>

#include "kudu/fs/block_id.h"
#include "kudu/fs/block_manager.h"
#include "kudu/util/locks.h"
#include "kudu/util/oid_generator.h"

namespace kudu {

class Env;
class RandomAccessFile;
class WritableFile;

namespace fs {

// A file-backed block storage implementation.
//
// This is a naive block implementation which maps each block to its own
// file on disk. To prevent the block directory from becoming too large,
// blocks are aggregated into a 3-level directory hierarchy.

// A file-backed block that has been opened for writing.
class FileWritableBlock : public WritableBlock {
 public:
  virtual ~FileWritableBlock();

  virtual Status Close() OVERRIDE;

  virtual const BlockId& id() const OVERRIDE;

  virtual Status Append(const Slice& data) OVERRIDE;

  virtual Status Sync() OVERRIDE;

  virtual Status FlushDataAsync() OVERRIDE;

  virtual size_t BytesAppended() const OVERRIDE;

 private:
  friend class FileBlockManager;

  FileWritableBlock(FileBlockManager* block_manager,
                    bool sync_on_close,
                    const BlockId& block_id,
                    const std::tr1::shared_ptr<WritableFile>& writer);

  // Synchronize this block's dirty metadata to disk.
  Status SyncMetadata();

  // Back pointer to the block manager.
  //
  // Should remain alive for the lifetime of this block.
  FileBlockManager* block_manager_;

  // Whether Sync() should be called during Close().
  const bool sync_on_close_;

  // The block's identifier.
  const BlockId block_id_;

  // The underlying opened file backing this block.
  std::tr1::shared_ptr<WritableFile> writer_;

  // Whether this block has been closed.
  bool closed_;

  // The number of bytes successfully appended to the block.
  size_t bytes_appended_;

  DISALLOW_COPY_AND_ASSIGN(FileWritableBlock);
};

// A file-backed block that has been opened for reading.
class FileReadableBlock : public ReadableBlock {
 public:
  virtual ~FileReadableBlock();

  virtual Status Close() OVERRIDE;

  virtual const BlockId& id() const OVERRIDE;

  virtual Status Size(size_t* sz) const OVERRIDE;

  virtual Status Read(uint64_t offset, size_t length,
                      Slice* result, uint8_t* scratch) const OVERRIDE;

 private:
  friend class FileBlockManager;

  FileReadableBlock(const BlockId& block_id,
                    const std::tr1::shared_ptr<RandomAccessFile>& reader);

  // The block's identifier.
  const BlockId block_id_;

  // The underlying opened file backing this block.
  std::tr1::shared_ptr<RandomAccessFile> reader_;

  // Whether this block has been closed.
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(FileReadableBlock);
};

// The file-backed block manager.
//
// This implementation does NOT use mmap-backed files when writing. That's
// because PosixMmapFile doesn't guarantee that the file has the right size
// following a Sync(). The file is only "right sized" during Close(). This
// violates the Block::Sync() contract.
class FileBlockManager : public BlockManager {
 public:

  // Creates a new in-memory instance of a FileBlockManager.
  //
  // 'env' should remain alive for the lifetime of the block manager.
  FileBlockManager(Env* env, const std::string& root_path);

  virtual ~FileBlockManager();

  virtual Status Create() OVERRIDE;

  virtual Status Open() OVERRIDE;

  virtual Status CreateAnonymousBlock(const CreateBlockOptions& opts,
                                      gscoped_ptr<WritableBlock>* block) OVERRIDE;

  virtual Status CreateAnonymousBlock(gscoped_ptr<WritableBlock>* block) OVERRIDE;

  virtual Status CreateNamedBlock(const CreateBlockOptions& opts,
                                  const BlockId& block_id,
                                  gscoped_ptr<WritableBlock>* block) OVERRIDE;

  virtual Status CreateNamedBlock(const BlockId& block_id,
                                  gscoped_ptr<WritableBlock>* block) OVERRIDE;

  virtual Status OpenBlock(const BlockId& block_id,
                           gscoped_ptr<ReadableBlock>* block) OVERRIDE;

  virtual Status DeleteBlock(const BlockId& block_id) OVERRIDE;

  virtual Status SyncBlocks(const std::vector<WritableBlock*>& blocks) OVERRIDE;

 private:
  friend class FileWritableBlock;

  // Creates the parent directory hierarchy for the block with the given id.
  Status CreateBlockDir(const BlockId& block_id);

  // Returns the path to a block with the given id.
  std::string GetBlockPath(const BlockId& block_id) const;

  // Creates a directory if it's not already present.
  Status CreateDirIfMissing(const std::string& path);

  // Creates a new block.
  void CreateBlock(const BlockId& block_id, const std::string& path,
                   const std::tr1::shared_ptr<WritableFile>& writer,
                   const CreateBlockOptions& opts,
                   gscoped_ptr<WritableBlock>* block);

  Env* env() const { return env_; }

  // For manipulating files.
  Env* env_;

  // Filesystem path where all block directories are found.
  const std::string root_path_;

  // For generating block IDs.
  ObjectIdGenerator oid_generator_;

  DISALLOW_COPY_AND_ASSIGN(FileBlockManager);
};

} // namespace fs
} // namespace kudu

#endif