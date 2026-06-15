#pragma once
#include "Disco.h"
#include "Disco_funciones.h"
#include "Esquema.h"
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <fstream>
#include <string>
#include <vector>

// ============================================================
//  ESCRITOR DE SECTORES
// ============================================================

struct EscritorSectores {
    Disco& disco;
    Sector* sector_actual = nullptr;
    int offset = 0;

    EscritorSectores(Disco& d) : disco(d) {}

    bool siguiente_sector_libre(Sector* anterior) {
        Sector* nuevo = buscar_sector_libre(disco);
        if (!nuevo) return false;
        nuevo->libre          = false;
        nuevo->es_continuacion = true;
        nuevo->siguiente_sector = nullptr;
        if (anterior) anterior->siguiente_sector = nuevo;
        sector_actual = nuevo;
        offset = 0;
        return true;
    }

    bool escribir(const char* src, int n, bool es_atomico) {
        if (!sector_actual) return false;
        if (es_atomico && (sector_actual->capacidad_bytes - offset) < n) {
            if (!siguiente_sector_libre(sector_actual)) return false;
        }
        int escrito = 0;
        while (escrito < n) {
            int espacio = sector_actual->capacidad_bytes - offset;
            if (espacio == 0) {
                if (!siguiente_sector_libre(sector_actual)) return false;
                espacio = sector_actual->capacidad_bytes;
            }
            int a = std::min(espacio, n - escrito);
            std::memcpy(sector_actual->datos + offset, src + escrito, a);
            offset  += a;
            escrito += a;
        }
        return true;
    }
};

// ============================================================
//  CONVERSION DE STRING A BOOLEANO
// ============================================================

static bool parse_bool(const std::string& val) {
    std::string u;
    for (char c : val) u += std::toupper((unsigned char)c);
    u = trim(u);
    return (u == "TRUE" || u == "SI" || u == "S" || u == "1" || u == "YES" || u == "Y");
}

// ============================================================
//  ESCRITURA BINARIA DE UN REGISTRO
// ============================================================

void escribir_registro_binario(Disco& disco,
                               const Esquema& esquema,
                               const std::vector<std::string>& valores)
{
    if (valores.size() < esquema.columnas.size())
        throw std::invalid_argument("Faltan valores para algunas columnas.");

    Sector* cabeza = buscar_sector_libre(disco);
    if (!cabeza) throw std::runtime_error("Disco lleno.");
    cabeza->libre           = false;
    cabeza->es_continuacion = false;
    cabeza->siguiente_sector = nullptr;

    EscritorSectores escritor(disco);
    escritor.sector_actual = cabeza;
    escritor.offset        = 0;

    for (int c = 0; c < (int)esquema.columnas.size(); ++c) {
        const Columna& col = esquema.columnas[c];
        const std::string& val = valores[c];

        if (col.tipo == 'I') {
            int v = val.empty() ? 0 : std::stoi(val);
            if (!escritor.escribir(reinterpret_cast<char*>(&v), sizeof(int), true))
                throw std::runtime_error("Disco lleno (INT).");

        } else if (col.tipo == 'F') {
            float v = val.empty() ? 0.0f : std::stof(val);
            if (!escritor.escribir(reinterpret_cast<char*>(&v), sizeof(float), true))
                throw std::runtime_error("Disco lleno (FLOAT).");

        } else if (col.tipo == 'B') {
            char v = parse_bool(val) ? 1 : 0;
            if (!escritor.escribir(&v, 1, true))
                throw std::runtime_error("Disco lleno (BOOL).");

        } else { // 'C' VARCHAR
            std::vector<char> buf(col.tamano_bytes, '\0');
            int len = std::min((int)val.size(), col.tamano_bytes - 1);
            std::memcpy(buf.data(), val.c_str(), len);
            if (!escritor.escribir(buf.data(), col.tamano_bytes, false))
                throw std::runtime_error("Disco lleno (VARCHAR).");
        }
    }
}

