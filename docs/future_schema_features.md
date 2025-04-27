# Future Schema Features

## Table and Column Attributes

### Autoincrement Primary Keys
- Implementation of autoincrement IDs for primary keys
- PostgreSQL uses `SERIAL` or `IDENTITY` types
- SQLite uses `AUTOINCREMENT` keyword
- MySQL uses `AUTO_INCREMENT` attribute

### Column Collation
- Text collation specifications for string columns
- Controls case sensitivity, accent handling, and sorting

### Generated/Computed Columns
- Columns whose values are calculated from other columns
- Support for both stored and virtual (calculated on read) options

### Temporary Tables
- Definition of tables existing only for the duration of a connection
- Simplified cleanup for transient data

## Constraints and Options

### Additional Column Constraints
- `NOT NULL WITH DEFAULT` constraint
- `GENERATED ALWAYS AS` for computed columns

### Table Options
- `WITHOUT ROWID` optimization (SQLite-specific)
- `STRICT` table type enforcement (SQLite-specific)
- Storage engine specifications (MySQL/MariaDB)

### Trigger Support
- Before/After Insert/Update/Delete triggers
- Integration with table definitions

## Documentation and Metadata

### Schema Comments
- SQL comments on tables and columns 
- Documentation generation from schema

## Advanced Features

### View Support
- Define views as composable queries over tables
- Materialized view support

### Column Type Modifiers
- `VARCHAR(N)` with length limitations
- `DECIMAL(precision, scale)` for exact numeric types
- Array types (PostgreSQL)

### Table Partitioning
- Horizontal and vertical partitioning definitions
- Sharding hints

### Column Storage Attributes
- Compression options
- Storage engine specific hints
