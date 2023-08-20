#include "index/b_plus_tree.h"

#include <string>

#include "glog/logging.h"
#include "index/basic_comparator.h"
#include "index/generic_key.h"
#include "page/index_roots_page.h"

/**
 * TODO: Student Implement
 */
BPlusTree::BPlusTree(index_id_t index_id, BufferPoolManager *buffer_pool_manager, const KeyManager &KM,
                     int leaf_max_size, int internal_max_size)
    : index_id_(index_id),
      buffer_pool_manager_(buffer_pool_manager),
      processor_(KM),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {
  auto *page = buffer_pool_manager_->FetchPage(INDEX_ROOTS_PAGE_ID);
  if (page != nullptr){
    auto *header = reinterpret_cast<IndexRootsPage *>(page->GetData());
    if(!header->GetRootId(index_id,&root_page_id_)) root_page_id_ = INVALID_PAGE_ID;
    buffer_pool_manager_->UnpinPage(INDEX_ROOTS_PAGE_ID,false);
  }else root_page_id_ = INVALID_PAGE_ID;

  if(leaf_max_size_ == 0) // 好像不对。。。
    leaf_max_size_ = (PAGE_SIZE - LEAF_PAGE_HEADER_SIZE) / (processor_.GetKeySize()+sizeof(RowId));
  if(internal_max_size_ == 0)
    internal_max_size_ = (PAGE_SIZE - INTERNAL_PAGE_HEADER_SIZE) / (processor_.GetKeySize()+sizeof(page_id_t));
}
void BPlusTree::Destroy(page_id_t current_page_id) {
  if (IsEmpty()) {
    return;
  }
  queue<page_id_t> tree_queue;
  page_id_t tmp_page_id;
  tree_queue.push(root_page_id_);
  while (!tree_queue.empty()) {
    tmp_page_id = tree_queue.front();
    tree_queue.pop();
    Page *page = buffer_pool_manager_->FetchPage(tmp_page_id);
    BPlusTreePage *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
    if (!node->IsLeafPage()) {
      InternalPage *internal_node = reinterpret_cast<InternalPage *>(page->GetData());
      for (int i = 0; i < node->GetSize(); i++) {
        tree_queue.push(internal_node->ValueAt(i));
      }
    }
    buffer_pool_manager_->UnpinPage(tmp_page_id, false);
    buffer_pool_manager_->DeletePage(tmp_page_id);
  }
}
/*
 * Helper function to decide whether current b+tree is empty
 */
