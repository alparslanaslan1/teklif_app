# ---------------------------------------------------------------------------
# core: iş mantığı. Qt Widgets'e bağımlı DEĞİL — arayüz açmadan test edilebilir
# olması ve ileride (gerekirse) başka bir arayüz katmanına taşınabilmesi için.
#
# Başlıklar da kaynak listesine yazılır: AUTOMOC bunları tarayıp Q_OBJECT
# içerenler için moc üretir, ayrıca IDE'lerin proje ağacında görünürler.
# ---------------------------------------------------------------------------
set(TEKLIF_CORE_HEADERS
    "${TEKLIF_INCLUDE_DIR}/core/calculator.h"
    "${TEKLIF_INCLUDE_DIR}/core/csv.h"
    "${TEKLIF_INCLUDE_DIR}/core/db.h"
    "${TEKLIF_INCLUDE_DIR}/core/log.h"
    "${TEKLIF_INCLUDE_DIR}/core/models.h"
    "${TEKLIF_INCLUDE_DIR}/core/money.h"
    "${TEKLIF_INCLUDE_DIR}/core/numparse.h"
    "${TEKLIF_INCLUDE_DIR}/core/numtowords.h"
    "${TEKLIF_INCLUDE_DIR}/core/quote_status.h"
    "${TEKLIF_INCLUDE_DIR}/core/repo_customers.h"
    "${TEKLIF_INCLUDE_DIR}/core/repo_items.h"
    "${TEKLIF_INCLUDE_DIR}/core/repo_quotes.h"
    "${TEKLIF_INCLUDE_DIR}/core/search.h"
    "${TEKLIF_INCLUDE_DIR}/core/settings.h"
    "${TEKLIF_INCLUDE_DIR}/core/transaction.h"
    "${TEKLIF_INCLUDE_DIR}/core/turkish.h"
)

add_library(teklif_core STATIC
    ${TEKLIF_CORE_HEADERS}
    calculator.cpp
    csv.cpp
    db.cpp
    log.cpp
    money.cpp
    numparse.cpp
    numtowords.cpp
    quote_status.cpp
    repo_customers.cpp
    repo_items.cpp
    repo_quotes.cpp
    search.cpp
    settings.cpp
    transaction.cpp
    turkish.cpp
)

# PUBLIC: bu iki dizin, teklif_core'a bağlanan HER hedefe (üst katmanlar ve
# testler) miras kalır — kimse elle include yolu tanımlamak zorunda kalmaz.
# generated/ içinde CMake'in ürettiği teklif/core/version.h vardır.
target_include_directories(teklif_core
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_BINARY_DIR}/generated")

target_link_libraries(teklif_core PUBLIC Qt6::Core Qt6::Sql)
