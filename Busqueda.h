#pragma once
#include "Disco.h"
#include "Esquema.h"
#include "disco_binario.h"
#include "AVL.h"
#include <string>
#include <vector>

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
    char tipo = '\0';

    AVL<int>         avl_int;
    AVL<double>      avl_flt;
    AVL<std::string> avl_str;

    Node<int>* root_int = nullptr;
    Node<double>* root_flt = nullptr;
    Node<std::string>* root_str = nullptr;

    IndiceColumna() = default;
    IndiceColumna(const IndiceColumna&) = delete;
    IndiceColumna& operator=(const IndiceColumna&) = delete;
    IndiceColumna(IndiceColumna&&) = default;
    IndiceColumna& operator=(IndiceColumna&&) = default;
};

struct Indice {
    std::vector<IndiceColumna> columnas;
    bool vacio() const { return columnas.empty(); }

    // Deshabilitar copia
    Indice() = default;
    Indice(const Indice&) = delete;
    Indice& operator=(const Indice&) = delete;

    // Habilitar movimiento
    Indice(Indice&&) = default;
    Indice& operator=(Indice&&) = default;

    ~Indice() {
        for (auto& ic : columnas) {
            ic.avl_int.liberar(ic.root_int);
            ic.avl_flt.liberar(ic.root_flt);
            ic.avl_str.liberar(ic.root_str);
        }
    }
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
                                ic.avl_int.insertar(std::stoi(vals[c]), &sec, ic.root_int);
                            else if (ic.tipo == 'F')
                                ic.avl_flt.insertar(std::stod(vals[c]), &sec, ic.root_flt);
                            else
                                ic.avl_str.insertar(vals[c], &sec, ic.root_str);
                        } catch (...) {
                            ic.avl_str.insertar(vals[c], &sec, ic.root_str); // fallback
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

    // Necesita ser no-const para llamar al AVL
    IndiceColumna& ic = const_cast<IndiceColumna&>(indice.columnas[col]);

    std::vector<Sector*>* sectores = nullptr;

    try {
        if (ic.tipo == '\0') return res;
        if (ic.tipo == 'I')
            sectores = ic.avl_int.buscar(std::stoi(valor), ic.root_int);
        else if (ic.tipo == 'F')
            sectores = ic.avl_flt.buscar(std::stod(valor), ic.root_flt);
        else
            sectores = ic.avl_str.buscar(valor, ic.root_str);
    }
    catch (...) { return res; }

    if (!sectores) return res;

    for (Sector* s : *sectores)
        res.push_back(sector_a_resultado(s, esquema));

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

    IndiceColumna& ic = const_cast<IndiceColumna&>(indice.columnas[col]);

    std::vector<Sector*> sectores;

    try {
        if (ic.tipo == 'I')
            ic.avl_int.buscar_rango(std::stoi(desde), std::stoi(hasta), ic.root_int, sectores);
        else if (ic.tipo == 'F')
            ic.avl_flt.buscar_rango(std::stod(desde), std::stod(hasta), ic.root_flt, sectores);
        else
            ic.avl_str.buscar_rango(desde, hasta, ic.root_str, sectores);
    }
    catch (...) { return res; }

    for (Sector* s : sectores)
        res.push_back(sector_a_resultado(s, esquema));

    return res;
}