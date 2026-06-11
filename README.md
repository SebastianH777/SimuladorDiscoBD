# SimuladorDiscoBD
# Simulador de Almacenamiento Físico de Base de Datos

Proyecto universitario que simula el almacenamiento físico de una base de datos en disco magnético, con fragmentación de registros y búsqueda de datos.

## Requisitos previos
- Windows 10/11 de 64 bits
- Git instalado (https://git-scm.com/download/win)
- Visual Studio 2022 Community (https://visualstudio.microsoft.com/es/downloads/)
  - Durante la instalación marcar: **Desarrollo para escritorio con C++**

## Instalación de vcpkg y dependencias

Abre **Developer Command Prompt for VS 2022** como administrador y ejecuta uno por uno:
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
bootstrap-vcpkg.bat
vcpkg integrate install
vcpkg install sfml:x64-windows
vcpkg install imgui-sfml:x64-windows

Esto puede tardar varios minutos.

## Cómo abrir el proyecto

1. Clona el repositorio: git clone https://github.com/Klins18/SimuladorDiscoBD.git
2. Abre Visual Studio 2022
3. Click en **Crear un proyecto nuevo**
4. Selecciona **Aplicación de consola (C++)**
5. En ubicación pon la carpeta donde clonaste el repo
6. Nombre del proyecto: **SimuladorDiscoBD**
7. Una vez creado, reemplaza el `main.cpp` con los archivos del repositorio:
   - `Disco.h` → Archivos de encabezado
   - `Disco_funciones.h` → Archivos de encabezado
   - `Disco.cpp` → Archivos de origen
   - `SimuladorDiscoBD.cpp` → reemplaza el main.cpp existente
8. Compila y ejecuta con **Ctrl + F5**

## Funcionalidades actuales (Avance 2)
- Configuración del disco (platos, pistas, sectores, capacidad)
- Inicialización dinámica del disco en memoria heap
- Escritura de registros con fragmentación automática
- Visualización de sectores ocupados en tabla

## Integrantes
- Klinsman
- Fiorella 
- Marcelo
- Sebastian
