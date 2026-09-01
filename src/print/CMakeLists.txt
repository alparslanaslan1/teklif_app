# ---------------------------------------------------------------------------
# print: baskı/PDF. teklif_core'a bağımlı; ekran (page_quote) ve gerçek
# yazıcı/PDF AYNI DocumentLayout/PrintService kodunu kullanır.
# ---------------------------------------------------------------------------
add_library(teklif_print STATIC
    "${TEKLIF_INCLUDE_DIR}/print/company_logo.h"
    "${TEKLIF_INCLUDE_DIR}/print/document_layout.h"
    "${TEKLIF_INCLUDE_DIR}/print/print_service.h"
    company_logo.cpp
    document_layout.cpp
    print_service.cpp
)
target_link_libraries(teklif_print PUBLIC teklif_core Qt6::PrintSupport)
