// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/CheatingNew/Finder.h"

#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "Core/HW/Memmap.h"

#include "DolphinWX/CheatingNew/MemoryRanges.h"
#include "DolphinWX/CheatingNew/Utils.h"

namespace Cheats
{
enum
{
  PHYSICAL_SCAN_BASE = 0x0U,
  LOGICAL_SCAN_BASE = 0x80000000U,
  REALRAM_OFFSET = 0x0U,
  EXRAM_OFFSET = 0x10000000U,
};

// clang-format off
const u32 CANDIDATE_MEMORY_RANGES[][4] = {
    {PHYSICAL_SCAN_BASE + REALRAM_OFFSET, Memory::REALRAM_SIZE},
    {PHYSICAL_SCAN_BASE + EXRAM_OFFSET,   Memory::EXRAM_SIZE},
    {LOGICAL_SCAN_BASE  + REALRAM_OFFSET, Memory::REALRAM_SIZE},
    {LOGICAL_SCAN_BASE  + EXRAM_OFFSET,   Memory::EXRAM_SIZE},
};
// clang-format on

Finder::Finder(const OptionalNewResultsCallback& new_results_callback)
    : m_primary_new_results_callback(new_results_callback)
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): creating", __FUNCTION__);
}

Finder::~Finder()
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): destructing", __FUNCTION__);

  CancelSearch();
  JoinSearchThread();
}

Finder::Status Finder::SearchItem(const MemoryItem& item,
                                  const OptionalSearchProgressCallback& search_progress_callback)
{
  NewResults();  // Fetch pending results if our caller neglected to do it.

  std::unique_lock<std::recursive_mutex> thread_lock(m_thread_mutex, std::defer_lock);
  std::shared_lock<std::shared_mutex> search_results_lock(m_search_results_mutex, std::defer_lock);
  std::lock(thread_lock, search_results_lock);

  if (!IsValidMemoryItem(item))
    return Status::ERROR_INVALID_VALUE;

  if (!m_search_results.empty() && GetMemoryItemType(item) != m_search_value_type)
    return Status::ERROR_MISMATCHED_VALUE_TYPE;

  if (!Memory::IsInitialized())
    return Status::ERROR_MEMORY_NOT_INITIALIZED;

  std::function<void(MemoryItemType type, const Predicate, const OptionalSearchProgressCallback,
                     std::promise<void>, std::promise<MaybeResultContainer>)>
      filter_fn;

  if (m_search_results.empty())
  {
    MemorySize size = GetMemoryItemSize(item);
    if (size == 0)
      return Status::ERROR_UNKNOWN_SIZE_OF_VALUE;

    auto valid_ranges = std::make_shared<MemoryRanges>();

    for (const auto& range : CANDIDATE_MEMORY_RANGES)
    {
      if (range[1] >= size && IsAddressRangeValid(range[0], range[1], AddressTranslation::DATA))
      {
        valid_ranges->emplace_back(range[0], range[0] + (range[1] - size));
        DEBUG_LOG(ACTIONREPLAY, "%s(): valid range: %X - %X", __FUNCTION__, range[0],
                  range[0] + (range[1] - size));
      }
    }

    if (valid_ranges->empty())
      return Status::ERROR_NO_VALID_MEMORY_RANGES;

    MemorySize alignment = GetMemoryItemAlignment(item);

    using namespace std::placeholders;
    filter_fn = std::bind<void>(
        &Finder::DoSearch<MemoryRangesIterator>, this,
        MemoryRangesIterator(valid_ranges->begin(), valid_ranges, alignment),
        MemoryRangesIterator(valid_ranges->end(), valid_ranges, alignment), _1, _2, _3, _4, _5);
  }
  else
  {
    using namespace std::placeholders;
    filter_fn =
        std::bind<void>(&Finder::DoSearch<decltype(m_search_results)::iterator>, this,
                        m_search_results.begin(), m_search_results.end(), _1, _2, _3, _4, _5);
  }

  if (m_future_results.valid() || m_search_thread.joinable())
    return Status::ERROR_SEARCH_IN_PROGRESS;

  std::promise<void> promised_shared_lock;
  std::future<void> future_shared_lock(promised_shared_lock.get_future());

  std::promise<MaybeResultContainer> promised_results;
  m_future_results = promised_results.get_future();

  m_search_value_type = GetMemoryItemType(item);
  m_search_thread =
      std::thread(filter_fn, m_search_value_type, MakePredicate(item), search_progress_callback,
                  std::move(promised_shared_lock), std::move(promised_results));

  future_shared_lock.get();  // Wait for the thread to acquire the lock.

  return Status::OK;
}