// ============================================================
//  LECTOR DE SECTORES
// ============================================================

struct LectorSectores {
    Sector* sector_actual;
    int offset;

    LectorSectores(Sector* inicio) : sector_actual(inicio), offset(0) {}

    int bytes_restantes() const {
        return sector_actual ? sector_actual->capacidad_bytes - offset : 0;
    }

    void avanzar() {
        if (sector_actual) sector_actual = sector_actual->siguiente_sector;
        offset = 0;
    }

    bool leer(char* dest, int n, bool es_atomico) {
        if (!sector_actual) return false;
        if (es_atomico && bytes_restantes() < n) { avanzar(); if (!sector_actual) return false; }
        int leido = 0;
        while (leido < n) {
            if (!sector_actual) return false;
            if (bytes_restantes() == 0) { avanzar(); continue; }
            int a = std::min(bytes_restantes(), n - leido);
            std::memcpy(dest + leido, sector_actual->datos + offset, a);
            offset += a; leido += a;
            if (offset >= sector_actual->capacidad_bytes) avanzar();
        }
        return true;
    }
};

// ============================================================
//  EXPORTACION A CSV
// ============================================================

void exportar_disco_a_csv(Disco& disco,
                          const Esquema& esquema,
                          const std::string& ruta_archivo)
{
    if (esquema.vacio()) throw std::invalid_argument("Esquema vacio.");

    std::ofstream archivo(ruta_archivo);
    if (!archivo.is_open()) throw std::runtime_error("No se pudo crear: " + ruta_archivo);

    // Cabecera
    for (int i = 0; i < (int)esquema.columnas.size(); ++i) {
        archivo << esquema.columnas[i].nombre;
        if (i + 1 < (int)esquema.columnas.size()) archivo << ',';
    }
    archivo << '\n';

    for (int p = 0; p < disco.cantidad_platos; ++p) {
        for (int sup = 0; sup < 2; ++sup) {
            Superficie& superficie = disco.platos[p].superficies[sup];
            for (int pi = 0; pi < superficie.cantidad_pistas; ++pi) {
                Pista& pista = superficie.pistas[pi];
                for (int s = 0; s < pista.cantidad_sectores; ++s) {
                    Sector& sec = pista.sectores[s];
                    if (sec.libre || sec.es_continuacion) continue;

                    LectorSectores lector(&sec);
                    bool ok = true;

                    for (int c = 0; c < (int)esquema.columnas.size() && ok; ++c) {
                        const Columna& col = esquema.columnas[c];
                        if (c > 0) archivo << ',';

                        if (col.tipo == 'I') {
                            int v = 0;
                            ok = lector.leer(reinterpret_cast<char*>(&v), sizeof(int), true);
                            if (ok) archivo << v;

                        } else if (col.tipo == 'F') {
                            float v = 0.0f;
                            ok = lector.leer(reinterpret_cast<char*>(&v), sizeof(float), true);
                            if (ok) { archivo << std::fixed; archivo.precision(2); archivo << v; }

                        } else if (col.tipo == 'B') {
                            char v = 0;
                            ok = lector.leer(&v, 1, true);
                            if (ok) archivo << (v ? "true" : "false");

                        } else { // VARCHAR
                            std::vector<char> buf(col.tamano_bytes + 1, '\0');
                            ok = lector.leer(buf.data(), col.tamano_bytes, false);
                            if (ok) {
                                std::string val(buf.data());
                                if (val.find(',') != std::string::npos || val.find('"') != std::string::npos) {
                                    archivo << '"';
                                    for (char ch : val) { if (ch == '"') archivo << '"'; archivo << ch; }
                                    archivo << '"';
                                } else {
                                    archivo << val;
                                }
                            }
                        }
                    }
                    if (ok) archivo << '\n';
                }
            }
        }
    }
    archivo.close();
}
