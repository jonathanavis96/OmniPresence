# OmniPresence Unit Tests
# Unit tests for rule engine, template engine, and core logic

# Optionally enable Qt6 test framework if available
find_package(Qt6 COMPONENTS Test QUIET)

if(Qt6_FOUND)
    # Test infrastructure would go here
    # Tests for:
    # - Rule matching engine (process names, window titles, browser domains, etc.)
    # - Template engine (variable substitution, conditional logic)
    # - Privacy level filtering and masking
    # - Discord RPC message formatting
else()
    # Placeholder test when Qt6 Test framework is not available
    add_test(
        NAME placeholder
        COMMAND ${CMAKE_COMMAND} -E echo "Qt6 Test framework not found. Install Qt6 with Test component."
    )
endif()
