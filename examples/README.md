# relx Examples

This directory contains examples demonstrating how to use the relx library with different database systems.

## PostgreSQL Example

The `postgresql_example.cpp` file demonstrates using relx with PostgreSQL databases. It showcases:

1. Connecting to a PostgreSQL database
2. Creating table schema definitions
3. Creating tables with various constraints
4. Inserting data with transactions
5. Basic CRUD operations (Create, Read, Update, Delete)
6. Complex queries with:
   - JOINs
   - Aggregation functions
   - GROUP BY and HAVING clauses
   - CASE expressions
   - Subqueries
   - Ordering and limiting results

### Prerequisites

To run the PostgreSQL example, you need:

1. A running PostgreSQL server
2. A database named `relx_example` (or modify the connection string in the example)
3. Appropriate user credentials (default: username=postgres, password=postgres)

### How to Build

```bash
# From the root directory of the project
mkdir build && cd build
cmake ..
make postgresql_example
```

### How to Run

```bash
# From the build directory
./examples/postgresql_example
```

### Expected Output

The example will:

1. Connect to the database
2. Create tables for users, posts, and comments
3. Insert sample data
4. Demonstrate basic queries (SELECT, UPDATE, DELETE)
5. Demonstrate complex queries with JOINs, aggregation, and advanced SQL features
6. Clean up by dropping the tables
7. Disconnect from the database

Each query will display its results to the console, showing the power of relx's type-safe query building and results processing. 