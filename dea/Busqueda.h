#pragma once
#include "Disco.h"
#include "Esquema.h"
#include "disco_binario.h"
#include <string>
#include <vector>
#include <map>

// ============================================================
//  RESULTADO DE BUSQUEDA
// ============================================================

struct ResultadoBusqueda {
    std::vector<std::string> valores;
    int plato, superficie, pista, sector;
};

// ============================================================
//  INDICE CON TIPO CORRECTO POR COLUMNA
//  - VARCHAR/BOOL → map<string,  ...>
//  - INT          → map<int,     ...>
//  - FLOAT        → map<double,  ...>
//  Cada columna tiene exactamente uno de los tres maps activo.
// ============================================================

struct IndiceColumna {
    char tipo; // 'I', 'F', 'C', 'B'
    std::map<std::string, std::vector<Sector*>> idx_str;
    std::map<int,         std::vector<Sector*>> idx_int;
    std::map<double,      std::vector<Sector*>> idx_flt;
};

struct Indice {
    std::vector<IndiceColumna> columnas;
    bool vacio() const { return columnas.empty(); }
};

// ============================================================
//  LEER UN REGISTRO COMPLETO DESDE UN SECTOR CABEZA
// ============================================================

static bool leer_registro(Sector* cabeza, const Esquema& esquema,
                           std::vector<std::string>& out)
{
    LectorSectores lector(cabeza);
    out.clear();
    for (const auto& col : esquema.columnas) {
        if (col.tipo == 'I') {
            int v = 0;
            if (!lector.leer(reinterpret_cast<char*>(&v), sizeof(int), true)) return false;
            out.push_back(std::to_string(v));
        } else if (col.tipo == 'F') {
            float v = 0.0f;
            if (!lector.leer(reinterpret_cast<char*>(&v), sizeof(float), true)) return false;
            char buf[32]; snprintf(buf, sizeof(buf), "%.2f", v);
            out.push_back(buf);
        } else if (col.tipo == 'B') {
            char v = 0;
            if (!lector.leer(&v, 1, true)) return false;
            out.push_back(v ? "true" : "false");
        } else {
            std::vector<char> buf(col.tamano_bytes + 1, '\0');
            if (!lector.leer(buf.data(), col.tamano_bytes, false)) return false;
            out.push_back(std::string(buf.data()));
        }
    }
    return true;
}

// ============================================================
//  CONSTRUIR INDICE  (una sola pasada por el disco)
// ============================================================

Indice construir_indice(Disco& disco, const Esquema& esquema) {
    Indice idx;
    idx.columnas.resize(esquema.columnas.size());
    for (int c = 0; c < (int)esquema.columnas.size(); ++c)
        idx.columnas[c].tipo = esquema.columnas[c].tipo;

    for (int p = 0; p < disco.cantidad_platos; ++p)
        for (int su = 0; su < 2; ++su) {
            Superficie& sup = disco.platos[p].superficies[su];
            for (int pi = 0; pi < sup.cantidad_pistas; ++pi) {
                Pista& pista = sup.pistas[pi];
                for (int s = 0; s < pista.cantidad_sectores; ++s) {
                    Sector& sec = pista.sectores[s];
                    if (sec.libre || sec.es_continuacion) continue;

                    std::vector<std::string> vals;
                    if (!leer_registro(&sec, esquema, vals)) continue;

                    for (int c = 0; c < (int)vals.size(); ++c) {
                        IndiceColumna& ic = idx.columnas[c];
                        try {
                            if (ic.tipo == 'I')
                                ic.idx_int[std::stoi(vals[c])].push_back(&sec);
                            else if (ic.tipo == 'F')
                                ic.idx_flt[std::stod(vals[c])].push_back(&sec);
                            else
                                ic.idx_str[vals[c]].push_back(&sec);
                        } catch (...) {
                            ic.idx_str[vals[c]].push_back(&sec); // fallback
                        }
                    }
                }
            }
        }
    return idx;
}

// ============================================================
//  CONVERTIR SECTOR* A ResultadoBusqueda
// ============================================================

static ResultadoBusqueda sector_a_resultado(Sector* sec, const Esquema& esquema) {
    ResultadoBusqueda r;
    r.plato      = sec->direccion.posicion_plato;
    r.superficie = sec->direccion.posicion_superficie;
    r.pista      = sec->direccion.posicion_pista;
    r.sector     = sec->direccion.posicion_sector;
    leer_registro(sec, esquema, r.valores);
    return r;
}

// ============================================================
//  BUSQUEDA EXACTA  O(log n)
// ============================================================

std::vector<ResultadoBusqueda> buscar_exacto(
    const Indice& indice,
    const Esquema& esquema,
    int col,
    const std::string& valor)
{
    std::vector<ResultadoBusqueda> res;
    if (col < 0 || col >= (int)indice.columnas.size()) return res;
    const IndiceColumna& ic = indice.columnas[col];

    auto agregar = [&](const std::vector<Sector*>& secs) {
        for (Sector* s : secs) res.push_back(sector_a_resultado(s, esquema));
    };

    try {
        if (ic.tipo == 'I') {
            auto it = ic.idx_int.find(std::stoi(valor));
            if (it != ic.idx_int.end()) agregar(it->second);
        } else if (ic.tipo == 'F') {
            auto it = ic.idx_flt.find(std::stod(valor));
            if (it != ic.idx_flt.end()) agregar(it->second);
        } else {
            auto it = ic.idx_str.find(valor);
            if (it != ic.idx_str.end()) agregar(it->second);
        }
    } catch (...) {}
    return res;
}

// ============================================================
//  BUSQUEDA POR RANGO  O(log n + k resultados)
// ============================================================

std::vector<ResultadoBusqueda> buscar_rango(
    const Indice& indice,
    const Esquema& esquema,
    int col,
    const std::string& desde,
    const std::string& hasta)
{
    std::vector<ResultadoBusqueda> res;
    if (col < 0 || col >= (int)indice.columnas.size()) return res;
    const IndiceColumna& ic = indice.columnas[col];

    auto agregar_rango = [&](auto& mapa, auto lo, auto hi) {
        // lower_bound y upper_bound usan el tipo nativo del map (int, double o string)
        auto it_lo = mapa.lower_bound(lo);
        auto it_hi = mapa.upper_bound(hi);
        for (auto it = it_lo; it != it_hi; ++it)
            for (Sector* s : it->second)
                res.push_back(sector_a_resultado(s, esquema));
    };

    try {
        if (ic.tipo == 'I')
            agregar_rango(ic.idx_int, std::stoi(desde), std::stoi(hasta));
        else if (ic.tipo == 'F')
            agregar_rango(ic.idx_flt, std::stod(desde), std::stod(hasta));
        else
            agregar_rango(ic.idx_str, desde, hasta);
    } catch (...) {}
    return res;
}
