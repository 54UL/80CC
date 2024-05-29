# - Config file for the 80CC_CORE package
# It defines the following variables
#  80CC_CORE_INCLUDE_DIRS - include directories for 80CC_CORE
#  80CC_CORE_LIBRARIES    - libraries to link against

# Compute paths
get_filename_component(80CC_CORE_CONFIG_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

# Set variables
set(80CC_CORE_INCLUDE_DIRS "${80CC_CORE_CONFIG_DIR}/../../src/Core/include")
set(80CC_CORE_LIBRARIES "80CC_CORE")

# Export variables
set(80CC_CORE_INCLUDE_DIRS "${80CC_CORE_INCLUDE_DIRS}" CACHE INTERNAL "")
set(80CC_CORE_LIBRARIES "${80CC_CORE_LIBRARIES}" CACHE INTERNAL "")
