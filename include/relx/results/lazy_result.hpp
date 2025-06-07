#pragma once

#include "../query/core.hpp"
#include "result.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace relx::result {

/// @brief Lazy cell that defers parsing until accessed
class LazyCell {
public:
  /// @brief Constructs a lazy cell with raw data and parsing context
  LazyCell(std::string_view raw_data, size_t start_pos, size_t end_pos)
      : raw_data_(raw_data), start_pos_(start_pos), end_pos_(end_pos) {}

  /// @brief Check if the cell contains a NULL value
  bool is_null() const { 
    auto value = get_raw_value();
    return value == "NULL"; 
  }

  /// @brief Get the raw string value (parsed on demand)
  std::string get_raw_value() const {
    return std::string(raw_data_.substr(start_pos_, end_pos_ - start_pos_));
  }

  /// @brief Parse the cell's value as the specified type
  template <typename T>
  ResultProcessingResult<T> as() const {
    return as<T>(false);
  }

  /// @brief Parse the cell's value as the specified type with boolean conversion options
  template <typename T>
  ResultProcessingResult<T> as(bool allow_numeric_bools) const {
    // Create a temporary Cell with the parsed value and delegate
    Cell temp_cell(get_raw_value());
    return temp_cell.template as<T>(allow_numeric_bools);
  }

private:
  std::string_view raw_data_;
  size_t start_pos_;
  size_t end_pos_;
};

/// @brief Lazy row that defers cell parsing until accessed
class LazyRow {
public:
  /// @brief Default constructor
  LazyRow() : raw_data_(""), column_names_({}), cells_parsed_(false), owns_data_(false) {}
  
  /// @brief Constructs a lazy row with raw data and column information
  LazyRow(std::string_view raw_data, std::vector<std::string> column_names = {})
      : raw_data_(raw_data), column_names_(std::move(column_names)), cells_parsed_(false), owns_data_(false) {}
  
  /// @brief Constructs a lazy row that owns its data (for streaming)
  LazyRow(std::string owned_data, std::vector<std::string> column_names)
      : raw_data_(), owned_data_(std::move(owned_data)), column_names_(std::move(column_names)), 
        cells_parsed_(false), owns_data_(true) {
    raw_data_ = owned_data_;
  }

  /// @brief Copy constructor
  LazyRow(const LazyRow& other)
      : raw_data_(other.raw_data_), owned_data_(other.owned_data_), column_names_(other.column_names_),
        cell_positions_(other.cell_positions_), cells_parsed_(other.cells_parsed_), owns_data_(other.owns_data_) {
    if (owns_data_) {
      // If this row owns its data, update raw_data_ to point to our copy
      raw_data_ = owned_data_;
    }
  }

  /// @brief Copy assignment operator
  LazyRow& operator=(const LazyRow& other) {
    if (this != &other) {
      raw_data_ = other.raw_data_;
      owned_data_ = other.owned_data_;
      column_names_ = other.column_names_;
      cell_positions_ = other.cell_positions_;
      cells_parsed_ = other.cells_parsed_;
      owns_data_ = other.owns_data_;
      
      if (owns_data_) {
        // If this row owns its data, update raw_data_ to point to our copy
        raw_data_ = owned_data_;
      }
    }
    return *this;
  }

  /// @brief Move constructor
  LazyRow(LazyRow&& other) noexcept
      : raw_data_(other.raw_data_), owned_data_(std::move(other.owned_data_)), column_names_(std::move(other.column_names_)),
        cell_positions_(std::move(other.cell_positions_)), cells_parsed_(other.cells_parsed_), owns_data_(other.owns_data_) {
    if (owns_data_) {
      // If this row owns its data, update raw_data_ to point to our moved data
      raw_data_ = owned_data_;
    }
    other.owns_data_ = false; // Other object no longer owns data
  }

