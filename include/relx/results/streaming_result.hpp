#pragma once

#include "lazy_result.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace relx::result {

/// @brief Streaming result set for very large datasets
template <typename DataSource>
class StreamingResultSet {
public:
  /// @brief Iterator that reads data on demand
  class streaming_iterator {
  public:
    streaming_iterator(DataSource& source, bool at_end = false)
        : source_(source), current_row_(), at_end_(at_end) {
      if (!at_end_) {
        advance();
      }
    }

    LazyRow operator*() const { return current_row_; }

    streaming_iterator& operator++() {
      advance();
      return *this;
    }

    bool operator!=(const streaming_iterator& other) const { return at_end_ != other.at_end_; }

    /// @brief Pull the next row from the data source
    /// @details Unlike the async iterator, begin() already sits on the first row (a
    /// synchronous fetch needs no co_await), so manual iteration advances after
    /// processing a row rather than before reading the first one:
    /// ```cpp
    /// for (auto it = results.begin(); !it.is_at_end(); it.advance()) {
    ///   process(*it);
    /// }
    /// ```
    /// Advancing past the end is a no-op.
    void advance() {
      if (at_end_) {
        return;
      }
      auto next_row_data = source_.get_next_row();
      if (next_row_data) {
        current_row_ = LazyRow(std::move(*next_row_data), source_.get_column_names());
      } else {
        at_end_ = true;
      }
    }

    /// @brief Whether the stream is exhausted, i.e. *it no longer names a row
    bool is_at_end() const { return at_end_; }

  private:
    DataSource& source_;
    LazyRow current_row_;
    bool at_end_;
  };

  StreamingResultSet(DataSource source) : source_(std::move(source)) {}

  // Iterators hold a reference to the stored data source; moving or copying the set
  // would silently end existing iterators, so the set is pinned in place
  StreamingResultSet(const StreamingResultSet&) = delete;
  StreamingResultSet& operator=(const StreamingResultSet&) = delete;
  StreamingResultSet(StreamingResultSet&&) = delete;
  StreamingResultSet& operator=(StreamingResultSet&&) = delete;

  streaming_iterator begin() { return streaming_iterator(source_); }

  streaming_iterator end() { return streaming_iterator(source_, true); }

  /// @brief Process every remaining row with a callback
  /// @details Mirrors AsyncStreamingResultSet::for_each for the synchronous case, minus
  /// the awaitable callback forms: a `void(const LazyRow&)` callback processes every row,
  /// a `bool(const LazyRow&)` callback stops the iteration by returning true.
  /// ```cpp
  /// results.for_each([](const auto& row) { process(row); });
  ///
  /// results.for_each([](const auto& row) -> bool {
  ///   return row.template get<int>("id").value_or(0) > 1000;  // true breaks
  /// });
  /// ```
  /// @tparam Func Callback type, returning void or bool
  /// @param func Callback invoked with each row
  template <typename Func>
  void for_each(Func&& func) {
    for (auto it = begin(); !it.is_at_end(); it.advance()) {
      using ReturnType = std::invoke_result_t<Func, decltype(*it)>;
      if constexpr (std::is_same_v<ReturnType, bool>) {
        if (func(*it)) {
          return;
        }
      } else {
        func(*it);
      }
    }
  }

  /// @brief The error that ended the stream, when the data source tracks one
  /// @details An empty optional means the rows simply ran out. Without this a failed
  /// query is indistinguishable from an empty result, since a row pull can only report
  /// "no more rows".
  decltype(auto) last_error() const
    requires requires(const DataSource& source) { source.last_error(); }
  {
    return source_.last_error();
  }

private:
  DataSource source_;
};

}  // namespace relx::result