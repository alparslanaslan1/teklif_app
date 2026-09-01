# ---------------------------------------------------------------------------
# Katmanlar. Sıra bağımlılık sırasıdır: her katman yalnızca kendinden
# öncekilere bağlanabilir, tersi olamaz.
#
#   core   -> iş mantığı (Widgets/Gui YOK)
#   update -> core + Network
#   print  -> core + PrintSupport
#   ui     -> core + print + update + Widgets
# ---------------------------------------------------------------------------
add_subdirectory(core)
add_subdirectory(update)
add_subdirectory(print)
add_subdirectory(ui)

# ---------------------------------------------------------------------------
# Uygulama. Windows'ta simge ve sürüm bilgisi bir kaynak (.rc) dosyasıyla
# exe'ye gömülür; diğer platformlarda bu adım atlanır.
# ---------------------------------------------------------------------------
set(TEKLIF_APP_SOURCES main.cpp)

if(WIN32)
    set(TEKLIF_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/teklif.ico")
    # .rc içinde ters bölü kaçış karakteridir; Windows yolları çevrilmeli.
    string(REPLACE "\\" "\\\\" TEKLIF_ICON_PATH "${TEKLIF_ICON_PATH}")

    # VERSIONINFO virgülle ayrılmış DÖRT sayı ister ("0,1,0,0"); TEKLIF_VERSION
    # ise noktalı üç parçadır. Dördüncü alan build numarası olarak 0 bırakılır.
    string(REPLACE "." "," TEKLIF_VERSION_RC "${TEKLIF_VERSION}")
    set(TEKLIF_VERSION_RC "${TEKLIF_VERSION_RC},0")

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/app.rc.in"
        "${CMAKE_BINARY_DIR}/generated/app.rc"
        @ONLY)
    list(APPEND TEKLIF_APP_SOURCES "${CMAKE_BINARY_DIR}/generated/app.rc")
endif()

add_executable(teklif_app ${TEKLIF_APP_SOURCES})
target_link_libraries(teklif_app PRIVATE teklif_ui)

# WIN32_EXECUTABLE: program açılırken arkasında siyah konsol penceresi belirmesin.
set_target_properties(teklif_app PROPERTIES WIN32_EXECUTABLE ON)

# ---------------------------------------------------------------------------
# exe HER ZAMAN build/bin/ altına çıkar.
#
# Varsayılan davranışta çıktı, hedefin tanımlandığı klasörü taklit eder
# (build/src/...) ve Visual Studio gibi çok yapılandırmalı üreticiler araya
# bir de Release/ ekler. Bu, kaynak ağacının düzenini paketleme betiklerine
# sızdırır: dosyalar taşınınca release.yml'deki kopyalama yolu bozulur.
# Sabitlemek bu bağı koparır — hem "make" hem Visual Studio, hem Debug hem
# Release için yol aynıdır: build/bin/teklif_app(.exe)
# ---------------------------------------------------------------------------
set_target_properties(teklif_app PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(yapilandirma DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(teklif_app PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${yapilandirma} "${CMAKE_BINARY_DIR}/bin")
endforeach()