  /// @brief Move assignment operator
  LazyRow& operator=(LazyRow&& other) noexcept {
    if (this != &other) {
      raw_data_ = other.raw_data_;
      owned_data_ = std::move(other.owned_data_);
      column_names_ = std::move(other.column_names_);
      cell_positions_ = std::move(other.cell_positions_);
      cells_parsed_ = other.cells_parsed_;
      owns_data_ = other.owns_data_;
      
      if (owns_data_) {
        // If this row owns its data, update raw_data_ to point to our moved data
        raw_data_ = owned_data_;
      }
      
      other.owns_data_ = false; // Other object no longer owns data
    }
    return *this;
  }

  /// @brief Get a cell by index (parsed on demand)
  ResultProcessingResult<LazyCell> get_cell(size_t index) const {
    ensure_cells_parsed();
    
    if (index >= cell_positions_.size()) {
      return std::unexpected(ResultError{"Cell index out of range"});
    }
    
    const auto& [start, end] = cell_positions_[index];
    return LazyCell(raw_data_, start, end);
  }

  /// @brief Get a cell by column name
  ResultProcessingResult<LazyCell> get_cell(const std::string& name) const {
    if (column_names_.empty()) {
      return std::unexpected(ResultError{"Column names not available"});
    }

    for (size_t i = 0; i < column_names_.size(); ++i) {
      if (column_names_[i] == name) {
        return get_cell(i);
      }
    }

    return std::unexpected(ResultError{"Column name not found: " + name});
  }

  /// @brief Get a typed value by index
  template <typename T>
  ResultProcessingResult<T> get(size_t index) const {
    return get<T>(index, false);
  }

  /// @brief Get a typed value by index with boolean conversion options
  template <typename T>
  ResultProcessingResult<T> get(size_t index, bool allow_numeric_bools) const {
    auto cell = get_cell(index);
    if (!cell) {
      return std::unexpected(cell.error());
    }
    return cell->template as<T>(allow_numeric_bools);
  }

  /// @brief Get a typed value by column name
  template <typename T>
  ResultProcessingResult<T> get(const std::string& name) const {
    return get<T>(name, false);
  }

  /// @brief Get a typed value by column name with boolean conversion options
  template <typename T>
  ResultProcessingResult<T> get(const std::string& name, bool allow_numeric_bools) const {
    auto cell = get_cell(name);
    if (!cell) {
      return std::unexpected(cell.error());
    }
    return cell->template as<T>(allow_numeric_bools);
  }

  /// @brief Get the number of cells in this row
  size_t size() const {
    ensure_cells_parsed();
    return cell_positions_.size();
  }

  /// @brief Get the column names
  const std::vector<std::string>& column_names() const { return column_names_; }

private:
  mutable std::string_view raw_data_;
  std::string owned_data_; // For cases where the LazyRow owns the data
  std::vector<std::string> column_names_;
  mutable std::vector<std::pair<size_t, size_t>> cell_positions_;
  mutable bool cells_parsed_;
  bool owns_data_;

  void ensure_cells_parsed() const {
    if (cells_parsed_) return;

    // Parse cell positions from raw data
    size_t pos = 0;
    size_t start = 0;
    
    while (pos < raw_data_.size()) {
      if (raw_data_[pos] == '|') {
        cell_positions_.emplace_back(start, pos);
        start = pos + 1;
      }
      ++pos;
    }
    
    // Add the last cell
    if (start < raw_data_.size()) {
      cell_positions_.emplace_back(start, raw_data_.size());
    }
    
    cells_parsed_ = true;
  }
};

/// @brief Lazy result set that defers row parsing until accessed
class LazyResultSet {
public:
  /// @brief Constructs a lazy result set with raw data
  LazyResultSet(std::string raw_data)
      : raw_data_(std::move(raw_data)), rows_parsed_(false) {}

  /// @brief Get the number of rows (requires parsing row boundaries)
  size_t size() const {
    ensure_rows_parsed();
    return row_positions_.size();
  }

