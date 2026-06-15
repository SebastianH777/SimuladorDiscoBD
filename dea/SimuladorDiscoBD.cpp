#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <optional>
#include "Disco_funciones.h"
#include "Esquema.h"
#include "disco_binario.h"
#include "Busqueda.h"
using namespace std;

int main() {
    sf::RenderWindow window(sf::VideoMode({ 1400, 800 }), "Simulador de Disco BD");
    window.setFramerateLimit(60);
    if (!ImGui::SFML::Init(window)) return -1;
    ImGui::GetIO().Fonts->AddFontDefault();

    sf::Clock clock;

    int cantidad_platos = 2;
    int cantidad_pistas = 3;
    int cantidad_sectores = 4;
    int capacidad_bytes = 32;

    Disco disco = inicializar_disco(cantidad_platos, cantidad_pistas, cantidad_sectores, capacidad_bytes);

    Esquema      esquema;
    ResultadoCSV csv_cargado;
    Indice       indice;

    char ruta_txt_buf[512] = "esquema.txt";
    char ruta_csv_buf[512] = "datos.csv";
    char ruta_export_buf[512] = "salida.csv";
    string mensaje_estado = "Carga primero el esquema SQL (.txt) y luego el CSV.";
    bool   hay_error = false;
    int    total_insertados = 0;
    int    total_fallidos = 0;

    int  col_seleccionada = 0;
    int  tipo_busqueda = 0;
    char buf_valor[256] = "";
    char buf_desde[256] = "";
    char buf_hasta[256] = "";
    vector<ResultadoBusqueda> resultados_busqueda;
    bool busqueda_realizada = false;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
            if (event->is<sf::Event::Closed>()) window.close();
        }
        ImGui::SFML::Update(window, clock.restart());

        // PANEL 1 - Configuracion del disco
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 190), ImGuiCond_FirstUseEver);
        ImGui::Begin("1. Configuracion del Disco");
        ImGui::InputInt("Platos", &cantidad_platos);
        ImGui::InputInt("Pistas/sup", &cantidad_pistas);
        ImGui::InputInt("Sectores/pista", &cantidad_sectores);
        ImGui::InputInt("Bytes/sector", &capacidad_bytes);
        if (cantidad_platos < 1) cantidad_platos = 1;
        if (cantidad_pistas < 1) cantidad_pistas = 1;
        if (cantidad_sectores < 1) cantidad_sectores = 1;
        if (capacidad_bytes < 4) capacidad_bytes = 4;
        if (ImGui::Button("Inicializar Disco", ImVec2(-1, 30))) {
            liberar_disco(disco);
            disco = inicializar_disco(cantidad_platos, cantidad_pistas, cantidad_sectores, capacidad_bytes);
            esquema = Esquema{}; csv_cargado = ResultadoCSV{}; indice = Indice{};
            total_insertados = 0; total_fallidos = 0;
            resultados_busqueda.clear(); busqueda_realizada = false;
            mensaje_estado = "Disco reinicializado."; hay_error = false;
        }
        int total_sectores = cantidad_platos * 2 * cantidad_pistas * cantidad_sectores;
        int usados = 0;
        for (int p = 0; p < disco.cantidad_platos; ++p)
            for (int su = 0; su < 2; ++su)
                for (int pi = 0; pi < disco.platos[p].superficies[su].cantidad_pistas; ++pi)
                    for (int s = 0; s < disco.platos[p].superficies[su].pistas[pi].cantidad_sectores; ++s)
                        if (!disco.platos[p].superficies[su].pistas[pi].sectores[s].libre) ++usados;
        ImGui::Separator();
        ImGui::Text("Sectores: %d / %d usados", usados, total_sectores);
        ImGui::ProgressBar((float)usados / total_sectores, ImVec2(-1, 12));
        ImGui::End();

        // PANEL 2 - Esquema SQL
        ImGui::SetNextWindowPos(ImVec2(10, 210), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 220), ImGuiCond_FirstUseEver);
        ImGui::Begin("2. Esquema SQL (.txt)");
        ImGui::Text("Archivo CREATE TABLE:");
        ImGui::InputText("##txt", ruta_txt_buf, sizeof(ruta_txt_buf));
        if (ImGui::Button("Cargar Esquema SQL", ImVec2(-1, 28))) {
            ResultadoTXT res = cargar_esquema_sql(ruta_txt_buf);
            if (!res.error.empty()) { mensaje_estado = "ERROR: " + res.error; hay_error = true; }
            else {
                esquema = res.esquema; csv_cargado = ResultadoCSV{}; indice = Indice{};
                total_insertados = 0; total_fallidos = 0;
                resultados_busqueda.clear(); busqueda_realizada = false; col_seleccionada = 0;
                mensaje_estado = "Esquema cargado: " + esquema.descripcion(); hay_error = false;
            }
        }
        if (!esquema.vacio()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1), "Tabla: %s", esquema.nombre_tabla.c_str());
            for (const auto& col : esquema.columnas) {
                const char* t = col.tipo == 'I' ? "INT" : col.tipo == 'F' ? "FLOAT" : col.tipo == 'B' ? "BOOL" : "VARCHAR";
                ImGui::BulletText("%s -> %s (%dB)", col.nombre.c_str(), t, col.tamano_bytes);
            }
        }
        ImGui::End();

        // PANEL 3 - Datos CSV
        ImGui::SetNextWindowPos(ImVec2(10, 440), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin("3. Datos CSV");
        ImGui::InputText("##csv", ruta_csv_buf, sizeof(ruta_csv_buf));
        if (ImGui::Button("Cargar e Importar CSV", ImVec2(-1, 28))) {
            if (esquema.vacio()) { mensaje_estado = "ERROR: Carga primero el esquema."; hay_error = true; }
            else {
                csv_cargado = cargar_csv(ruta_csv_buf);
                if (!csv_cargado.error.empty()) { mensaje_estado = "ERROR: " + csv_cargado.error; hay_error = true; }
                else {
                    total_insertados = 0; total_fallidos = 0;
                    for (const auto& fila : csv_cargado.filas) {
                        try { escribir_registro_binario(disco, esquema, fila); ++total_insertados; }
                        catch (...) { ++total_fallidos; }
                    }
                    // Construir indice automaticamente al importar
                    indice = construir_indice(disco, esquema);
                    resultados_busqueda.clear(); busqueda_realizada = false;
                    mensaje_estado = "Importados: " + to_string(total_insertados)
                        + "  Fallidos: " + to_string(total_fallidos)
                        + "  | Indice construido.";
                    hay_error = (total_fallidos > 0);
                }
            }
        }
        if (total_insertados > 0)
            ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "%d registros | indice: %s",
                total_insertados, indice.vacio() ? "NO" : "SI");
        ImGui::End();

        // PANEL 4 - Exportar
        ImGui::SetNextWindowPos(ImVec2(10, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 80), ImGuiCond_FirstUseEver);
        ImGui::Begin("4. Exportar a CSV");
        ImGui::InputText("##exp", ruta_export_buf, sizeof(ruta_export_buf));
        if (ImGui::Button("Exportar CSV", ImVec2(-1, 28))) {
            if (esquema.vacio()) { mensaje_estado = "ERROR: No hay esquema."; hay_error = true; }
            else {
                try {
                    exportar_disco_a_csv(disco, esquema, ruta_export_buf);
                    mensaje_estado = string("Exportado a: ") + ruta_export_buf; hay_error = false;
                }
                catch (const exception& e) { mensaje_estado = string("ERROR: ") + e.what(); hay_error = true; }
            }
        }
        ImGui::End();

        // PANEL 5 - Busqueda
        ImGui::SetNextWindowPos(ImVec2(10, 690), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 290), ImGuiCond_FirstUseEver);
        ImGui::Begin("5. Busqueda");

        if (indice.vacio()) {
            ImGui::TextDisabled("Carga datos primero para habilitar la busqueda.");
        }
        else {
            // Selector de columna
            ImGui::Text("Columna:");
            vector<const char*> nombres_col;
            for (const auto& c : esquema.columnas) nombres_col.push_back(c.nombre.c_str());
            ImGui::Combo("##col", &col_seleccionada, nombres_col.data(), (int)nombres_col.size());

            // Tipo
            ImGui::Text("Tipo:");
            ImGui::RadioButton("Exacta", &tipo_busqueda, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Rango", &tipo_busqueda, 1);
            ImGui::Separator();

            if (tipo_busqueda == 0) {
                ImGui::Text("Valor:");
                ImGui::InputText("##val", buf_valor, sizeof(buf_valor));
                if (ImGui::Button("Buscar", ImVec2(-1, 30))) {
                    resultados_busqueda = buscar_exacto(indice, esquema, col_seleccionada, buf_valor);
                    busqueda_realizada = true;
                    mensaje_estado = "Exacta: " + to_string(resultados_busqueda.size()) + " resultado(s).";
                    hay_error = false;
                }
            }
            else {
                ImGui::Text("Desde:"); ImGui::InputText("##desde", buf_desde, sizeof(buf_desde));
                ImGui::Text("Hasta:"); ImGui::InputText("##hasta", buf_hasta, sizeof(buf_hasta));
                if (ImGui::Button("Buscar Rango", ImVec2(-1, 30))) {
                    resultados_busqueda = buscar_rango(indice, esquema, col_seleccionada, buf_desde, buf_hasta);
                    busqueda_realizada = true;
                    mensaje_estado = "Rango: " + to_string(resultados_busqueda.size()) + " resultado(s).";
                    hay_error = false;
                }
            }

            if (busqueda_realizada) {
                ImGui::Separator();
                if (resultados_busqueda.empty())
                    ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "Sin resultados.");
                else
                    ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1),
                        "%d resultado(s)", (int)resultados_busqueda.size());
            }
            if (ImGui::Button("Limpiar", ImVec2(-1, 22))) {
                resultados_busqueda.clear(); busqueda_realizada = false;
                buf_valor[0] = buf_desde[0] = buf_hasta[0] = '\0';
            }
        }
        ImGui::End();

        // PANEL 6 - Estado
        ImGui::SetNextWindowPos(ImVec2(10, 788), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 24), ImGuiCond_FirstUseEver);
        ImGui::Begin("Estado", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        if (hay_error)
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", mensaje_estado.c_str());
        else
            ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "%s", mensaje_estado.c_str());
        ImGui::End();

        // PANEL 7 - Estado del disco
        ImGui::SetNextWindowPos(ImVec2(340, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(600, 490), ImGuiCond_FirstUseEver);
        ImGui::Begin("Estado del Disco");
        if (ImGui::BeginTable("sectores", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Plato", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Sup", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableSetupColumn("Pista", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Sector", ImGuiTableColumnFlags_WidthFixed, 55);
            ImGui::TableSetupColumn("Cont.", ImGuiTableColumnFlags_WidthFixed, 45);
            ImGui::TableSetupColumn("Sig.", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (int p = 0; p < disco.cantidad_platos; ++p)
                for (int su = 0; su < 2; ++su) {
                    Superficie& sup = disco.platos[p].superficies[su];
                    for (int pi = 0; pi < sup.cantidad_pistas; ++pi) {
                        Pista& pista = sup.pistas[pi];
                        for (int s = 0; s < pista.cantidad_sectores; ++s) {
                            Sector& sec = pista.sectores[s];
                            if (sec.libre) continue;
                            ImGui::TableNextRow();
                            if (!sec.es_continuacion)
                                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(40, 80, 40, 255));
                            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", sec.direccion.posicion_plato);
                            ImGui::TableSetColumnIndex(1); ImGui::Text("%d", sec.direccion.posicion_superficie);
                            ImGui::TableSetColumnIndex(2); ImGui::Text("%d", sec.direccion.posicion_pista);
                            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", sec.direccion.posicion_sector);
                            ImGui::TableSetColumnIndex(4); ImGui::Text("%s", sec.es_continuacion ? "SI" : "NO");
                            ImGui::TableSetColumnIndex(5);
                            if (sec.siguiente_sector) {
                                DireccionDisco& d = sec.siguiente_sector->direccion;
                                ImGui::Text("%d-%d-%d-%d", d.posicion_plato, d.posicion_superficie,
                                    d.posicion_pista, d.posicion_sector);
                            }
                            else ImGui::Text("fin");
                        }
                    }
                }
            ImGui::EndTable();
        }
        ImGui::End();

        // PANEL 8 - Resultados de busqueda
        ImGui::SetNextWindowPos(ImVec2(340, 510), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1050, 280), ImGuiCond_FirstUseEver);
        ImGui::Begin("Resultados de Busqueda");
        if (!busqueda_realizada) {
            ImGui::TextDisabled("Realiza una busqueda para ver resultados aqui.");
        }
        else if (resultados_busqueda.empty()) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "No se encontraron registros.");
        }
        else {
            int num_cols = (int)esquema.columnas.size() + 4;
            if (ImGui::BeginTable("resultados", num_cols,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                for (const auto& col : esquema.columnas)
                    ImGui::TableSetupColumn(col.nombre.c_str(), ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Plato", ImGuiTableColumnFlags_WidthFixed, 45);
                ImGui::TableSetupColumn("Sup", ImGuiTableColumnFlags_WidthFixed, 35);
                ImGui::TableSetupColumn("Pista", ImGuiTableColumnFlags_WidthFixed, 45);
                ImGui::TableSetupColumn("Sector", ImGuiTableColumnFlags_WidthFixed, 50);
                ImGui::TableHeadersRow();

                for (const auto& r : resultados_busqueda) {
                    ImGui::TableNextRow();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(20, 60, 20, 255));
                    for (int c = 0; c < (int)r.valores.size(); ++c) {
                        ImGui::TableSetColumnIndex(c);
                        if (c == col_seleccionada)
                            ImGui::TextColored(ImVec4(1, 1, 0.3f, 1), "%s", r.valores[c].c_str());
                        else
                            ImGui::Text("%s", r.valores[c].c_str());
                    }
                    int base = (int)esquema.columnas.size();
                    ImGui::TableSetColumnIndex(base);   ImGui::Text("%d", r.plato);
                    ImGui::TableSetColumnIndex(base + 1); ImGui::Text("%d", r.superficie);
                    ImGui::TableSetColumnIndex(base + 2); ImGui::Text("%d", r.pista);
                    ImGui::TableSetColumnIndex(base + 3); ImGui::Text("%d", r.sector);
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();

        window.clear(sf::Color(20, 20, 20));
        ImGui::SFML::Render(window);
        window.display();
    }

    liberar_disco(disco);
    ImGui::SFML::Shutdown();
    return 0;
}