bool BPlusTree::IsEmpty() const {
  return root_page_id_ == INVALID_PAGE_ID;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
bool BPlusTree::GetValue(const GenericKey *key, std::vector<RowId> &result, Transaction *transaction) {
  Page *leaf_page = FindLeafPage(key);
  //assert(leaf_page != nullptr);
  if(leaf_page == nullptr) return false;
  LeafPage *leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  RowId value;
  bool is_exist = leaf_node->Lookup(key, value, processor_);  
  buffer_pool_manager_->UnpinPage(leaf_node->GetPageId(), false);

  if (!is_exist) return false;
  result.push_back(value);
  return true;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
bool BPlusTree::Insert(GenericKey *key, const RowId &value, Transaction *transaction) {
  if (IsEmpty()) 
  {
    StartNewTree(key, value);
    return true;
  }
  // 这一行是鼠么意思？
  //buffer_pool_manager_->UnpinPage(0, true);
  return InsertIntoLeaf(key, value, transaction);
}
/*
 * Insert constant key & value pair into an empty tree
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then update b+
 * tree's root page id and insert entry directly into leaf page.
 */
void BPlusTree::StartNewTree(GenericKey *key, const RowId &value) {
  page_id_t new_page_id = INVALID_PAGE_ID;
  Page *root_page = buffer_pool_manager_->NewPage(new_page_id);
  if (root_page == nullptr)
    throw std::runtime_error("out of memory.");
  root_page_id_ = new_page_id;
  UpdateRootPageId(1);
  LeafPage *root_node = reinterpret_cast<LeafPage *>(root_page->GetData());

  root_node->Init(root_page_id_, INVALID_PAGE_ID, processor_.GetKeySize(), leaf_max_size_);
  root_node->Insert(key, value, processor_);
  buffer_pool_manager_->UnpinPage(root_page_id_, true);
}

/*
 * Insert constant key & value pair into leaf page
 * User needs to first find the right leaf page as insertion target, then look
 * through leaf page to see whether insert key exist or not. If exist, return
 * immediately, otherwise insert entry. Remember to deal with split if necessary.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
bool BPlusTree::InsertIntoLeaf(GenericKey *key, const RowId &value, Transaction *transaction) {
  // 1. Find the leaf node to insert
  Page *leaf_page = FindLeafPage(key);
  if(leaf_page == nullptr) return false;
  LeafPage *leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  RowId v;
  // 2. Judge whether the key-value pair to be inserted already exists in the leaf node
  if (leaf_node->Lookup(key, v, processor_)) 
  {
    buffer_pool_manager_->UnpinPage(leaf_node->GetPageId(), false);
    LOG(WARNING) << "Key " << key << " already exists." << std::endl;
    return false;
  }
  // 3. Insert the key-value pair into leaf node
  int new_size = leaf_node->Insert(key, value, processor_);
  // 4. Judge whether the node size after insertion has exceeded the limit
  if (new_size >= leaf_node->GetMaxSize()) { // why GE?
    LeafPage *new_leaf_node = Split(leaf_node, transaction);
    InsertIntoParent(leaf_node, new_leaf_node->KeyAt(0), new_leaf_node, transaction);
    buffer_pool_manager_->UnpinPage(leaf_node->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(new_leaf_node->GetPageId(), true);
    return true;
  }
  buffer_pool_manager_->UnpinPage(leaf_node->GetPageId(), true);
  return true;
}

/*
 * Split input page and return newly created page.
 * Using template N to represent either internal page or leaf page.
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then move half
 * of key & value pairs from input page to newly created page
 */
BPlusTreeInternalPage *BPlusTree::Split(InternalPage *node, Transaction *transaction) {
  page_id_t new_page_id = INVALID_PAGE_ID;
  Page *new_page = buffer_pool_manager_->NewPage(new_page_id);
  if (new_page == nullptr) throw std::runtime_error("out of memory.");
  auto new_node = reinterpret_cast<InternalPage*>(new_page->GetData());
  new_node->Init(new_page_id, node->GetParentPageId(), processor_.GetKeySize(), internal_max_size_);
  node->MoveHalfTo(new_node, buffer_pool_manager_);
  return new_node;
}
BPlusTreeLeafPage *BPlusTree::Split(LeafPage *node, Transaction *transaction) {
  page_id_t new_page_id = INVALID_PAGE_ID;
  Page *new_page = buffer_pool_manager_->NewPage(new_page_id);
  if (new_page == nullptr) throw std::runtime_error("out of memory.");
  LeafPage *new_node = reinterpret_cast<LeafPage *>(new_page->GetData());
  new_node->Init(new_page_id, node->GetParentPageId(), processor_.GetKeySize(), leaf_max_size_);
  node->MoveHalfTo(new_node);
  new_node->SetNextPageId(node->GetNextPageId());
  node->SetNextPageId(new_node->GetPageId());
  return new_node;
}

/*
 * Insert key & value pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
void BPlusTree::InsertIntoParent(BPlusTreePage *old_node, GenericKey *key, BPlusTreePage *new_node,
                                 Transaction *transaction) {
  // 1. If old Node is the root node, generating a new root page
  if (old_node->IsRootPage()) 
  {
    page_id_t new_page_id = INVALID_PAGE_ID;
    Page *new_page = buffer_pool_manager_->NewPage(new_page_id);
    root_page_id_ = new_page_id;
    InternalPage *new_root_node = reinterpret_cast<InternalPage *>(new_page->GetData());
    new_root_node->Init(root_page_id_, INVALID_PAGE_ID, processor_.GetKeySize(), internal_max_size_);
    new_root_node->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());
    old_node->SetParentPageId(root_page_id_);
    new_node->SetParentPageId(root_page_id_);
    buffer_pool_manager_->UnpinPage(new_root_node->GetPageId(), true);
    UpdateRootPageId(0);
  }
  else
  {
    page_id_t parent_id = old_node->GetParentPageId();
    Page *parent_page = buffer_pool_manager_->FetchPage(parent_id);
    auto parent_node = reinterpret_cast<InternalPage*>(parent_page->GetData());
    parent_node->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
    // 4. Judge whether the node size after insertion has exceeded the limit
    if (parent_node->GetSize() >= parent_node->GetMaxSize()) { // why GE?
      InternalPage *split_node = Split(parent_node, transaction);
      InsertIntoParent(parent_node, split_node->KeyAt(0), split_node, transaction);
      buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(split_node->GetPageId(), true);
    }
    else
      buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
void BPlusTree::Remove(const GenericKey *key, Transaction *transaction) {
  if (IsEmpty()) {
    return;
  }
  Page *leaf_page = FindLeafPage(key);
  assert(leaf_page != nullptr);
  LeafPage *leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  leaf_node->RemoveAndDeleteRecord(key, processor_);
  if(CoalesceOrRedistribute(leaf_node, transaction) == false)
    buffer_pool_manager_->UnpinPage(leaf_node->GetPageId(), true);
}

/* todo
 * User needs to first find the sibling of input page. If sibling's size + input
 * page's size > page's max size, then redistribute. Otherwise, merge.
 * Using template N to represent either internal page or leaf page.
 * @return: true means target leaf page should be deleted, false means no
 * deletion happens
 */
template <typename N>
bool BPlusTree::CoalesceOrRedistribute(N *&node, Transaction *transaction) {
  // 1. If it is a root node, adjust the root node
  if (node->IsRootPage()) {
    return AdjustRoot(node);
  }
  // 2. If the node size is valid, No need to coalesce or redistribute the node
  if (node->GetSize() >= node->GetMinSize()) {
    return false;
  }
  // 3. Else, find the parent node first
  Page *parent_page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  InternalPage *parent_node = reinterpret_cast<InternalPage *>(parent_page->GetData());
  int index = parent_node->ValueIndex(node->GetPageId());
  // 4. Then find the neighbor nodes through parent node
  int neighbor_index = index - 1;
  if (index == 0) {
    neighbor_index = 1;
  }
  page_id_t neighbor_page_id = parent_node->ValueAt(neighbor_index);
  Page *neighbor_page = buffer_pool_manager_->FetchPage(neighbor_page_id);
  N *neighbor_node = reinterpret_cast<N *>(neighbor_page->GetData());
  // 5. If the sum size of neighbor nodes and current node >= the max size , redistribute the two nodes
  //    Else, coalesce the two nodes
  if (node->GetSize() + neighbor_node->GetSize() >= node->GetMaxSize()) {
    Redistribute(neighbor_node, node, index);
    buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(neighbor_node->GetPageId(), true);
    return false;
  } else {
    Coalesce(neighbor_node, node, parent_node, index, transaction);
    buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(neighbor_node->GetPageId(), true);
    return true;
  }
}

/*
 * Move all the key & value pairs from one page to its sibling page, and notify
 * buffer pool manager to delete this page. Parent page must be adjusted to
 * take info of deletion into account. Remember to deal with coalesce or
 * redistribute recursively if necessary.
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 * @param   parent             parent page of input "node"
 * @return  true means parent node should be deleted, false means no deletion happened
 */
bool BPlusTree::Coalesce(LeafPage *&neighbor_node, LeafPage *&node, InternalPage *&parent, int index,
                         Transaction *transaction) {
   // 1. If the current node is at the leftmost, swap it with the neighbor node
  if (index == 0) {
    std::swap(neighbor_node, node);
  }
  // 2. Then move all the entries of the current node to the neighbor node
  node->MoveAllTo(neighbor_node);
  neighbor_node->SetNextPageId(node->GetNextPageId());
  page_id_t leaf_node_id = node->GetPageId();
  buffer_pool_manager_->UnpinPage(leaf_node_id, false);
  buffer_pool_manager_->DeletePage(leaf_node_id);
  // 3. Delete the key value pair corresponding to the parent node
  parent->Remove(index == 0 ? 1 : index);
  // 4. Recursively determine whether the parent node needs to be coalesce or redistribute
  return CoalesceOrRedistribute(parent, transaction); 
}
bool BPlusTree::Coalesce(InternalPage *&neighbor_node, InternalPage *&node, InternalPage *&parent, int index,
                         Transaction *transaction) {
  // 1. If the current node is at the leftmost, swap it with the neighbor node
  if (index == 0) {
    std::swap(neighbor_node, node);
  }
  // 2. Then move all the entries of the current node to the neighbor node
  GenericKey* middle_key = parent->KeyAt(index == 0 ? 1 : index);
  node->MoveAllTo(neighbor_node, middle_key, buffer_pool_manager_);
  page_id_t internal_node_id = node->GetPageId();
  buffer_pool_manager_->UnpinPage(internal_node_id, false);
  buffer_pool_manager_->DeletePage(internal_node_id);
  // 3. Delete the key value pair corresponding to the parent node
  parent->Remove(index == 0 ? 1 : index);
  // 4. Recursively determine whether the parent node needs to be coalesce or redistribute
  return CoalesceOrRedistribute(parent, transaction);
}

/*
 * Redistribute key & value pairs from one page to its sibling page. If index ==
 * 0, move sibling page's first key & value pair into end of input "node",
 * otherwise move sibling page's last key & value pair into head of input
 * "node".
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 */
void BPlusTree::Redistribute(LeafPage *neighbor_node, LeafPage *node, int index) {
  Page *parent_page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  InternalPage *parent_node = reinterpret_cast<InternalPage *>(parent_page->GetData());
  if (index == 0) {
    neighbor_node->MoveFirstToEndOf(node);
    parent_node->SetKeyAt(1, neighbor_node->KeyAt(0));
  } else {
    neighbor_node->MoveLastToFrontOf(node);
    parent_node->SetKeyAt(index, node->KeyAt(0));
  }
  buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
}
void BPlusTree::Redistribute(InternalPage *neighbor_node, InternalPage *node, int index) {
  Page *parent_page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  InternalPage *parent_node = reinterpret_cast<InternalPage *>(parent_page->GetData());
  if (index == 0) {
    neighbor_node->MoveFirstToEndOf(node, parent_node->KeyAt(1), buffer_pool_manager_);
    parent_node->SetKeyAt(1, neighbor_node->KeyAt(0));
  } else {
    neighbor_node->MoveLastToFrontOf(node, parent_node->KeyAt(index), buffer_pool_manager_);
    parent_node->SetKeyAt(index, node->KeyAt(0));
  }
  buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
}
/*
 * Update root page if necessary
 * NOTE: size of root page can be less than min size and this method is only
 * called within coalesceOrRedistribute() method
 * case 1: when you delete the last element in root page, but root page still
 * has one last child
 * case 2: when you delete the last element in whole b+ tree
 * @return : true means root page should be deleted, false means no deletion
 * happened
 */
bool BPlusTree::AdjustRoot(BPlusTreePage *old_root_node) {
  // 1. When you delete the last element in root page, but root page still has one last child
  if (!old_root_node->IsLeafPage() && old_root_node->GetSize() == 1) {
    InternalPage *internal_node = reinterpret_cast<InternalPage *>(old_root_node);
    page_id_t new_root_id_ = internal_node->RemoveAndReturnOnlyChild();
    root_page_id_ = new_root_id_;
    UpdateRootPageId(0);
    Page *new_root_page = buffer_pool_manager_->FetchPage(root_page_id_);
    InternalPage *new_root_node = reinterpret_cast<InternalPage *>(new_root_page->GetData());
    new_root_node->SetParentPageId(INVALID_PAGE_ID);
    buffer_pool_manager_->UnpinPage(new_root_node->GetPageId(), true);
    return true;
  }
  // 2. When you delete the last element in whole b+ tree
  if (old_root_node->IsLeafPage() && old_root_node->GetSize() == 0) {
    root_page_id_ = INVALID_PAGE_ID;
    UpdateRootPageId(0);
    return true;
  }
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the left most leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
IndexIterator BPlusTree::Begin() {
  GenericKey* key;
  Page* first_leaf_page = FindLeafPage(key, INVALID_PAGE_ID, true);
  page_id_t page_id = first_leaf_page->GetPageId();
  buffer_pool_manager_->UnpinPage(first_leaf_page->GetPageId(), false);
  return IndexIterator(page_id, buffer_pool_manager_, 0);
}

/*
 * Input parameter is low-key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
IndexIterator BPlusTree::Begin(const GenericKey *key) {
  Page* leaf_page = FindLeafPage(key);
  LeafPage *leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  int index = leaf_node->KeyIndex(key, processor_);
  page_id_t page_id = leaf_node->GetPageId();
  buffer_pool_manager_->UnpinPage(leaf_node->GetPageId(), false);
  return IndexIterator(page_id, buffer_pool_manager_, index);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
IndexIterator BPlusTree::End() {
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  BPlusTreePage *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  while (!node->IsLeafPage()) {
    InternalPage *internal_node = reinterpret_cast<InternalPage *>(node);
    page_id_t chlid_page_id = internal_node->ValueAt(internal_node->GetSize() - 1);
    buffer_pool_manager_->UnpinPage(node->GetPageId(), false);
    page = buffer_pool_manager_->FetchPage(chlid_page_id);
    node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  }
  int last_index = node->GetSize() - 1; // 感觉这里不-1、、
  page_id_t page_id = page->GetPageId();
  buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
  return IndexIterator(page_id, buffer_pool_manager_, last_index);
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page
 * Note: the leaf page is pinned, you need to unpin it after use.
 */
Page *BPlusTree::FindLeafPage(const GenericKey *key, page_id_t page_id, bool leftMost) {
  if (IsEmpty()) return nullptr;
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  BPlusTreePage *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  while (!node->IsLeafPage()) {
    //std::cout << "2" << std::endl;
    InternalPage *i_node = reinterpret_cast<InternalPage *>(node);
    page_id_t child_node_page_id;
    if (leftMost) // always find left ignore key
      child_node_page_id = i_node->ValueAt(0);
    else
      child_node_page_id = i_node->Lookup(key, processor_);
    auto child_page = buffer_pool_manager_->FetchPage(child_node_page_id);
    auto child_node = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
    buffer_pool_manager_->UnpinPage(i_node->GetPageId(), false);
    page = child_page;
    node = child_node;
  }
  return page;
}

/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      default value is false. When set to true,
 * insert a record <index_name, current_page_id> into header page instead of
 * updating it.
 * 讲人话？
 */
void BPlusTree::UpdateRootPageId(int insert_record) {
  IndexRootsPage *index_root_page = reinterpret_cast<IndexRootsPage *>(buffer_pool_manager_->FetchPage(INDEX_ROOTS_PAGE_ID));
  if (insert_record != 0) // a new index
    index_root_page->Insert(index_id_, root_page_id_);
  else // existing index but update root
    index_root_page->Update(index_id_, root_page_id_);

  buffer_pool_manager_->UnpinPage(INDEX_ROOTS_PAGE_ID, true);
}

/**
 * This method is used for debug only, You don't need to modify
 */
void BPlusTree::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId()
        << ",Parent=" << leaf->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId()
        << ",Parent=" << inner->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 */
void BPlusTree::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
      bpm->UnpinPage(internal->ValueAt(i), false);
    }
  }
}

bool BPlusTree::Check() {
  bool all_unpinned = buffer_pool_manager_->CheckAllUnpinned();
  if (!all_unpinned) {
    LOG(ERROR) << "problem in page unpin" << endl;
  }
  return all_unpinned;
}