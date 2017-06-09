// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/CheatingNew/MemoryRanges.h"

#include <iterator>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "DolphinWX/CheatingNew/Utils.h"

namespace Cheats
{
// The ranges must be sorted, non-overlapping and each range in the vector must be non-empty.
MemoryRangesIterator::MemoryRangesIterator(const MemoryRanges::iterator& range,
                                           const std::shared_ptr<MemoryRanges>& ranges,
                                           u32 alignment)
    : m_ranges(ranges), m_range_iter(range), m_alignment(alignment)
{
  // TODO: prefix with _dbg
  _assert_msg_(ACTIONREPLAY, alignment != 0 && (alignment & (alignment - 1)) == 0,
               "invalid alignment");

  FindNonEmptyRange();
}

MemoryRangesIterator& MemoryRangesIterator::operator++()
{
  // TODO: prefix with _dbg
  _assert_msg_(ACTIONREPLAY, m_range_iter != m_ranges->end(),
               "must not iterate beyond the last range");

  u32 next_address = m_address + m_alignment;
  if (next_address <= m_upper_bound && next_address > m_address)
  {
    m_address = next_address;
  }
  else
  {
    ++m_range_iter;
    FindNonEmptyRange();
  }

  return *this;
}

MemoryRangesIterator MemoryRangesIterator::operator++(int)
{
  MemoryRangesIterator ret = *this;
  ++(*this);
  return ret;
}

bool MemoryRangesIterator::operator==(MemoryRangesIterator other)
{
  // TODO: prefix with _dbg
  _assert_msg_(ACTIONREPLAY, m_ranges.get() == other.m_ranges.get(),
               "iterators of different memory ranges cannot be compared");

  if (m_range_iter != other.m_range_iter)
    return false;

  return m_address == other.m_address;
}

bool MemoryRangesIterator::operator!=(MemoryRangesIterator other)
{
  return !(*this == other);
}

SearchResult MemoryRangesIterator::operator*()
{
  return SearchResult{m_address, UnspecifiedItem, UnspecifiedItem};
}

MemoryRangesIterator::difference_type MemoryRangesIterator::
operator-(const MemoryRangesIterator& other)
{
  // TODO: prefix with _dbg
  _assert_msg_(ACTIONREPLAY, m_ranges.get() == other.m_ranges.get(),
               "iterators of different memory ranges cannot be subtracted");

  // TODO: prefix with _dbg
  _assert_msg_(ACTIONREPLAY, m_alignment == other.m_alignment,
               "iterators with different alignment cannot be subtracted");

  if (m_range_iter == other.m_range_iter)
    return (m_range_iter == m_ranges->end()) ? 0 : (m_address - other.m_address) / m_alignment;

  MemoryRangesIterator::difference_type diff = 0;

  if (other.m_range_iter != other.m_ranges->end())
  {
    diff += (other.m_upper_bound - other.m_address) / m_alignment + 1;
    for (auto it = other.m_range_iter + 1; it != other.m_ranges->end(); ++it)
    {
      u32 lo = (it->first + (m_alignment - 1)) & -m_alignment;
      if (it == m_range_iter)
      {
        return diff + (m_address - lo) / m_alignment;
      }

      u32 hi = it->second & -m_alignment;
      if (lo <= hi && lo >= it->first)
      {
        diff += (hi - lo) / m_alignment + 1;
      }
    }
  }

  if (m_range_iter != m_ranges->end())
  {
    diff -= (m_upper_bound - m_address) / m_alignment + 1;
    for (auto it = m_range_iter + 1; it != m_ranges->end(); ++it)
    {
      u32 lo = (it->first + (m_alignment - 1)) & -m_alignment;
      u32 hi = it->second & -m_alignment;
      if (lo <= hi && lo >= it->first)
      {
        diff -= (hi - lo) / m_alignment + 1;
      }
    }
  }

  return diff;
}

void MemoryRangesIterator::FindNonEmptyRange()
{
  while (true)
  {
    if (m_range_iter == m_ranges->end())
    {
      m_address = 0x0U;
      m_upper_bound = 0x0U;
      return;
    }

    u32 next_address = (m_range_iter->first + (m_alignment - 1)) & -m_alignment;
    u32 next_upper_bound = m_range_iter->second & -m_alignment;

    if (next_address <= next_upper_bound && next_address >= m_range_iter->first)
    {
      m_address = next_address;
      m_upper_bound = next_upper_bound;
      return;
    }

    ++m_range_iter;
  }
}

}  // namespace Cheats
