cmake_minimum_required(VERSION 3.21)
project(TeklifApp VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Dizin düzeni
#
#   include/teklif/<katman>/   genel başlıklar (.h) — bir katmanın dışarıya
#                              açtığı arayüz burada, tek bir ağaçta durur
#   src/<katman>/              gerçekleştirim (.cpp) + o katmanın CMakeLists'i
#   cmake/                     CMake'in doldurduğu şablonlar (version.h, app.rc)
#   tests/                     Qt Test ikilileri
#   resources/                 simge gibi ham kaynaklar
#   packaging/                 Inno Setup kurulum betiği
#
# Başlıkların .cpp'lerden ayrı durmasının pratik faydası: bir katmanın
# arayüzüne bakmak için gerçekleştirim dosyalarının arasında dolaşmak
# gerekmez ve #include yolları her yerde aynı biçimdedir:
#   #include "teklif/core/db.h"
# Dosya nereye taşınırsa taşınsın (aynı katman içinde) yol değişmez.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Başlık ağacının kökü. Katman CMakeLists'leri kendi .h dosyalarını bu
# değişken üzerinden listeler; düzen değişirse tek satır güncellenir.
set(TEKLIF_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/teklif")

# ---------------------------------------------------------------------------
# Derleyici uyarıları. Hiç açık değildi; kullanılmayan değişken, imzalı/
# imzasız karşılaştırma, eksik switch dalı gibi gerçek hataların habercisi
# olan uyarılar sessizce geçiyordu.
#
# Uyarılar HATAYA ÇEVRİLMEZ: Qt başlıkları derleyici sürümüne göre kendi
# uyarılarını üretebilir ve bu, projeyi bizim kodumuzla ilgisi olmayan bir
# sebeple derlenemez hale getirirdi.
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Widgets teklif ekranı, PrintSupport baskı/PDF, Network otomatik güncelleme
# içindir. Hangi katmanın hangisine bağlandığı src/<katman>/CMakeLists.txt'de.
find_package(Qt6 REQUIRED COMPONENTS Core Sql Widgets PrintSupport Network)

# ---------------------------------------------------------------------------
# Sürüm bilgisi: tek doğruluk kaynağı yukarıdaki project(... VERSION ...).
# CI, git tag'inden (v0.2.0) gelen değeri -DTEKLIF_VERSION=0.2.0 ile geçer;
# elle derlerken bu değer verilmezse project() sürümü kullanılır.
# Üretilen version.h build klasörüne yazılır, repoya girmez.
# ---------------------------------------------------------------------------
set(TEKLIF_VERSION "${PROJECT_VERSION}" CACHE STRING "Yayınlanan sürüm numarası")

# Güncelleme manifestinin adresi. Varsayılan olarak BOŞ: adres verilmezse
# program güncelleme aramaz ve menüde ilgili girdi çıkmaz. CI, yayın
# derlemelerinde bunu GitHub Releases adresiyle doldurur.
set(TEKLIF_UPDATE_URL "" CACHE STRING "latest.json adresi")

# Yayıncı adı; installer ve exe özelliklerinde görünür.
set(TEKLIF_PUBLISHER "Karasu Vizyon Doğalgaz" CACHE STRING "Yayıncı adı")

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

# Üretilen başlık, kaynak ağacındakilerle AYNI yol biçimini kullanır
# (teklif/core/version.h) — kodun tarafında üretilmiş olması fark etmez.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/teklif/core/version.h"
    @ONLY)

message(STATUS "TeklifApp sürüm ${TEKLIF_VERSION} (${TEKLIF_GIT_SHA})")

add_subdirectory(src)

# tests/ hem teklif_core hem teklif_ui hedeflerini kullanır — ikisi de
# tanımlandıktan SONRA eklenir.
enable_testing()
add_subdirectory(tests)
