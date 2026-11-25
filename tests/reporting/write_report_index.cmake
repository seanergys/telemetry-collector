# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: EUPL-1.2

file(MAKE_DIRECTORY "${REPORT_ROOT}")
file(LOCK "${REPORT_ROOT}/.report.lock" TIMEOUT 10)
set(index "# Test reports\n\n")
set(missing_reports)
foreach(suite IN ITEMS plugin-manager plugin-configuration plugin-tests)
  if(EXISTS "${REPORT_ROOT}/${suite}/report.md")
    string(APPEND index "- [${suite}](${suite}/report.md)\n")
  elseif(REQUIRE_ALL_REPORTS)
    string(APPEND index "- ${suite} (report unavailable)\n")
  endif()
  if(REQUIRE_ALL_REPORTS)
    foreach(artifact IN ITEMS report.md ctest.log junit.xml)
      if(NOT EXISTS "${REPORT_ROOT}/${suite}/${artifact}")
        list(APPEND missing_reports "${suite}/${artifact}")
      endif()
    endforeach()
  endif()
endforeach()
file(WRITE "${REPORT_ROOT}/report.md" "${index}")
file(LOCK "${REPORT_ROOT}/.report.lock" RELEASE)

if(missing_reports)
  list(JOIN missing_reports ", " missing_report_list)
  message(FATAL_ERROR "Missing test report artifacts: ${missing_report_list}")
endif()
