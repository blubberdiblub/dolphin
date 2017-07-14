// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "Common/CommonTypes.h"

#include "DolphinWX/CheatingNew/Utils.h"

namespace Cheats
{
class Finder final
{
public:
  typedef std::function<void()> NewResultsCallback;
  typedef std::optional<NewResultsCallback> OptionalNewResultsCallback;
  typedef std::function<void(int)> SearchProgressCallback;
  typedef std::optional<SearchProgressCallback> OptionalSearchProgressCallback;

  enum class Status : unsigned int
  {
    OK,
    ERROR_INVALID_VALUE,
    ERROR_MISMATCHED_VALUE_TYPE,
    ERROR_MEMORY_NOT_INITIALIZED,
    ERROR_UNKNOWN_SIZE_OF_VALUE,
    ERROR_NO_VALID_MEMORY_RANGES,
    ERROR_SEARCH_IN_PROGRESS,
  };

  explicit Finder(const OptionalNewResultsCallback& new_results_callback = std::nullopt);
  ~Finder();

  Status SearchItem(const MemoryItem& item,
                    const OptionalSearchProgressCallback& search_progress_callback = std::nullopt);
  bool NewResults();
  void CancelSearch();
  void ClearResults();
  std::size_t GetResultCount() const;
  MaybeAddress GetAddress(std::size_t idx) const;
  MemoryItemType GetItemType(std::size_t idx) const;
  MemoryItem GetCurrentItem(std::size_t idx);
  MemoryItem GetPreviousItem(std::size_t idx);

  int RegisterCallback(NewResultsCallback new_results_callback);
  void UnregisterCallback(int callback_id);

private:
  void UpdateItem(SearchResult& result);

  typedef std::vector<SearchResult> ResultContainer;
  typedef std::optional<ResultContainer> MaybeResultContainer;

  OptionalNewResultsCallback m_primary_new_results_callback;
  typedef std::list<std::pair<int, NewResultsCallback>> CallbackCollection;
  CallbackCollection m_extra_new_results_callbacks;
  int m_recent_callback_id = 0;

  MemoryItemType m_search_value_type = MemoryItemType::UNSPECIFIED;
  // Mutex guards r/w m_search_thread, r/w m_future_results and writing to m_cancel_search.
  std::recursive_mutex m_thread_mutex;
  std::thread m_search_thread;
  std::atomic_bool m_cancel_search{false};
  std::future<MaybeResultContainer> m_future_results;
  // Mutex guards search results, except in the search thread. The methods writing m_search_results
  // need to make sure they return without mutating m_search_results when the thread is running.
  mutable std::shared_mutex m_search_results_mutex;
  ResultContainer m_search_results;

  typedef std::function<bool(const MemoryItem& item)> Predicate;

  template <typename Iter>
  void DoSearch(Iter start, Iter stop, MemoryItemType type, const Predicate pred,
                const OptionalSearchProgressCallback search_progress_callback,
                std::promise<void> promised_shared_lock,
                std::promise<MaybeResultContainer> promised_results);

  void InvokeCallbacks();

  Predicate MakePredicate(const MemoryItem& match);

  void JoinSearchThread();
};

}  // namespace Cheats
