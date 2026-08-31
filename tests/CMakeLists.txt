find_package(Qt6 REQUIRED COMPONENTS Test)

function(add_teklif_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE teklif_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# UI testleri teklif_ui'ya (Widgets içerir) bağlanır. Bu ortamda ve
# CI'da gerçek bir ekran olmadan çalışabilsin diye QT_QPA_PLATFORM=offscreen
# ile çalıştırılır — widget'lar gerçekten oluşturulur/etkileşime girer,
# sadece hiçbir pencere yöneticisine ihtiyaç duymazlar.
function(add_teklif_ui_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE teklif_ui Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

# print testleri teklif_print'e (PrintSupport içerir) bağlanır. QPrinter
# gerçek bir yazıcı olmadan da PDF üretebilir (Qt'nin kendi PDF motoru,
# CUPS/sistem yazıcısı gerekmez) — bu yüzden offscreen'e ek bir gereksinim
# yok, ama tutarlılık için aynı ortam değişkeni veriliyor.
function(add_teklif_print_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE teklif_print Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

# update testleri teklif_update'e (Network içerir) bağlanır. Ağa HİÇ çıkmazlar:
# sürüm karşılaştırma, manifest ayrıştırma ve özet doğrulama saf mantıktır,
# gerçek indirme testi CI'da anlamlı olmaz ve kırılgan olur.
function(add_teklif_update_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE teklif_update Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_teklif_test(test_money)
add_teklif_test(test_migration)
add_teklif_test(test_calculator)
add_teklif_test(test_numtowords)
add_teklif_test(test_search)
add_teklif_test(test_repos)
add_teklif_test(test_numparse)
add_teklif_test(test_repo_customers)
add_teklif_test(test_repo_quotes)
add_teklif_test(test_turkish)
add_teklif_test(test_transaction)
add_teklif_test(test_settings)
add_teklif_test(test_archive)
add_teklif_test(test_csv)

add_teklif_ui_test(test_quote_line_model)
add_teklif_ui_test(test_item_search)
add_teklif_ui_test(test_dlg_line_entry)
add_teklif_ui_test(test_quote_table_view)
add_teklif_ui_test(test_page_quote)
add_teklif_ui_test(test_page_archive)
add_teklif_ui_test(test_page_catalog)
add_teklif_ui_test(test_page_settings)
add_teklif_ui_test(test_first_run)

add_teklif_update_test(test_updater)

add_teklif_print_test(test_company_logo)
add_teklif_print_test(test_document_layout)
add_teklif_print_test(test_print_service)
