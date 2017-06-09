// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <iterator>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"

namespace Cheats
{
typedef std::vector<std::pair<u32, u32>> MemoryRanges;

struct SearchResult;

class MemoryRangesIterator : public std::iterator<std::forward_iterator_tag, SearchResult>
{
public:
  // The ranges must be sorted, non-overlapping and each range in the vector must be non-empty.
  explicit MemoryRangesIterator(const MemoryRanges::iterator& range,
                                const std::shared_ptr<MemoryRanges>& ranges, u32 alignment = 1);

  MemoryRangesIterator& operator++();
  MemoryRangesIterator operator++(int);

  bool operator==(MemoryRangesIterator other);
  bool operator!=(MemoryRangesIterator other);

  SearchResult operator*();

  MemoryRangesIterator::difference_type operator-(const MemoryRangesIterator& other);

private:
  void FindNonEmptyRange();

  std::shared_ptr<MemoryRanges> m_ranges;
  MemoryRanges::iterator m_range_iter;
  u32 m_alignment;
  u32 m_address;
  u32 m_upper_bound;
};

}  // namespace Cheats
