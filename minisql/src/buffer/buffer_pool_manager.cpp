#include "buffer/buffer_pool_manager.h"

#include "glog/logging.h"
#include "page/bitmap_page.h"

static const char EMPTY_PAGE_DATA[PAGE_SIZE] = {0};

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager) {
  pages_ = new Page[pool_size_];
  replacer_ = new LRUReplacer(pool_size_);
  for (size_t i = 0; i < pool_size_; i++) {
    free_list_.emplace_back(i);
  }
}

BufferPoolManager::~BufferPoolManager() {
  for (auto page : page_table_) {
    FlushPage(page.first);
  }
  delete[] pages_;
  delete replacer_;
}

/**
 * TODO: Student Implement
 */
Page *BufferPoolManager::FetchPage(page_id_t page_id) {
  // 1.     Search the page table for the requested page (P).
  // 1.1    If P exists, pin it and return it immediately.
  if(page_id == INVALID_PAGE_ID) return nullptr;
  if(page_table_.find(page_id) != page_table_.end())
  {
    replacer_->Pin(page_table_[page_id]);
    pages_[page_table_[page_id]].pin_count_++;
    return &pages_[page_table_[page_id]];
  }
  // 1.2    If P does not exist, find a replacement page (R) from either the free list or the replacer.
  //        Note that pages are always found from the free list first.
  else 
  {
    frame_id_t victim = 0;
    if(free_list_.size())
    {
      page_table_[page_id] = free_list_.front();
      victim = free_list_.front();
      free_list_.erase(free_list_.begin());
      disk_manager_->ReadPage(page_id, pages_[victim].GetData());
      pages_[victim].page_id_ = page_id;
      pages_[victim].pin_count_ = 1;
      pages_[victim].is_dirty_ = false;
      return &pages_[victim];
    }
    else
    {
      if(replacer_->Victim(&victim) == true)
      {
  // 2.     If R is dirty, write it back to the disk
        if(pages_[victim].is_dirty_ == true) FlushPage(pages_[victim].GetPageId());
  // 3.     Delete R from the page table and insert P.
        page_table_.erase(pages_[victim].GetPageId());
        page_table_[page_id] = victim;
  // 4.     Update P's metadata, read in the page content from disk, and then return a pointer to P.
        disk_manager_->ReadPage(page_id, pages_[victim].GetData());
        pages_[victim].page_id_ = page_id;
        pages_[victim].pin_count_ = 1;
        pages_[victim].is_dirty_ = false;
        return &pages_[victim];
      }
      else return nullptr;
    }
  }
}

/**
 * TODO: Student Implement
 */
Page *BufferPoolManager::NewPage(page_id_t &page_id) {
  // 0.   Make sure you call AllocatePage!
  // 1.   If all the pages in the buffer pool are pinned, return nullptr.
  if(free_list_.size() == 0 && replacer_->Size() == 0) return nullptr;
  // 2.   Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
  page_id_t newPage = AllocatePage();
  if(newPage == INVALID_PAGE_ID) return nullptr;
  frame_id_t newFrame = 0;
  if(free_list_.size())
  {
    newFrame = free_list_.front();
    free_list_.erase(free_list_.begin());
  }
  else
  {
    if(replacer_->Victim(&newFrame) == false) return nullptr;
    page_table_.erase(pages_[newFrame].GetPageId());
  }
  page_table_[newPage] = newFrame;
  // 3.   Update P's metadata, zero out memory and add P to the page table.
  pages_[newFrame].ResetMemory();
  pages_[newFrame].page_id_ = newPage;
  pages_[newFrame].pin_count_ = 1;
  pages_[newFrame].is_dirty_ = false;
  // 4.   Set the page ID output parameter. Return a pointer to P.
  page_id = newPage;
  return &pages_[newFrame];
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::DeletePage(page_id_t page_id) {
  // 0.   Make sure you call DeallocatePage!
  // 1.   Search the page table for the requested page (P).
  // 1.   If P does not exist, return true.
  if(page_table_.find(page_id) == page_table_.end()) return true;
  // 2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
  else if(pages_[page_table_[page_id]].pin_count_) return false;
  // 3.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.
  else
  {
    pages_[page_table_[page_id]].page_id_ = INVALID_PAGE_ID;
    if(pages_[page_table_[page_id]].IsDirty()) FlushPage(page_id);
    replacer_->Pin(page_table_[page_id]);
    free_list_.emplace_back(page_table_[page_id]);
    DeallocatePage(page_id);
    page_table_.erase(page_id);
  }
  return true;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
  if(page_table_.find(page_id) == page_table_.end()) return false;
  pages_[page_table_[page_id]].is_dirty_ = is_dirty;
  pages_[page_table_[page_id]].pin_count_ = max(0, pages_[page_table_[page_id]].pin_count_-1);
  if(pages_[page_table_[page_id]].pin_count_ == 0) replacer_->Unpin(page_table_[page_id]);
  return true;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::FlushPage(page_id_t page_id) {
  if(page_table_.find(page_id) == page_table_.end()) return false;
  disk_manager_->WritePage(page_id, pages_[page_table_[page_id]].GetData());
  pages_[page_table_[page_id]].is_dirty_ = false;
  return true;
}

page_id_t BufferPoolManager::AllocatePage() {
  int next_page_id = disk_manager_->AllocatePage();
  return next_page_id;
}

void BufferPoolManager::DeallocatePage(__attribute__((unused)) page_id_t page_id) {
  disk_manager_->DeAllocatePage(page_id);
}

bool BufferPoolManager::IsPageFree(page_id_t page_id) {
  return disk_manager_->IsPageFree(page_id);
}

// Only used for debug
bool BufferPoolManager::CheckAllUnpinned() {
  bool res = true;
  for (size_t i = 0; i < pool_size_; i++) {
    if (pages_[i].pin_count_ != 0) {
      res = false;
      LOG(ERROR) << "page " << pages_[i].page_id_ << " pin count:" << pages_[i].pin_count_ << endl;
    }
  }
  return res;
}