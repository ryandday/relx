-- This script runs during PostgreSQL container initialization
-- It ensures the relx_test database exists and is properly configured

-- Note: The POSTGRES_DB environment variable should already create the database,
-- but this script provides a backup and can add additional configuration

-- Connect to the default database first
\c postgres;

-- Create the relx_test database if it doesn't exist
-- Using a more reliable approach for older PostgreSQL versions
SELECT 'CREATE DATABASE relx_test'
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = 'relx_test')\gexec

-- Connect to the relx_test database to configure it
\c relx_test;

-- Set database-level configuration
ALTER DATABASE relx_test SET timezone = 'UTC';
ALTER DATABASE relx_test SET default_transaction_isolation = 'read committed';

-- Create any extensions that might be needed for testing
-- Note: These are commented out as they might not be available in all PostgreSQL images
-- CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
-- CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- Ensure the postgres user has all necessary privileges
GRANT ALL PRIVILEGES ON DATABASE relx_test TO postgres; 