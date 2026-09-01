# ---------------------------------------------------------------------------
# update: otomatik güncelleme. teklif_core'a bağımlı, Widgets'e DEĞİL —
# sunucudaki latest.json'u okuyup sürüm karşılaştıran ve paketi indirip
# doğrulayan kısım arayüzsüz test edilebilsin diye. Kullanıcıya soru soran
# diyalog ui katmanında kalır.
# ---------------------------------------------------------------------------
add_library(teklif_update STATIC
    "${TEKLIF_INCLUDE_DIR}/update/update_info.h"
    "${TEKLIF_INCLUDE_DIR}/update/updater.h"
    update_info.cpp
    updater.cpp
)
target_link_libraries(teklif_update PUBLIC teklif_core Qt6::Network)
