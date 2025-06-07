# Add common library components if any
# This is for any non-header-only components that are always built

# Add migrations library (always available)
add_subdirectory(migrations)

# Add PostgreSQL components only if enabled
if(RELX_ENABLE_POSTGRES_CLIENT)
    add_subdirectory(postgres)
endif()