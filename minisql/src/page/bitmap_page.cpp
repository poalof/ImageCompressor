#include "page/bitmap_page.h"

#include "glog/logging.h"

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::AllocatePage(uint32_t &page_offset) {
  if(page_allocated_ >= BitmapPage::GetMaxSupportedSize()) return false;
  // update meta data
  page_offset = next_free_page_;
  page_allocated_++;
  bytes[page_offset/8] ^= (0x1 << (7-page_offset%8));
  if(page_allocated_ == BitmapPage::GetMaxSupportedSize())
  {
    next_free_page_ = BitmapPage::GetMaxSupportedSize();
    return true;
  }
  // search for next free page from current pos to end
  for(int i = page_offset+1; i < BitmapPage::GetMaxSupportedSize(); i++)
    if(IsPageFree(i)) 
    {
      next_free_page_ = i;
      return true;
    }
  // search for next free page from head to current pos
  for(int i = 0; i < page_offset; i++)
    if(IsPageFree(i)) 
    {
      next_free_page_ = i;
      return true;
    }

}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::DeAllocatePage(uint32_t page_offset) {
  if(IsPageFree(page_offset)) return false;

  bytes[page_offset/8] ^= (0x1 << (7-page_offset%8));
  page_allocated_--;
  if(next_free_page_ > page_offset) next_free_page_ = page_offset;

  return IsPageFree(page_offset);
}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFree(uint32_t page_offset) const {
  // byte/bit both from high to low (or left to right)
  return IsPageFreeLow(page_offset/8, page_offset%8);
}
template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFreeLow(uint32_t byte_index, uint8_t bit_index) const {
  return (bytes[byte_index] & (0x1 << (7-bit_index))) == 0;
}

template class BitmapPage<64>;

template class BitmapPage<128>;

template class BitmapPage<256>;

template class BitmapPage<512>;

template class BitmapPage<1024>;

template class BitmapPage<2048>;

template class BitmapPage<4096>;