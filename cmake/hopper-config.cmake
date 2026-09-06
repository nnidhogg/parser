# The installed hopper package depends on munch, which every parser reads its tokens through; munch is found as
# its own installed package, and everything else hopper uses is the standard library.
include(CMakeFindDependencyMacro)
find_dependency(munch)

include("${CMAKE_CURRENT_LIST_DIR}/hopper-targets.cmake")
