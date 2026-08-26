cmake_minimum_required(VERSION 3.21)
project(TeklifApp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Part 4'te Widgets girdi (teklif ekranı). Part 5'te PrintSupport giriyor
# (baskı/PDF). Network (Part 8) hâlâ ilgili parçasında eklenecek.
find_package(Qt6 REQUIRED COMPONENTS Core Sql Widgets PrintSupport)

# ---------------------------------------------------------------------------
# core: iş mantığı. Qt Widgets'e bağımlı DEĞİL — arayüz açmadan test edilebilir
# olması ve ileride (gerekirse) başka bir arayüz katmanına taşınabilmesi için.
# ---------------------------------------------------------------------------
add_library(teklif_core STATIC
    src/core/money.h
    src/core/money.cpp
    src/core/db.h
    src/core/db.cpp
    src/core/calculator.h
    src/core/calculator.cpp
    src/core/numtowords.h
    src/core/numtowords.cpp
    src/core/models.h
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
target_include_directories(teklif_core PUBLIC src)
target_link_libraries(teklif_core PUBLIC Qt6::Core Qt6::Sql)

# ---------------------------------------------------------------------------
# print: baskı/PDF. teklif_core'a bağımlı; ekran (page_quote) ve gerçek
# yazıcı/PDF AYNI DocumentLayout/PrintService kodunu kullanır.
# ---------------------------------------------------------------------------
add_library(teklif_print STATIC
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
)
target_link_libraries(teklif_ui PUBLIC teklif_core teklif_print Qt6::Widgets)

add_executable(teklif_app src/main.cpp)
target_link_libraries(teklif_app PRIVATE teklif_ui)

# tests/ hem teklif_core hem teklif_ui hedeflerini kullanır — ikisi de
# tanımlandıktan SONRA eklenir.
enable_testing()
add_subdirectory(tests)

# Network (Part 8) hâlâ eklenecek.
