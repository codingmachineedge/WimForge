set(WIMFORGE_PROCESS_LAUNCH_SOURCE
    ${PROJECT_SOURCE_DIR}/src/core/ProcessLaunch.cpp)

function(wimforge_add_core_test target)
    cmake_parse_arguments(TEST "" "" "SOURCES;CORE_SOURCES" ${ARGN})
    qt_add_executable(${target}
        ${TEST_SOURCES}
        ${TEST_CORE_SOURCES}
        ${WIMFORGE_PROCESS_LAUNCH_SOURCE}
    )
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_compile_definitions(${target} PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
    )
    target_link_libraries(${target} PRIVATE Qt6::Core)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endfunction()

wimforge_add_core_test(project_history_tests
    SOURCES project_history_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/GitHistory.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ProjectConfig.cpp
)

wimforge_add_core_test(notification_store_tests
    SOURCES notification_store_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/GitHistory.cpp
        ${PROJECT_SOURCE_DIR}/src/core/NotificationStore.cpp
)

wimforge_add_core_test(opencode_setup_tests
    SOURCES opencode_setup_tests.cpp
    CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/core/OpenCodeSetup.cpp
)

wimforge_add_core_test(process_launch_tests
    SOURCES process_launch_tests.cpp
)

wimforge_add_core_test(embedded_terminal_tests
    SOURCES embedded_terminal_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/EmbeddedTerminalSession.cpp
        ${PROJECT_SOURCE_DIR}/src/core/EmbeddedTerminalSession.h
)

wimforge_add_core_test(qml_contract_tests
    SOURCES qml_contract_tests.cpp
)

wimforge_add_core_test(search_index_tests
    SOURCES search_index_tests.cpp
    CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/core/SearchIndex.cpp
)
target_compile_definitions(qml_contract_tests PRIVATE
    WIMFORGE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

wimforge_add_core_test(unattend_builder_tests
    SOURCES unattend_builder_tests.cpp
    CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/core/UnattendBuilder.cpp
)

wimforge_add_core_test(gpo_catalog_tests
    SOURCES gpo_catalog_tests.cpp
    CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/core/GpoCatalog.cpp
)

wimforge_add_core_test(gpo_policy_compiler_tests
    SOURCES gpo_policy_compiler_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/GpoCatalog.cpp
        ${PROJECT_SOURCE_DIR}/src/core/GpoPolicyCompiler.cpp
        ${PROJECT_SOURCE_DIR}/src/core/GitHistory.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ProjectConfig.cpp
)

wimforge_add_core_test(package_studio_tests
    SOURCES package_studio_tests.cpp
    CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/core/PackageStudio.cpp
)

wimforge_add_core_test(action_history_tests
    SOURCES action_history_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/ActionHistory.cpp
        ${PROJECT_SOURCE_DIR}/src/core/GitHistory.cpp
)

wimforge_add_core_test(project_bundle_tests
    SOURCES project_bundle_tests.cpp
    CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/core/ProjectBundle.cpp
)

wimforge_add_core_test(servicing_plan_tests
    SOURCES servicing_plan_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/GitHistory.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ProjectConfig.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ServicingPlan.cpp
)

wimforge_add_core_test(job_engine_tests
    SOURCES job_engine_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/GitHistory.cpp
        ${PROJECT_SOURCE_DIR}/src/core/JobEngine.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ProjectConfig.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ServicingPlan.cpp
)

wimforge_add_core_test(winforge_bridge_tests
    SOURCES winforge_bridge_tests.cpp
    CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/core/WinForgeBridge.cpp
)

wimforge_add_core_test(vm_lab_tests
    SOURCES vm_lab_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/VmLab.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabProviders.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabVmx.cpp
)

wimforge_add_core_test(vm_lab_manager_tests
    SOURCES vm_lab_manager_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/VmLab.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabProviders.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabVmx.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabManager.cpp
)

wimforge_add_core_test(vm_validation_store_tests
    SOURCES vm_validation_store_tests.cpp
    CORE_SOURCES ${PROJECT_SOURCE_DIR}/src/core/VmValidationStore.cpp
)

wimforge_add_core_test(vm_lab_cli_service_tests
    SOURCES vm_lab_cli_service_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/core/VmLab.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabProviders.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabVmx.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabManager.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabCliService.cpp
)

wimforge_add_core_test(cli_runner_tests
    SOURCES cli_runner_tests.cpp
    CORE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/cli/CliRunner.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ActionHistory.cpp
        ${PROJECT_SOURCE_DIR}/src/core/GitHistory.cpp
        ${PROJECT_SOURCE_DIR}/src/core/GpoCatalog.cpp
        ${PROJECT_SOURCE_DIR}/src/core/GpoPolicyCompiler.cpp
        ${PROJECT_SOURCE_DIR}/src/core/NotificationStore.cpp
        ${PROJECT_SOURCE_DIR}/src/core/PackageStudio.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ProjectConfig.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ProjectBundle.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ServicingPlan.cpp
        ${PROJECT_SOURCE_DIR}/src/core/UnattendBuilder.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLab.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabProviders.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabVmx.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabManager.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabCliService.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmLabScope.cpp
        ${PROJECT_SOURCE_DIR}/src/core/VmValidationStore.cpp
        ${PROJECT_SOURCE_DIR}/src/core/WinForgeBridge.cpp
)
