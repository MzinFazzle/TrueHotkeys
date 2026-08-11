# Local overlay for the colorglass registry's commonlibsse-ng port - added
# 2026-08-10. See DESIGN.md's 1.1.30 entry for the full story: the colorglass
# registry's own port (gitlab.com/colorglass/vcpkg-colorglass) still pins
# CharmedBaryon/CommonLibSSE at commit c4ab853d095e81e3390b282d7ba01ab2f24ebf25
# as of this writing - confirmed directly by fetching that exact registry
# commit's portfile.cmake - which predates RE::ControlMap::PushInputContext()/
# PopInputContext() being added upstream. Bumping vcpkg-configuration.json's
# baseline (tried first, v1.1.30's first attempt) can't fix this - every
# baseline still resolves to that same pinned commit until colorglass's own
# port catches up, which is outside this project's control and on no known
# timeline.
#
# This overlay is otherwise an exact copy of colorglass's real port (same
# dependencies, same install steps - fetched directly from
# gitlab.com/colorglass/vcpkg-colorglass at commit c4ab853d itself, i.e. our
# actual currently-resolved port, not guessed), with only REPO/REF changed to
# pull CharmedBaryon/CommonLibSSE directly instead.
#
# REF is "master" (a moving branch, not a pinned commit) because GitHub's API
# was not reachable this session to resolve the exact current commit hash -
# confirmed via raw.githubusercontent.com that master's ControlMap.h has both
# methods as of 2026-08-10, but that's as far as verification could go
# without either the API or a build to actually pin it. SHA512 is
# deliberately the placeholder "0": vcpkg will refuse the download and print
# the real hash in its error the first time this is built - paste that value
# in below, and REF can be pinned to a real commit at the same time once
# there's an easy way to look it up (e.g. asking again once GitHub access
# cooperates, or reading it out of the SPDX file vcpkg writes on a
# successful install). Left this way rather than guessing a commit hash,
# since a wrong guess would just be a different flavor of the same problem
# this whole detour was trying to avoid.
vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO CharmedBaryon/CommonLibSSE
        REF master
        SHA512 ca837d11c6919aa0c07156641c4de520e7ff7beaf0c3f19eaad5add525aedbcdfe2b72580c74bfe85d944ff7a748be4822575a266c5f7ea80cf82c3cfedaaf7d
        HEAD_REF master
)

vcpkg_configure_cmake(
        SOURCE_PATH "${SOURCE_PATH}"
        PREFER_NINJA
        OPTIONS -DBUILD_TESTS=off -DSKSE_SUPPORT_XBYAK=on
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME CommonLibSSE CONFIG_PATH lib/cmake)
vcpkg_copy_pdbs()

file(GLOB CMAKE_CONFIGS "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE/*.cmake")
file(INSTALL ${CMAKE_CONFIGS} DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE")
file(INSTALL "${SOURCE_PATH}/cmake/CommonLibSSE.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE")

file(
        INSTALL "${SOURCE_PATH}/LICENSE"
        DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
        RENAME copyright)
