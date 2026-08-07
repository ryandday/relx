#pragma once

#include "../query/core.hpp"
#include "result.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
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

  /// @brief Check if the cell contains a NULL value. Nullness is out-of-band: the
  /// encoded cell text `\N` marks NULL, while a real "NULL" (or `\N`) value arrives
  /// escaped and is not null.
  bool is_null() const { return encoded_value() == text_format::null_marker; }

  /// @brief Get the raw string value (unescaped on demand). A NULL cell carries no
  /// text; unescaping its `\N` marker would fabricate the value "N".
  std::string get_raw_value() const {
    if (is_null()) {
      return {};
    }
    return text_format::unescape(encoded_value());
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
    Cell temp_cell = is_null() ? Cell::null() : Cell(get_raw_value());
    return temp_cell.template as<T>(allow_numeric_bools);
  }

private:
  std::string_view raw_data_;
  size_t start_pos_;
  size_t end_pos_;

  std::string_view encoded_value() const {
    return raw_data_.substr(start_pos_, end_pos_ - start_pos_);
  }
};

/// @brief Lazy row that defers cell parsing until accessed
class LazyRow {
public:
  /// @brief Default constructor
  LazyRow() : raw_data_(""), column_names_({}), cells_parsed_(false), owns_data_(false) {}

  /// @brief Constructs a lazy row with raw data and column information
  LazyRow(std::string_view raw_data, std::vector<std::string> column_names = {})
      : raw_data_(raw_data), column_names_(std::move(column_names)), cells_parsed_(false),
        owns_data_(false) {}

  /// @brief Constructs a lazy row that owns its data (for streaming)
  LazyRow(std::string owned_data, std::vector<std::string> column_names)
      : raw_data_(), owned_data_(std::move(owned_data)), column_names_(std::move(column_names)),
        cells_parsed_(false), owns_data_(true) {
    raw_data_ = owned_data_;
  }

  /// @brief Copy constructor
  LazyRow(const LazyRow& other)
      : raw_data_(other.raw_data_), owned_data_(other.owned_data_),
        column_names_(other.column_names_), cell_positions_(other.cell_positions_),
        cells_parsed_(other.cells_parsed_), owns_data_(other.owns_data_) {
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
      : raw_data_(other.raw_data_), owned_data_(std::move(other.owned_data_)),
        column_names_(std::move(other.column_names_)),
        cell_positions_(std::move(other.cell_positions_)), cells_parsed_(other.cells_parsed_),
        owns_data_(other.owns_data_) {
    if (owns_data_) {
      // If this row owns its data, update raw_data_ to point to our moved data
      raw_data_ = owned_data_;
    }
    other.owns_data_ = false;  // Other object no longer owns data
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

      other.owns_data_ = false;  // Other object no longer owns data
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
  std::string owned_data_;  // For cases where the LazyRow owns the data
  std::vector<std::string> column_names_;
  mutable std::vector<std::pair<size_t, size_t>> cell_positions_;
  mutable bool cells_parsed_;
  bool owns_data_;

  void ensure_cells_parsed() const {
    if (cells_parsed_) return;

    // A default-constructed sentinel row (no data, no columns) has zero cells; any
    // real row has N separators and N+1 cells, so trailing empty cells survive
    if (!raw_data_.empty() || !column_names_.empty()) {
      for (const auto cell : text_format::split_cells(raw_data_)) {
        const size_t start = static_cast<size_t>(cell.data() - raw_data_.data());
        cell_positions_.emplace_back(start, start + cell.size());
      }
    }

    cells_parsed_ = true;
  }
};

/// @brief Lazy result set that defers row parsing until accessed
class LazyResultSet {
public:
  /// @brief Constructs a lazy result set with raw data
  LazyResultSet(std::string raw_data) : raw_data_(std::move(raw_data)), rows_parsed_(false) {}

  /// @brief Get the number of rows (requires parsing row boundaries)
  size_t size() const {
    ensure_rows_parsed();
    return row_positions_.size();
  }

  /// @brief Check if the result set is empty
  bool empty() const { return size() == 0; }

  /// @brief Get a row by index (parsed on demand)
  ResultProcessingResult<LazyRow> at(size_t index) const {
    ensure_rows_parsed();

    if (index >= row_positions_.size()) {
      return std::unexpected(ResultError{"Row index out of range"});
    }

    const auto& [start, end] = row_positions_[index];
    // The row owns a copy of its data: a row handed out here must stay valid even if
    // the result set is moved or destroyed first
    return LazyRow(std::string(raw_data_, start, end - start), column_names_);
  }

  /// @brief Access a row by index using the subscript operator
  /// @note Throws std::out_of_range on an invalid index - use at() for the
  /// non-throwing form. Returning an empty row here would silently yield
  /// default-constructed data.
  LazyRow operator[](size_t index) const {
    auto result = at(index);
    if (!result) {
      throw std::out_of_range(result.error().message);
    }
    return *result;
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
      auto result = result_set_.at(index_);
      if (!result) {
        // Iteration has no error channel; an empty row would silently yield
        // default-constructed data
        throw std::out_of_range(result.error().message);
      }
      return *result;
    }

    iterator& operator++() {
      ++index_;
      return *this;
    }

    bool operator!=(const iterator& other) const { return index_ != other.index_; }

  private:
    const LazyResultSet& result_set_;
    size_t index_;
  };

  iterator begin() const { return iterator(*this, 0); }

  iterator end() const { return iterator(*this, size()); }

  /// @brief Transform to regular ResultSet if needed
  /// @note This method will propagate any errors encountered during transformation
  ResultProcessingResult<ResultSet> to_result_set() const {
    std::vector<Row> rows;
    rows.reserve(size());

    for (size_t i = 0; i < size(); ++i) {
      auto lazy_row_result = at(i);
      if (!lazy_row_result) {
        return std::unexpected(lazy_row_result.error());
      }

      auto lazy_row = *lazy_row_result;
      std::vector<Cell> cells;
      cells.reserve(lazy_row.size());

      for (size_t j = 0; j < lazy_row.size(); ++j) {
        auto lazy_cell = lazy_row.get_cell(j);
        if (lazy_cell) {
          cells.push_back(lazy_cell->is_null() ? Cell::null() : Cell(lazy_cell->get_raw_value()));
        } else {
          return std::unexpected(ResultError{"Failed to get cell at index " + std::to_string(j)});
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

    // `\n` terminates a row, so an empty line is a real row (a single empty cell);
    // a trailing fragment without a terminator still counts as a line. Line 1 is the
    // header.
    size_t pos = 0;
    size_t line_start = 0;
    bool first_line = true;

    const auto add_line = [&](size_t line_end) {
      if (first_line) {
        parse_column_names(std::string_view(raw_data_.data() + line_start, line_end - line_start));
        first_line = false;
      } else {
        row_positions_.emplace_back(line_start, line_end);
      }
    };

    while (pos < raw_data_.size()) {
      if (raw_data_[pos] == '\n') {
        add_line(pos);
        line_start = pos + 1;
      }
      ++pos;
    }
    if (line_start < raw_data_.size()) {
      add_line(raw_data_.size());
    }

    rows_parsed_ = true;
  }

  /// @brief Parse the header line. Empty names are kept - dropping them would shift
  /// the name-to-index mapping and make get<T>("name") read the wrong column.
  void parse_column_names(std::string_view header_line) const {
    if (header_line.empty()) {
      return;
    }
    for (const auto raw_name : text_format::split_cells(header_line)) {
      column_names_.emplace_back(text_format::unescape(raw_name));
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