bool Finder::NewResults()
{
  std::unique_lock<std::recursive_mutex> thread_lock(m_thread_mutex);
  // Nesting locks (see search_results_lock below) can be dangerous, risking a deadlock.
  // However, locking the search results here could get the main thread stuck waiting for
  // the search thread to complete.
  // But as long as we don't nest both locks the other way round somewhere else, we'll be fine.

  if (!m_future_results.valid())
    return false;

  // A valid future can mean
  // (1) that the thread is still running and hasn't produced a result yet,
  // (2) that the thread is still running and has already produced a result (and will finish soon)
  // (3) or that the thread finished and produced a result, but hasn't been joined yet.
  // In any case it means that the thread is joinable.

  if (m_future_results.wait_for(std::chrono::milliseconds{100}) != std::future_status::ready)
    return false;

  auto maybe_results = m_future_results.get();
  if (maybe_results)
  {
    std::unique_lock<std::shared_mutex> search_results_lock(m_search_results_mutex);
    m_search_results = std::move(*maybe_results);
  }

  JoinSearchThread();
  return static_cast<bool>(maybe_results);
}

void Finder::JoinSearchThread()
{
  // Guard both m_search_thread and writing m_cancel_search.
  std::unique_lock<std::recursive_mutex> thread_lock(m_thread_mutex);

  if (!m_search_thread.joinable())
    return;

  if (std::this_thread::get_id() == m_search_thread.get_id())
  {
    ERROR_LOG(ACTIONREPLAY, "%s(): cannot join thread with itself", __FUNCTION__);
    return;
  }

  m_search_thread.join();
  m_cancel_search.store(false);
}

void Finder::CancelSearch()
{
  // Guard both m_search_thread and writing m_cancel_search.
  std::unique_lock<std::recursive_mutex> thread_lock(m_thread_mutex);

  if (!m_search_thread.joinable())
    return;

  m_cancel_search.store(true);
}

void Finder::ClearResults()
{
  std::unique_lock<std::recursive_mutex> thread_lock(m_thread_mutex);

  CancelSearch();

  std::promise<MaybeResultContainer> promised_results;
  m_future_results = promised_results.get_future();
  promised_results.set_value(ResultContainer());

  thread_lock.unlock();

  InvokeCallbacks();
}

std::size_t Finder::GetResultCount() const
{
  std::shared_lock<std::shared_mutex> search_results_lock(m_search_results_mutex);

  return m_search_results.size();
}

MaybeAddress Finder::GetAddress(std::size_t idx) const
{
  std::shared_lock<std::shared_mutex> search_results_lock(m_search_results_mutex);

  if (idx >= m_search_results.size())
    return std::nullopt;

  return m_search_results[idx].address;
}

MemoryItemType Finder::GetItemType(std::size_t idx) const
{
  return m_search_value_type;
}

void Finder::UpdateItem(SearchResult& result)
{
  result.current = PeekMemoryItem(result.address, m_search_value_type, AddressTranslation::DATA);
}

MemoryItem Finder::GetCurrentItem(std::size_t idx)
{
  std::shared_lock<std::shared_mutex> search_results_lock(m_search_results_mutex);

  if (idx >= m_search_results.size())
    return UnspecifiedItem;

  UpdateItem(m_search_results[idx]);
  return m_search_results[idx].current;
}

MemoryItem Finder::GetPreviousItem(std::size_t idx)
{
  // TODO: implement properly
  std::shared_lock<std::shared_mutex> search_results_lock(m_search_results_mutex);

  if (idx >= m_search_results.size())
    return UnspecifiedItem;

  UpdateItem(m_search_results[idx]);
  return m_search_results[idx].previous;
}

int Finder::RegisterCallback(NewResultsCallback new_results_callback)
{
  if (!new_results_callback)
  {
    ERROR_LOG(ACTIONREPLAY, "%s(): called with invalid callback", __FUNCTION__);
    return -1;
  }

  m_extra_new_results_callbacks.emplace_front(++m_recent_callback_id, new_results_callback);
  return m_recent_callback_id;
}

