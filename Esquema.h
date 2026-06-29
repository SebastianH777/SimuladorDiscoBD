#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

// ============================================================
//  TIPOS DE DATOS SOPORTADOS
//  I = INT (4 bytes)
//  F = FLOAT (4 bytes)
//  C = VARCHAR/CHAR (N bytes)
//  B = BOOLEAN (1 byte)
// ============================================================

struct Columna {
    std::string nombre;
    char tipo;
    int tamano_bytes;
    std::string tipo_sql; // nombre original del SQL, ej: "VARCHAR(50)", "INT"
};

struct Esquema {
    std::string nombre_tabla;
    std::vector<Columna> columnas;

    bool vacio() const { return columnas.empty(); }

    int tamano_registro() const {
        int total = 0;
        for (const auto& c : columnas) total += c.tamano_bytes;
        return total;
    }

    std::string descripcion() const {
        std::string s = nombre_tabla + ": ";
        for (const auto& c : columnas)
            s += c.nombre + "(" + c.tipo_sql + ") ";
        return s;
    }
};

// ============================================================
//  UTILIDADES DE STRING
// ============================================================

static std::string trim(const std::string& s) {
    int i = 0, j = (int)s.size() - 1;
    while (i <= j && std::isspace((unsigned char)s[i])) ++i;
    while (j >= i && std::isspace((unsigned char)s[j])) --j;
    return s.substr(i, j - i + 1);
}

static std::string to_upper(std::string s) {
    for (char& c : s) c = std::toupper((unsigned char)c);
    return s;
}

// ============================================================
//  LECTOR DE ARCHIVO TXT CON CREATE TABLE SQL
//  Formato esperado:
//    CREATE TABLE nombre_tabla (
//        columna1 INT,
//        columna2 VARCHAR(50),
//        columna3 FLOAT,
//        columna4 BOOLEAN
//    );
// ============================================================

struct ResultadoTXT {
    Esquema esquema;
    std::string error;
};

ResultadoTXT cargar_esquema_sql(const std::string& ruta) {
    ResultadoTXT res;
    std::ifstream archivo(ruta);
    if (!archivo.is_open()) {
        res.error = "No se pudo abrir: " + ruta;
        return res;
    }

    // Leer todo el contenido
    std::string contenido((std::istreambuf_iterator<char>(archivo)),std::istreambuf_iterator<char>());

    // Quitar BOM UTF-8
    if (contenido.size() >= 3 && (unsigned char)contenido[0] == 0xEF && (unsigned char)contenido[1] == 0xBB && 
        (unsigned char)contenido[2] == 0xBF) contenido = contenido.substr(3);

    std::string upper = to_upper(contenido);

    // Buscar CREATE TABLE
    size_t pos_create = upper.find("CREATE TABLE");
    if (pos_create == std::string::npos) {
        res.error = "No se encontro CREATE TABLE en el archivo.";
        return res;
    }

    // Extraer nombre de tabla
    size_t pos_after_table = pos_create + 12; // len("CREATE TABLE")
    size_t pos_paren = contenido.find('(', pos_after_table);
    if (pos_paren == std::string::npos) {
        res.error = "Formato invalido: falta '(' despues del nombre de tabla.";
        return res;
    }
    res.esquema.nombre_tabla = trim(contenido.substr(pos_after_table, pos_paren - pos_after_table));

    // Extraer contenido entre parentesis
    size_t pos_close = contenido.rfind(')');
    if (pos_close == std::string::npos || pos_close <= pos_paren) {
        res.error = "Formato invalido: falta ')' de cierre.";
        return res;
    }
    std::string cuerpo = contenido.substr(pos_paren + 1, pos_close - pos_paren - 1);

    // Parsear cada linea como una columna
    std::istringstream stream(cuerpo);
    std::string linea;
    while (std::getline(stream, linea)) {
        linea = trim(linea);
        // Quitar coma final
        if (!linea.empty() && linea.back() == ',')
            linea.pop_back();
        linea = trim(linea);
        if (linea.empty()) continue;

        // Separar nombre y tipo (primera palabra = nombre, resto = tipo)
        size_t espacio = linea.find(' ');
        if (espacio == std::string::npos) continue;

        std::string nombre_col = trim(linea.substr(0, espacio));
        std::string tipo_sql   = trim(linea.substr(espacio + 1));
        std::string tipo_upper = to_upper(tipo_sql);

        if (nombre_col.empty() || tipo_sql.empty()) continue;

        Columna col;
        col.nombre   = nombre_col;
        col.tipo_sql = tipo_sql;

        if (tipo_upper == "INT" || tipo_upper == "INTEGER" ||
            tipo_upper == "SMALLINT" || tipo_upper == "BIGINT") {
            col.tipo         = 'I';
            col.tamano_bytes = 4;

        } else if (tipo_upper == "FLOAT" || tipo_upper == "DOUBLE" || tipo_upper == "REAL"  || tipo_upper == "DECIMAL" ||
                   tipo_upper.find("NUMERIC") != std::string::npos) {
            col.tipo         = 'F';
            col.tamano_bytes = 4;

        } else if (tipo_upper == "BOOLEAN" || tipo_upper == "BOOL") {
            col.tipo         = 'B';
            col.tamano_bytes = 1;

        } else if (tipo_upper.find("VARCHAR") != std::string::npos ||
                   tipo_upper.find("CHAR")    != std::string::npos ||
                   tipo_upper.find("TEXT")    != std::string::npos ||
                   tipo_upper.find("DATE")    != std::string::npos ||
                   tipo_upper.find("TIME")    != std::string::npos) {
            col.tipo = 'C';
            // Extraer N de VARCHAR(N)
            size_t pa = tipo_upper.find('(');
            size_t pc = tipo_upper.find(')');
            if (pa != std::string::npos && pc != std::string::npos && pc > pa) {
                try {
                    col.tamano_bytes = std::stoi(tipo_upper.substr(pa + 1, pc - pa - 1)) + 1;
                } catch (...) {
                    col.tamano_bytes = 51; // default VARCHAR(50)
                }
            } else {
                col.tamano_bytes = 256; // TEXT sin tamaño
            }
        } else {
            // Tipo desconocido → tratar como string de 50 bytes
            col.tipo         = 'C';
            col.tamano_bytes = 51;
        }

        res.esquema.columnas.push_back(col);
    }

    if (res.esquema.columnas.empty()) {
        res.error = "No se encontraron columnas validas en el CREATE TABLE.";
        return res;
    }

    return res;
}

