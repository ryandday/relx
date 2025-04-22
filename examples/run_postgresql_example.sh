#!/bin/bash

# Check if docker is installed
if ! command -v docker &> /dev/null; then
    echo "Docker is not installed. Please install Docker first."
    exit 1
fi

# Check if PostgreSQL container is running
if ! docker ps | grep -q relx-postgres; then
    echo "Starting PostgreSQL container..."
    docker run --name relx-postgres -e POSTGRES_PASSWORD=postgres -e POSTGRES_DB=relx_example -p 5435:5432 -d postgres:14
    
    # Wait for PostgreSQL to initialize
    echo "Waiting for PostgreSQL to initialize..."
    sleep 5
else
    echo "PostgreSQL container is already running."
fi

# Check if the example has been built
if [ ! -f "../build/examples/postgresql_example" ]; then
    echo "Building the example..."
    mkdir -p ../build
    cd ../build
    cmake ..
    make postgresql_example
    cd ../examples
fi

# Run the example
echo "Running PostgreSQL example..."
../build/examples/postgresql_example

# Ask if the user wants to stop the PostgreSQL container
read -p "Do you want to stop the PostgreSQL container? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Stopping PostgreSQL container..."
    docker stop relx-postgres
    docker rm relx-postgres
    echo "PostgreSQL container stopped and removed."
fi 