void Finder::UnregisterCallback(int callback_id)
{
  if (callback_id < 0)
  {
    ERROR_LOG(ACTIONREPLAY, "%s(): called with invalid callback ID", __FUNCTION__);
    return;
  }

  CallbackCollection::const_iterator it;

  for (it = m_extra_new_results_callbacks.cbegin(); it != m_extra_new_results_callbacks.cend();
       ++it)
  {
    if (it->first == callback_id)
      break;
  }

  if (it == m_extra_new_results_callbacks.cend())
  {
    ERROR_LOG(ACTIONREPLAY, "%s(): callback ID not found", __FUNCTION__);
    return;
  }

  m_extra_new_results_callbacks.erase(it);
}

template <typename Iter>
void Finder::DoSearch(Iter start, Iter stop, MemoryItemType type, const Predicate pred,
                      const OptionalSearchProgressCallback search_progress_callback,
                      std::promise<void> promised_shared_lock,
                      std::promise<MaybeResultContainer> promised_results)
{
  std::shared_lock<std::shared_mutex> search_results_lock(m_search_results_mutex);
  promised_shared_lock.set_value();  // Notify calling thread that we acquired the lock.

  int percent = 0;
  typename Iter::difference_type progress = 0;
  typename Iter::difference_type candidates = stop - start;
  typename Iter::difference_type next_update = (search_progress_callback) ? 0 : -1;

  // TODO: prefix with _dbg
  _assert_msg_(ACTIONREPLAY, candidates >= 0, "number of candidates must be non-negative");

  ResultContainer new_search_results;

  while (start != stop)
  {
    if (progress++ == next_update)
    {
      if (m_cancel_search)
        break;

      if (search_progress_callback)
      {
        (*search_progress_callback)(percent);
      }

      std::this_thread::yield();

      if (m_cancel_search)
        break;

      percent = (progress * 100 + (candidates - 1)) / candidates;
      next_update = (percent * candidates) / 100;
    }

    const SearchResult old_result = *start++;

    MemoryItem item = PeekMemoryItem(old_result.address, type, AddressTranslation::DATA);
    if (!IsValidMemoryItem(item))
      continue;

    if (!pred(item))
      continue;

    new_search_results.push_back({old_result.address, item, old_result.current});
  }

  search_results_lock.unlock();  // Better unlock now as we don't know what the callbacks will do.

  bool cancelled = m_cancel_search;

  if (search_progress_callback)
  {
    (*search_progress_callback)(cancelled ? 0 : 100);
  }

  if (cancelled)
  {
    promised_results.set_value(std::nullopt);
  }
  else
  {
    // TODO: prefix with _dbg
    _assert_msg_(ACTIONREPLAY, progress == candidates,
                 "progress doesn't match number of candidates");

    DEBUG_LOG(ACTIONREPLAY, "%s(): %lu -> %lu", __FUNCTION__, progress, new_search_results.size());
    promised_results.set_value(std::move(new_search_results));
  }

  InvokeCallbacks();
}

void Finder::InvokeCallbacks()
{
  if (m_primary_new_results_callback)
  {
    (*m_primary_new_results_callback)();
  }

  for (const auto& entry : m_extra_new_results_callbacks)
  {
    entry.second();
  }
}

Finder::Predicate Finder::MakePredicate(const MemoryItem& match)
{
  if (const auto* match_ptr = std::get_if<u8>(&match))
  {
    const auto match_value = *match_ptr;
    return [match_value](const MemoryItem& item) -> bool {
      const auto* value_ptr = std::get_if<u8>(&item);
      return value_ptr != nullptr && *value_ptr == match_value;
    };
  }

  if (const auto* match_ptr = std::get_if<u16>(&match))
  {
    const auto match_value = *match_ptr;
    return [match_value](const MemoryItem& item) -> bool {
      const auto* value_ptr = std::get_if<u16>(&item);
      return value_ptr != nullptr && *value_ptr == match_value;
    };
  }

  if (const auto* match_ptr = std::get_if<u32>(&match))
  {
    const auto match_value = *match_ptr;
    return [match_value](const MemoryItem& item) -> bool {
      const auto* value_ptr = std::get_if<u32>(&item);
      return value_ptr != nullptr && *value_ptr == match_value;
    };
  }

  if (const auto* match_ptr = std::get_if<u64>(&match))
  {
    const auto match_value = *match_ptr;
    return [match_value](const MemoryItem& item) -> bool {
      const auto* value_ptr = std::get_if<u64>(&item);
      return value_ptr != nullptr && *value_ptr == match_value;
    };
  }

  return [](const MemoryItem& item) -> bool { return false; };
}

}  // namespace Cheats
