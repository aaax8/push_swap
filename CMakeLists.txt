cmake_minimum_required(VERSION 3.25)

project(untitled LANGUAGES CXX)

# === C++�W�� ===
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# === MSVC ���ʐݒ� ===
if (MSVC)
    add_compile_definitions(
            _CRT_SECURE_NO_WARNINGS
            _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
            NOMINMAX
            WIN32
            _WINDOWS
    )
endif()

# === 実行ファイル（CLionで全cpp/hを自動ターゲット所属）===
add_executable(untitled sort_algorithm/YakinamashiQuery/IterativeGreedy.h sort_algorithm/YakinamashiQuery/Construction/GreedyQueriesConstructor.cpp)

file(GLOB PROJECT_ROOT_CPP
        "${CMAKE_SOURCE_DIR}/*.cpp"
        )
file(GLOB PROJECT_ROOT_HEADERS
        "${CMAKE_SOURCE_DIR}/*.h"
        )
file(GLOB_RECURSE PROJECT_COMMAND_CPP
        "${CMAKE_SOURCE_DIR}/Command/*.cpp"
        )
file(GLOB_RECURSE PROJECT_COMMAND_HEADERS
        "${CMAKE_SOURCE_DIR}/Command/*.h"
        )
file(GLOB_RECURSE PROJECT_SORT_CPP
        "${CMAKE_SOURCE_DIR}/sort_algorithm/*.cpp"
        )
file(GLOB_RECURSE PROJECT_SORT_HEADERS
        "${CMAKE_SOURCE_DIR}/sort_algorithm/*.h"
        )
file(GLOB PROJECT_LEGACY_LIS_CPP
        "${CMAKE_SOURCE_DIR}/Legacy/lis_chank/Lis.cpp"
        "${CMAKE_SOURCE_DIR}/Legacy/lis_chank/PrecureSort.cpp"
        "${CMAKE_SOURCE_DIR}/Legacy/lis_chank/MergeBSale.cpp"
        )
file(GLOB_RECURSE PROJECT_ANKERL_HEADERS
        "${CMAKE_SOURCE_DIR}/ankerl/*.h"
        )

set(PROJECT_CPP
        ${PROJECT_ROOT_CPP}
        ${PROJECT_COMMAND_CPP}
        ${PROJECT_SORT_CPP}
        ${PROJECT_LEGACY_LIS_CPP}
        )
set(PROJECT_HEADERS
        ${PROJECT_ROOT_HEADERS}
        ${PROJECT_COMMAND_HEADERS}
        ${PROJECT_SORT_HEADERS}
        ${PROJECT_ANKERL_HEADERS}
        )

# build生成物や外部生成コードは除外（誤ってmain重複や不要コンパイルを防ぐため）
set(EXCLUDE_PATTERNS
        "/Archive/"
        "/build/"
        "/build_to_vs/"
        "/cmake-build-debug/"
        "/cmake-build-debug-/"
        "/cmake-build-release/"
        "/out/build/"
        )

foreach(PATTERN IN LISTS EXCLUDE_PATTERNS)
    list(FILTER PROJECT_CPP EXCLUDE REGEX ".*${PATTERN}.*")
    list(FILTER PROJECT_HEADERS EXCLUDE REGEX ".*${PATTERN}.*")
endforeach()

# 単一実行バイナリでmain重複になるテストエントリは除外
list(FILTER PROJECT_CPP EXCLUDE REGEX ".*/sort_algorithm/YakinamashiChunk/Neighborhood/test_neighborhood\\.cpp$")
# human_review_begin: checker.cpp は checker 専用の main を持つため、既存 untitled/push_swap から除外する。
list(FILTER PROJECT_CPP EXCLUDE REGEX ".*/checker\\.cpp$")
# human_review_end

target_sources(untitled PRIVATE
        ${CMAKE_SOURCE_DIR}/sort_algorithm/A2BBeam/test/test_b2a_under_max.cpp
        ${PROJECT_CPP}
        ${PROJECT_HEADERS}
        )

