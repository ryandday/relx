#pragma once

#include "lazy_result.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace relx::result {

/// @brief Streaming result set for very large datasets
template<typename DataSource>
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

    LazyRow operator*() const {
      return current_row_;
    }

    streaming_iterator& operator++() {
      advance();
      return *this;
    }

    bool operator!=(const streaming_iterator& other) const {
      return at_end_ != other.at_end_;
    }

  private:
    DataSource& source_;
    LazyRow current_row_;
    bool at_end_;

    void advance() {
      auto next_row_data = source_.get_next_row();
      if (next_row_data) {
        current_row_ = LazyRow(std::move(*next_row_data), source_.get_column_names());
      } else {
        at_end_ = true;
      }
    }
  };

  StreamingResultSet(DataSource source) : source_(std::move(source)) {}

  streaming_iterator begin() {
    return streaming_iterator(source_);
  }

  streaming_iterator end() {
    return streaming_iterator(source_, true);
  }

private:
  DataSource source_;
};

}  // namespace relx::result 