  /// @brief Check if the result set is empty
  bool empty() const {
    return size() == 0;
  }

  /// @brief Get a row by index (parsed on demand)
  LazyRow at(size_t index) const {
    ensure_rows_parsed();
    
    if (index >= row_positions_.size()) {
      throw std::out_of_range("Row index out of range");
    }
    
    const auto& [start, end] = row_positions_[index];
    std::string_view row_data(raw_data_.data() + start, end - start);
    return LazyRow(row_data, column_names_);
  }

  /// @brief Access a row by index using the subscript operator
  LazyRow operator[](size_t index) const {
    return at(index);
  }

  /// @brief Get the column names
  const std::vector<std::string>& column_names() const {
    ensure_rows_parsed();
    return column_names_;
  }

  /// @brief Iterator for lazy rows
  class iterator {
  public:
    iterator(const LazyResultSet& result_set, size_t index)
        : result_set_(result_set), index_(index) {}

    LazyRow operator*() const {
      return result_set_.at(index_);
    }

    iterator& operator++() {
      ++index_;
      return *this;
    }

    bool operator!=(const iterator& other) const {
      return index_ != other.index_;
    }

  private:
    const LazyResultSet& result_set_;
    size_t index_;
  };

  iterator begin() const {
    return iterator(*this, 0);
  }

  iterator end() const {
    return iterator(*this, size());
  }

  /// @brief Transform to regular ResultSet if needed
  ResultSet to_result_set() const {
    std::vector<Row> rows;
    rows.reserve(size());
    
    for (size_t i = 0; i < size(); ++i) {
      auto lazy_row = at(i);
      std::vector<Cell> cells;
      cells.reserve(lazy_row.size());
      
      for (size_t j = 0; j < lazy_row.size(); ++j) {
        auto lazy_cell = lazy_row.get_cell(j);
        if (lazy_cell) {
          cells.emplace_back(lazy_cell->get_raw_value());
        }
      }
      
      rows.emplace_back(std::move(cells), column_names_);
    }
    
    return ResultSet(std::move(rows), column_names_);
  }

private:
  std::string raw_data_;
  mutable std::vector<std::pair<size_t, size_t>> row_positions_;
  mutable std::vector<std::string> column_names_;
  mutable bool rows_parsed_;

  void ensure_rows_parsed() const {
    if (rows_parsed_) return;

    // Find line boundaries
    size_t pos = 0;
    size_t line_start = 0;
    bool first_line = true;
    
    while (pos <= raw_data_.size()) {
      if (pos == raw_data_.size() || raw_data_[pos] == '\n') {
        if (pos > line_start) {
          std::string_view line(raw_data_.data() + line_start, pos - line_start);
          
          if (first_line) {
            // Parse header line for column names
            parse_column_names(line);
            first_line = false;
          } else if (!line.empty()) {
            // Add data row
            row_positions_.emplace_back(line_start, pos);
          }
        }
        line_start = pos + 1;
      }
      ++pos;
    }
    
    rows_parsed_ = true;
  }

  void parse_column_names(std::string_view header_line) const {
    size_t pos = 0;
    size_t start = 0;
    
    while (pos < header_line.size()) {
      if (header_line[pos] == '|') {
        if (pos > start) {
          column_names_.emplace_back(header_line.substr(start, pos - start));
        }
        start = pos + 1;
      }
      ++pos;
    }
    
    // Add the last column
    if (start < header_line.size()) {
      column_names_.emplace_back(header_line.substr(start));
    }
  }
};

/// @brief Parse raw results from a database into a lazy ResultSet
/// @tparam Query The query type
/// @param query The query that was executed
/// @param raw_results The raw results as CSV or similar format
/// @return A LazyResultSet that parses data on demand
template <query::SqlExpr Query>
LazyResultSet parse_lazy(const Query& /*query*/, std::string raw_results) {
  return LazyResultSet(std::move(raw_results));
}

}  // namespace relx::result 