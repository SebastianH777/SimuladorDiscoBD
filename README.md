# SimuladorDiscoBD — Fork con Avance 3

**Fork de:** [Klins18/SimuladorDiscoBD](https://github.com/Klins18/SimuladorDiscoBD)

Simulador de almacenamiento físico de base de datos en disco magnético, con fragmentación de registros, escritura binaria por tipo de dato, índice de búsqueda y exportación CSV.

---

## Cambios respecto al repositorio original (Avance 3)

El proyecto original (Avance 2) permitía configurar el disco e ingresar registros como texto libre, mostrando los bytes en bruto en una tabla básica.

Este fork agrega las siguientes funcionalidades:

### Nuevos archivos

**`Esquema.h`**
- Parser de archivos `.txt` con sintaxis `CREATE TABLE` estándar de SQL
- Soporte para los tipos `INT`, `FLOAT`, `BOOLEAN` y `VARCHAR(N)` / `CHAR` / `TEXT` / `DATE`
- Parser de archivos CSV con detección automática de cabecera
- Manejo de BOM UTF-8 y campos entre comillas

**`disco_binario.h`**
- Escritura binaria real por tipo de dato:
  - `INT` → 4 bytes
  - `FLOAT` → 4 bytes
  - `BOOLEAN` → 1 byte
  - `VARCHAR(N)` → N bytes con padding `\0`
- `EscritorSectores`: fragmenta automáticamente el registro entre sectores cuando no hay espacio suficiente; los campos atómicos (INT, FLOAT, BOOL) no se parten entre sectores
- `LectorSectores`: reconstruye el registro siguiendo la cadena de sectores enlazados
- Función `exportar_disco_a_csv()`: recorre el disco y exporta todos los registros no-continuación a un archivo CSV

**`Busqueda.h`**
- Índice por columna con tipo nativo: `map<int,…>` para INT, `map<double,…>` para FLOAT, `map<string,…>` para VARCHAR/BOOL
- `construir_indice()`: una sola pasada por el disco
- `buscar_exacto()`: O(log n)
- `buscar_rango()`: O(log n + k resultados) usando `lower_bound` / `upper_bound`

**`datos.csv` / `esquema.txt`**
- Archivos de ejemplo incluidos para prueba inmediata

### Cambios en `SimuladorDiscoBD.cpp`

El `main()` fue reescrito para incorporar 8 paneles ImGui:

| Panel | Función |
|-------|---------|
| 1. Configuración del Disco | Parámetros + barra de progreso de sectores usados |
| 2. Esquema SQL (.txt) | Carga y visualiza las columnas del `CREATE TABLE` |
| 3. Datos CSV | Importación masiva + construcción automática del índice |
| 4. Exportar a CSV | Exporta todos los registros del disco a un archivo |
| 5. Búsqueda | Búsqueda exacta o por rango en cualquier columna |
| 6. Estado | Barra de estado con mensajes de éxito/error |
| 7. Estado del Disco | Tabla de sectores con columnas Cont. y Sig. (cadena de fragmentos) |
| 8. Resultados de Búsqueda | Tabla con todos los campos + dirección física; columna buscada resaltada en amarillo |

La ventana pasó de 800×600 a 1400×800 para acomodar los nuevos paneles.

---

## Requisitos previos

- Windows 10/11 de 64 bits
- Git instalado: https://git-scm.com/download/win
- Visual Studio 2022 Community: https://visualstudio.microsoft.com/es/downloads/
  - Durante la instalación marcar: **Desarrollo para escritorio con C++**

---

## Instalación de vcpkg y dependencias

Abre **Developer Command Prompt for VS 2022** como administrador y ejecuta uno por uno:

```
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
bootstrap-vcpkg.bat
vcpkg integrate install
```

### Instalar SFML e ImGui

```
vcpkg install sfml:x64-windows
vcpkg install imgui-sfml:x64-windows
```

Esto puede tardar varios minutos.

---

## Cómo abrir el proyecto

1. Clona este repositorio:
   ```
   git clone https://github.com/TU_USUARIO/SimuladorDiscoBD.git
   ```
2. Abre Visual Studio 2022
3. Archivo → Abrir → Proyecto/Solución
4. Selecciona `SimuladorDiscoBD.vcxproj`
5. Compila y ejecuta con `Ctrl + F5`

### Archivos de ejemplo incluidos

- `esquema.txt` — define la tabla `estudiante` con 8 columnas
- `datos.csv` — 5 registros de ejemplo

Al iniciar, carga primero el esquema y luego el CSV. El índice se construye automáticamente al importar.

---

## Funcionalidades

- Configuración del disco (platos, pistas, sectores, capacidad por sector)
- Inicialización dinámica en memoria heap
- Escritura binaria de registros con fragmentación automática entre sectores
- Lectura binaria y reconstrucción de registros
- Importación desde CSV con detección de cabecera
- Exportación del disco a CSV
- Índice tipado por columna
- Búsqueda exacta y por rango con O(log n)
- Visualización del estado del disco con cadena de fragmentos

---

## Integrantes

- Klinsman
- Fiorella
- Marcelo
- Sebastian
