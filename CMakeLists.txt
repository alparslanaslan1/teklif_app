cmake_minimum_required(VERSION 3.21)
project(TeklifApp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Part 4'te Widgets girdi (teklif ekranı). Part 5'te PrintSupport giriyor
# (baskı/PDF). Network otomatik güncelleme (src/update) için eklendi.
find_package(Qt6 REQUIRED COMPONENTS Core Sql Widgets PrintSupport Network)

# ---------------------------------------------------------------------------
# Sürüm bilgisi: tek doğruluk kaynağı yukarıdaki project(... VERSION ...).
# CI, git tag'inden (v0.2.0) gelen değeri -DTEKLIF_VERSION=0.2.0 ile geçer;
# elle derlerken bu değer verilmezse project() sürümü kullanılır.
# Üretilen version.h build klasörüne yazılır, repoya girmez.
# ---------------------------------------------------------------------------
set(TEKLIF_VERSION "${PROJECT_VERSION}" CACHE STRING "Yayınlanan sürüm numarası")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE TEKLIF_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
if(NOT TEKLIF_GIT_SHA)
    set(TEKLIF_GIT_SHA "bilinmiyor")
endif()

string(TIMESTAMP TEKLIF_BUILD_DATE "%Y-%m-%dT%H:%M:%SZ" UTC)

configure_file(
    "${CMAKE_SOURCE_DIR}/src/core/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/core/version.h"
    @ONLY)

# Güncelleme manifestinin adresi. Varsayılan olarak BOŞ: adres verilmezse
# program güncelleme aramaz ve menüde ilgili girdi çıkmaz. CI, yayın
# derlemelerinde bunu GitHub Releases adresiyle doldurur.
set(TEKLIF_UPDATE_URL "" CACHE STRING "latest.json adresi")

# Yayıncı adı; installer ve exe özelliklerinde görünür.
set(TEKLIF_PUBLISHER "Karasu Vizyon Doğalgaz" CACHE STRING "Yayıncı adı")

# VERSIONINFO virgülle ayrılmış DÖRT sayı ister ("0,1,0,0"); TEKLIF_VERSION
# ise noktalı üç parçadır. Dördüncü alan build numarası olarak 0 bırakılır.
string(REPLACE "." "," TEKLIF_VERSION_RC "${TEKLIF_VERSION}")
set(TEKLIF_VERSION_RC "${TEKLIF_VERSION_RC},0")

message(STATUS "TeklifApp sürüm ${TEKLIF_VERSION} (${TEKLIF_GIT_SHA})")


# ---------------------------------------------------------------------------
# core: iş mantığı. Qt Widgets'e bağımlı DEĞİL — arayüz açmadan test edilebilir
# olması ve ileride (gerekirse) başka bir arayüz katmanına taşınabilmesi için.
# ---------------------------------------------------------------------------
add_library(teklif_core STATIC
    src/core/money.h
    src/core/money.cpp
    src/core/turkish.h
    src/core/turkish.cpp
    src/core/transaction.h
    src/core/transaction.cpp
    src/core/settings.h
    src/core/settings.cpp
    src/core/db.h
    src/core/db.cpp
    src/core/calculator.h
    src/core/calculator.cpp
    src/core/numtowords.h
    src/core/numtowords.cpp
    src/core/models.h
    src/core/quote_status.h
    src/core/quote_status.cpp
    src/core/search.h
    src/core/search.cpp
    src/core/csv.h
    src/core/csv.cpp
    src/core/repo_items.h
    src/core/repo_items.cpp
    src/core/numparse.h
    src/core/numparse.cpp
    src/core/repo_customers.h
    src/core/repo_customers.cpp
    src/core/repo_quotes.h
    src/core/repo_quotes.cpp
)
target_include_directories(teklif_core PUBLIC src "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(teklif_core PUBLIC Qt6::Core Qt6::Sql)

# ---------------------------------------------------------------------------
# update: otomatik güncelleme. teklif_core'a bağımlı, Widgets'e DEĞİL —
# sunucudaki latest.json'u okuyup sürüm karşılaştıran ve paketi indirip
# doğrulayan kısım arayüzsüz test edilebilsin diye. Kullanıcıya soru soran
# diyalog ui katmanında kalır.
# ---------------------------------------------------------------------------
add_library(teklif_update STATIC
    src/update/update_info.h
    src/update/update_info.cpp
    src/update/updater.h
    src/update/updater.cpp
)
target_link_libraries(teklif_update PUBLIC teklif_core Qt6::Network)

# ---------------------------------------------------------------------------
# print: baskı/PDF. teklif_core'a bağımlı; ekran (page_quote) ve gerçek
# yazıcı/PDF AYNI DocumentLayout/PrintService kodunu kullanır.
# ---------------------------------------------------------------------------
add_library(teklif_print STATIC
    src/print/company_logo.h
    src/print/company_logo.cpp
    src/print/document_layout.h
    src/print/document_layout.cpp
    src/print/print_service.h
    src/print/print_service.cpp
)
target_link_libraries(teklif_print PUBLIC teklif_core Qt6::PrintSupport)

# ---------------------------------------------------------------------------
# ui: teklif ekranı ve bileşenleri. teklif_core'a bağımlı, tersi değil —
# iş mantığı arayüzü hiç bilmez.
# ---------------------------------------------------------------------------
add_library(teklif_ui STATIC
    src/ui/item_search.h
    src/ui/item_search.cpp
    src/ui/dlg_line_entry.h
    src/ui/dlg_line_entry.cpp
    src/ui/quote_line_model.h
    src/ui/quote_line_model.cpp
    src/ui/quote_table_view.h
    src/ui/quote_table_view.cpp
    src/ui/page_quote.h
    src/ui/page_quote.cpp
    src/ui/quote_summary_model.h
    src/ui/quote_summary_model.cpp
    src/ui/page_archive.h
    src/ui/page_archive.cpp
    src/ui/page_customers.h
    src/ui/page_customers.cpp
    src/ui/item_table_model.h
    src/ui/item_table_model.cpp
    src/ui/page_catalog.h
    src/ui/page_catalog.cpp
    src/ui/theme.h
    src/ui/theme.cpp
    src/ui/dlg_first_run.h
    src/ui/dlg_first_run.cpp
    src/ui/update_prompt.h
    src/ui/update_prompt.cpp
    src/ui/page_settings.h
    src/ui/page_settings.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
)
target_link_libraries(teklif_ui PUBLIC teklif_core teklif_print teklif_update Qt6::Widgets)

# ---------------------------------------------------------------------------
# Uygulama. Windows'ta simge ve sürüm bilgisi bir kaynak (.rc) dosyasıyla
# exe'ye gömülür; diğer platformlarda bu adım atlanır.
# ---------------------------------------------------------------------------
set(TEKLIF_APP_SOURCES src/main.cpp)

if(WIN32)
    set(TEKLIF_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/teklif.ico")
    # .rc içinde ters bölü kaçış karakteridir; Windows yolları çevrilmeli.
    string(REPLACE "\\" "\\\\" TEKLIF_ICON_PATH "${TEKLIF_ICON_PATH}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/app.rc.in"
        "${CMAKE_BINARY_DIR}/generated/app.rc"
        @ONLY)
    list(APPEND TEKLIF_APP_SOURCES "${CMAKE_BINARY_DIR}/generated/app.rc")
endif()

add_executable(teklif_app ${TEKLIF_APP_SOURCES})
target_link_libraries(teklif_app PRIVATE teklif_ui)

# WIN32_EXECUTABLE: program açılırken arkasında siyah konsol penceresi belirmesin.
set_target_properties(teklif_app PROPERTIES WIN32_EXECUTABLE ON)

# tests/ hem teklif_core hem teklif_ui hedeflerini kullanır — ikisi de
# tanımlandıktan SONRA eklenir.
enable_testing()
add_subdirectory(tests)

# Network artık teklif_update tarafından kullanılıyor (bkz. src/update/).