// ============================================================
//  PARSEO DE CSV (solo valores, esquema ya viene del TXT)
// ============================================================

static std::vector<std::string> parsear_linea_csv(const std::string& linea) {
    std::vector<std::string> campos;
    std::string campo;
    bool en_comillas = false;
    for (int i = 0; i < (int)linea.size(); ++i) {
        char c = linea[i];
        if (c == '"') {
            if (en_comillas && i + 1 < (int)linea.size() && linea[i+1] == '"') {
                campo += '"'; ++i;
            } else {
                en_comillas = !en_comillas;
            }
        } else if (c == ',' && !en_comillas) {
            campos.push_back(campo); campo.clear();
        } else {
            campo += c;
        }
    }
    campos.push_back(campo);
    return campos;
}

struct ResultadoCSV {
    std::vector<std::vector<std::string>> filas;
    std::string error;
};

ResultadoCSV cargar_csv(const std::string& ruta) {
    ResultadoCSV res;
    std::ifstream archivo(ruta);
    if (!archivo.is_open()) {
        res.error = "No se pudo abrir: " + ruta;
        return res;
    }

    std::string linea;
    // Saltar cabecera si existe (primera linea)
    if (!std::getline(archivo, linea)) {
        res.error = "Archivo CSV vacio.";
        return res;
    }
    // Quitar BOM
    if (linea.size() >= 3 && (unsigned char)linea[0] == 0xEF && (unsigned char)linea[1] == 0xBB && (unsigned char)linea[2] == 0xBF)
        linea = linea.substr(3);

    // Detectar si la primera linea es cabecera (tiene texto no numerico)
    // Si es cabecera, ya la saltamos. Si son datos, la procesamos.
    auto primera = parsear_linea_csv(linea);
    bool es_cabecera = false;
    for (const auto& campo : primera) {
        std::string t = trim(campo);
        // Si algun campo no es numero ni bool, es cabecera
        bool es_num = !t.empty();
        for (char ch : t)
            if (!std::isdigit((unsigned char)ch) && ch != '.' && ch != '-' && ch != '+')
                { es_num = false; break; }
        if (!es_num && to_upper(t) != "TRUE" && to_upper(t) != "FALSE" &&
            to_upper(t) != "SI" && to_upper(t) != "NO" &&
            to_upper(t) != "1" && to_upper(t) != "0") {
            es_cabecera = true; break;
        }
    }
    if (!es_cabecera)
        res.filas.push_back(primera); // era dato, no cabecera

    while (std::getline(archivo, linea)) {
        if (trim(linea).empty()) continue;
        res.filas.push_back(parsear_linea_csv(linea));
    }

    if (res.filas.empty())
        res.error = "El CSV no tiene filas de datos.";

    return res;
}
