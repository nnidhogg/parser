# Probes back claims made outside the libraries; a consumer building hopper as a dependency needs neither them nor
# their corpora, so they follow the same contract as the tests.
if (HOPPER_BUILD_TESTS)
    add_subdirectory(probes)
endif ()