# human_review_begin: CLion 用の既存 untitled は残し、明示ビルド時だけ push_swap 名の成果物も作るため。
add_executable(push_swap EXCLUDE_FROM_ALL)

target_sources(push_swap PRIVATE
        ${CMAKE_SOURCE_DIR}/sort_algorithm/A2BBeam/test/test_b2a_under_max.cpp
        ${PROJECT_CPP}
        ${PROJECT_HEADERS}
        )

set_target_properties(push_swap PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
# human_review_end

# human_review_begin: nafuka tester が使う checker を CMake から明示ビルドできるようにするため。
add_executable(checker EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/checker.cpp
        )

set_target_properties(checker PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
# human_review_end

# === include �p�X ===
# すべてのサブディレクトリを自動的に取得してインクルードパスに追加

# CLionで赤字が出にくいよう、主要サブディレクトリを静的に指定
# human_review_begin: push_swap ターゲットにも既存 untitled と同じ include path を適用するため。
set(PROJECT_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Archive/StrictMoveTo
        ${CMAKE_SOURCE_DIR}/Command
        ${CMAKE_SOURCE_DIR}/sort_algorithm
        ${CMAKE_SOURCE_DIR}/sort_algorithm/InitBeam
        ${CMAKE_SOURCE_DIR}/sort_algorithm/PastYakinamashi
        ${CMAKE_SOURCE_DIR}/sort_algorithm/YakinamashiQuery/Order
        ${CMAKE_SOURCE_DIR}/sort_algorithm/Yakinamashi/Order
        ${CMAKE_SOURCE_DIR}/sort_algorithm/YakinamashiQuery
        ${CMAKE_SOURCE_DIR}/sort_algorithm/YakinamashiQuery/Command
        ${CMAKE_SOURCE_DIR}/sort_algorithm/YakinamashiQuery/Query
        ${CMAKE_SOURCE_DIR}/sort_algorithm/YakinamashiQuery/State
        ${CMAKE_SOURCE_DIR}/sort_algorithm/YakinamashiQuery/Util
        ${CMAKE_SOURCE_DIR}/sort_algorithm/YakinamashiQuery/Validation
        ${CMAKE_SOURCE_DIR}/sort_algorithm/YakinamashiQuery/Effect
        # 必要に応じて他のサブディレクトリも追加
        )

target_include_directories(untitled PRIVATE ${PROJECT_INCLUDE_DIRS})
target_include_directories(push_swap PRIVATE ${PROJECT_INCLUDE_DIRS})
# human_review_end

# === MSVC: UTF-8 �𖾎��i����̖{���I�C���j===
if (MSVC)
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    target_compile_options(untitled PRIVATE /utf-8 /bigobj)
    # human_review_begin: push_swap ターゲットにも既存 untitled と同じ MSVC compile option を適用するため。
    target_compile_options(push_swap PRIVATE /utf-8 /bigobj)
    # human_review_end
    # human_review_begin: checker.cpp も MSVC で UTF-8 と大きめのオブジェクト設定を使うため。
    target_compile_options(checker PRIVATE /utf-8 /bigobj)
    # human_review_end
endif()
add_compile_definitions(PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

# human_review_begin: Windows でも tester が探す拡張子なし push_swap を明示ターゲットビルド時に生成するため。
if (WIN32)
    add_custom_command(TARGET push_swap POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                    $<TARGET_FILE:push_swap>
                    ${CMAKE_SOURCE_DIR}/push_swap
            )
endif()
# human_review_end

# human_review_begin: Windows でも tester が探す拡張子なし checker を明示ターゲットビルド時に生成するため。
if (WIN32)
    add_custom_command(TARGET checker POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                    $<TARGET_FILE:checker>
                    ${CMAKE_SOURCE_DIR}/checker
            )
endif()
# human_review_end
