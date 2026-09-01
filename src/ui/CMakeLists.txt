# ---------------------------------------------------------------------------
# ui: ekranlar ve bileşenleri. core/print/update'e bağımlı, tersi değil —
# iş mantığı arayüzü hiç bilmez.
#
# Ekranlar (page_*) birbirini tanımaz; aralarındaki haberleşme sinyallerle
# MainWindow üzerinden geçer, böylece bir ekran tek başına değiştirilebilir.
# ---------------------------------------------------------------------------
set(TEKLIF_UI_HEADERS
    "${TEKLIF_INCLUDE_DIR}/ui/dlg_first_run.h"
    "${TEKLIF_INCLUDE_DIR}/ui/dlg_line_entry.h"
    "${TEKLIF_INCLUDE_DIR}/ui/item_search.h"
    "${TEKLIF_INCLUDE_DIR}/ui/item_table_model.h"
    "${TEKLIF_INCLUDE_DIR}/ui/mainwindow.h"
    "${TEKLIF_INCLUDE_DIR}/ui/page_archive.h"
    "${TEKLIF_INCLUDE_DIR}/ui/page_catalog.h"
    "${TEKLIF_INCLUDE_DIR}/ui/page_customers.h"
    "${TEKLIF_INCLUDE_DIR}/ui/page_quote.h"
    "${TEKLIF_INCLUDE_DIR}/ui/page_settings.h"
    "${TEKLIF_INCLUDE_DIR}/ui/quote_line_model.h"
    "${TEKLIF_INCLUDE_DIR}/ui/quote_summary_model.h"
    "${TEKLIF_INCLUDE_DIR}/ui/quote_table_view.h"
    "${TEKLIF_INCLUDE_DIR}/ui/theme.h"
    "${TEKLIF_INCLUDE_DIR}/ui/update_prompt.h"
)

add_library(teklif_ui STATIC
    ${TEKLIF_UI_HEADERS}
    dlg_first_run.cpp
    dlg_line_entry.cpp
    item_search.cpp
    item_table_model.cpp
    mainwindow.cpp
    page_archive.cpp
    page_catalog.cpp
    page_customers.cpp
    page_quote.cpp
    page_settings.cpp
    quote_line_model.cpp
    quote_summary_model.cpp
    quote_table_view.cpp
    theme.cpp
    update_prompt.cpp
)
target_link_libraries(teklif_ui PUBLIC teklif_core teklif_print teklif_update Qt6::Widgets)
