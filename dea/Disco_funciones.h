#pragma once
#include "Disco.h"
#include <string>
using namespace std;
#include <cctype>
// Analiza un string y deduce su tipo real
inline char inferir_tipo_dato(const string& valor) {
    if (valor.empty()) return 'C';

    bool tiene_punto = false;
    bool es_numero = true;

    for (char c : valor) {
        if (c == '-') continue; // Permite numeros negativos
        if (c == '.') {
            if (tiene_punto) { es_numero = false; break; } // Dos puntos = no es numero
            tiene_punto = true;
        }
        else if (!std::isdigit(c)) {
            es_numero = false; break; // Tiene letras = es texto
        }
    }

    if (es_numero) {
        return tiene_punto ? 'F' : 'I';
    }
    return 'C';
}

 Disco inicializar_disco(int cantidad_platos,
    int cantidad_pistas_por_superficie,
    int cantidad_sectores_por_pista,
    int capacidad_bytes_por_sector
);

void liberar_disco(Disco& disco);

Sector* buscar_sector_libre(Disco& disco);

void escribir_registro(Disco& disco, string datos);

DireccionDisco calcular_direccion(int n, int num_platos, int pistas_por_sup, int sectores_por_pista);