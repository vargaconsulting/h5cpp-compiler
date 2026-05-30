vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO vargalabs/h5cpp-compiler
    REF "v${VERSION}"
    SHA512 fa3f5da1f028c5353c70387a94bd40543a27dbcdbd9ace8e0a8666978759018ab519dd60c17b1f99120e4cd48b2ff4bbbb56778efdd92787dd4fafcf1444dc28
    HEAD_REF staging
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/bin")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYRIGHT.txt")
