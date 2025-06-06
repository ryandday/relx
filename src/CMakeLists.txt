# Add common library components if any
# This is for any non-header-only components that are always built

# Add PostgreSQL components only if enabled
if(RELX_ENABLE_POSTGRES_CLIENT)
    add_subdirectory(postgres)
endif()