# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: EUPL-1.2

set(report "${ARTIFACT_ROOT}/report.md")
file(WRITE "${report}" "# ${REPORT_TITLE}\n\n")
if("${ctest_result}" STREQUAL "0")
  file(APPEND "${report}" "CTest result: passed.\n\n")
else()
  file(APPEND "${report}" "CTest result: failed (${ctest_result}).\n\n")
endif()

if(EXISTS "${ARTIFACT_ROOT}/ctest.log")
  file(APPEND "${report}" "[CTest log](ctest.log)\n\n")
endif()
if(EXISTS "${ARTIFACT_ROOT}/junit.xml")
  file(APPEND "${report}" "[Individual test results (JUnit)](junit.xml)\n\n")
  if(REPORT_PYTHON AND REPORT_CASES)
    execute_process(
      COMMAND "${REPORT_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/write_case_reports.py"
              "${ARTIFACT_ROOT}"
      RESULT_VARIABLE report_result)
    if(NOT "${report_result}" STREQUAL "0")
      message(WARNING "Could not write per-case logs for ${REPORT_TITLE}")
    endif()
  endif()
endif()
